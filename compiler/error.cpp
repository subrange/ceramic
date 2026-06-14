#include "error.hpp"
#include "ceramic.hpp"
#include "codegen.hpp"
#include "diagnostic.hpp"
#include "evaluator.hpp"
#include "invoketables.hpp"
#include "matchinvoke.hpp"
#include "printer.hpp"

#include <algorithm>
#include <cstdarg>

namespace ceramic {
bool shouldPrintFullMatchErrors;
set<pair<string, string>> logMatchSymbols;

//
// invoke stack - a compilation call stack
//

static vector<CompileContextEntry> contextStack;

static constexpr unsigned RECURSION_WARNING_LEVEL = 1000;

void pushCompileContext(const ObjectPtr &obj) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.emplace_back(obj);
}

void pushCompileContext(const ObjectPtr &obj,
                        const llvm::ArrayRef<ObjectPtr> params) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.emplace_back(obj, params);
}

void pushCompileContext(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices) {
    if (contextStack.size() >= RECURSION_WARNING_LEVEL)
        warning("potential runaway recursion");
    if (!contextStack.empty())
        contextStack.back().location = topLocation();
    contextStack.emplace_back(obj, params, dispatchIndices);
}

void popCompileContext() { contextStack.pop_back(); }

void setCurrentOverload(OverloadPtr overload) {
    if (!contextStack.empty())
        contextStack.back().overload = overload;
}

vector<CompileContextEntry> getCompileContext() { return contextStack; }

void setCompileContext(llvm::ArrayRef<CompileContextEntry> x) {
    contextStack = x;
}

CompileContextPusher::CompileContextPusher(
    ObjectPtr obj, llvm::ArrayRef<PVData> params,
    llvm::ArrayRef<unsigned> dispatchIndices) {
    vector<ObjectPtr> params2;
    for (const auto &param : params) {
        params2.emplace_back(param.type.ptr());
    }
    pushCompileContext(obj, params2, dispatchIndices);
}

//
// source location of the current item being processed
//

static vector<Location> errorLocations;
static vector<Span> errorSpans;

void pushLocation(Location const &location) {
    errorLocations.push_back(location);
}

void popLocation() { errorLocations.pop_back(); }

Location topLocation() {
    auto i = errorLocations.end();
    auto begin = errorLocations.begin();
    while (i != begin) {
        --i;
        if (i->ok())
            return *i;
    }
    return {};
}

static Span topSpan() {
    for (auto i = errorSpans.rbegin(); i != errorSpans.rend(); ++i) {
        if (i->ok())
            return *i;
    }
    return {};
}

namespace {
struct SpanHint {
    bool active;
    SpanHint(Span const &s) : active(s.ok()) {
        if (active)
            errorSpans.push_back(s);
    }
    ~SpanHint() {
        if (active)
            errorSpans.pop_back();
    }
};
} // namespace

//
// DebugPrinter
//

static vector<ObjectPtr> debugStack;

int DebugPrinter::indent = 0;

DebugPrinter::DebugPrinter(ObjectPtr obj) : obj(obj) {
    for (int i = 0; i < indent; ++i)
        llvm::outs() << ' ';
    llvm::outs() << "BEGIN - " << obj << '\n';
    ++indent;
    debugStack.push_back(obj);
}

DebugPrinter::~DebugPrinter() {
    debugStack.pop_back();
    --indent;
    for (int i = 0; i < indent; ++i)
        llvm::outs() << ' ';
    llvm::outs() << "DONE - " << obj << '\n';
}

//
// report error
//

static void computeLineCol(Location const &location, unsigned &line,
                           unsigned &column, unsigned &tabColumn) {
    const char *p = location.source->data();
    const char *end = p + location.offset;
    line = column = tabColumn = 0;
    for (; p != end; ++p) {
        ++column;
        ++tabColumn;
        if (*p == '\n') {
            ++line;
            column = 0;
            tabColumn = 0;
        } else if (*p == '\t') {
            tabColumn += 7;
        }
    }
}

llvm::DIFile *getDebugLineCol(Location const &location, unsigned &line,
                              unsigned &column) {
    if (!location.ok()) {
        line = 0;
        column = 0;
        return nullptr;
    }

    unsigned tabColumn;
    computeLineCol(location, line, column, tabColumn);
    line += 1;
    column += 1;
    return location.source->getDebugInfo();
}

void getLineCol(Location const &location, unsigned &line, unsigned &column,
                unsigned &tabColumn) {
    if (!location.ok()) {
        line = 0;
        column = 0;
        tabColumn = 0;
        return;
    }

    computeLineCol(location, line, column, tabColumn);
}

// This has to use stdio because it needs to be usable from the debugger
// and cerr or errs may be destroyed if there's a bug in global dtors
extern "C" void displayCompileContext() {
    if (contextStack.empty())
        return;
    fprintf(stderr, "\ncompilation context: \n");
    string buf;
    llvm::raw_string_ostream errs(buf);
    for (size_t i = contextStack.size(); i > 0; --i) {
        ObjectPtr obj = contextStack[i - 1].callable;
        llvm::ArrayRef<ObjectPtr> params = contextStack[i - 1].params;

        if (i < contextStack.size() && contextStack[i - 1].location.ok()) {
            errs << "  ";
            printFileLineCol(errs, contextStack[i - 1].location);
            errs << ":\n";
        }

        errs << "    ";
        if (obj->objKind == GLOBAL_VARIABLE) {
            errs << "global ";
            printName(errs, obj);
            if (!params.empty()) {
                errs << "[";
                printNameList(errs, params);
                errs << "]";
            }
        } else {
            printName(errs, obj);
            if (contextStack[i - 1].hasParams) {
                errs << "(";
                printNameList(errs, params,
                              contextStack[i - 1].dispatchIndices);
                errs << ")";
            }
        }
        errs << "\n";
    }
    fprintf(stderr, "%s", errs.str().c_str());
    fflush(stderr);
}

static void displayDebugStack() {
    if (debugStack.empty())
        return;
    llvm::errs() << "\ndebug stack:\n";
    for (size_t i = debugStack.size(); i > 0; --i) {
        llvm::errs() << "  " << debugStack[i - 1]->toString() << "\n";
    }
}

static string stripTrailingNewline(llvm::Twine const &msg) {
    string s = msg.str();
    if (!s.empty() && s.back() == '\n')
        s.pop_back();
    return s;
}

static string compileFrameHeadline(CompileContextEntry const &frame) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "while compiling ";
    ObjectPtr obj = frame.callable;
    if (obj->objKind == GLOBAL_VARIABLE) {
        sout << "global ";
        printName(sout, obj);
        if (!frame.params.empty()) {
            sout << "[";
            printNameList(sout, llvm::ArrayRef<ObjectPtr>(frame.params));
            sout << "]";
        }
    } else {
        printName(sout, obj);
        if (frame.hasParams) {
            sout << "(";
            printNameList(sout, llvm::ArrayRef<ObjectPtr>(frame.params),
                          llvm::ArrayRef<unsigned>(frame.dispatchIndices));
            sout << ")";
        }
    }
    sout.flush();
    return buf;
}

static void appendContextNotes(Diagnostic &diag, Location primaryLocation,
                               bool skipInnermost) {
    size_t end = contextStack.size();
    if (skipInnermost && end > 0)
        --end;
    for (size_t i = 0; i < end; ++i) {
        CompileContextEntry const &frame = contextStack[i];
        if (!frame.location.ok())
            continue;
        if (frame.overload.ptr() != nullptr &&
            frame.overload->isDiagnosticTransparent)
            continue;
        if (primaryLocation.ok() &&
            frame.location.source == primaryLocation.source &&
            frame.location.offset == primaryLocation.offset)
            continue;
        diag.notes.emplace_back(Severity::Note, compileFrameHeadline(frame),
                                Span(frame.location));
    }
}

static Span currentSpan() {
    Span span = topSpan();
    if (!span.ok())
        span = Span(topLocation());
    return span;
}

DiagBuilder::DiagBuilder(llvm::Twine const &headline, Severity severity) {
    diag.severity = severity;
    diag.headline = stripTrailingNewline(headline);
}

DiagBuilder &DiagBuilder::at(Span span) {
    diag.primary = span;
    explicitSpan = true;
    return *this;
}

DiagBuilder &DiagBuilder::at(Location const &location) {
    at(Span(location));
    skipLocation = location;
    explicitSkip = true;
    return *this;
}

DiagBuilder &DiagBuilder::label(llvm::Twine const &text) {
    diag.primaryLabel = text.str();
    return *this;
}

DiagBuilder &DiagBuilder::note(Span span, llvm::Twine const &text) {
    diag.notes.emplace_back(Severity::Note, text.str(), span);
    return *this;
}

DiagBuilder &DiagBuilder::note(Location const &location,
                               llvm::Twine const &text) {
    return note(Span(location), text);
}

DiagBuilder &DiagBuilder::detail(string text) {
    diag.detail = std::move(text);
    return *this;
}

DiagBuilder &DiagBuilder::help(llvm::Twine const &text) {
    diag.suggestion = text.str();
    return *this;
}

DiagBuilder &DiagBuilder::noContextNotes() {
    contextNotes = false;
    return *this;
}

DiagBuilder &DiagBuilder::skipInnermostContextNote() {
    innermostContextNote = false;
    return *this;
}

void DiagBuilder::finish() {
    // resolve fallbacks late so location pushes made between construction
    // and emit still count
    if (!explicitSpan)
        diag.primary = currentSpan();
    if (contextNotes)
        appendContextNotes(diag, explicitSkip ? skipLocation : topLocation(),
                           !innermostContextNote);
    displayDiagnostic(diag);
}

void DiagBuilder::display() { finish(); }

void DiagBuilder::emit() {
    finish();
    throw CompilerError();
}

void warning(llvm::Twine const &msg) {
    DiagBuilder(msg, Severity::Warning).display();
}

void note(llvm::Twine const &msg) {
    DiagBuilder(msg, Severity::Note).display();
}

void error(llvm::Twine const &msg) { DiagBuilder(msg).emit(); }

void error(Location const &location, llvm::Twine const &msg) {
    if (location.ok())
        pushLocation(location);
    error(msg);
}

void error(Span span, llvm::Twine const &msg) {
    DiagBuilder(msg).at(span).emit();
}

static Span exprSpan(Expr const *e) {
    if (e == nullptr)
        return {};
    if (e->startLocation.ok() && e->endLocation.ok() &&
        e->startLocation.source == e->endLocation.source)
        return Span(e->startLocation.source, e->startLocation.offset,
                    e->endLocation.offset);
    return {};
}

void error(Expr const *context, llvm::Twine const &msg) {
    SpanHint hint(exprSpan(context));
    if (context != nullptr && context->location.ok())
        pushLocation(context->location);
    error(msg);
}

void error(Pointer<Expr> context, llvm::Twine const &msg) {
    error(context.ptr(), msg);
}

void argumentError(size_t index, llvm::StringRef msg) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "argument " << (index + 1) << ": " << msg;
    error(sout.str());
}

static const char *valuesStr(size_t n) { return (n == 1) ? "value" : "values"; }

void arityError(size_t expected, size_t received) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected " << expected << " " << valuesStr(expected);
    sout << ", found " << received;
    error(sout.str());
}

void arityError2(size_t minExpected, size_t received) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected at least " << minExpected << " "
         << valuesStr(minExpected);
    sout << ", found " << received;
    error(sout.str());
}

void ensureArity(const MultiStaticPtr &args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(const MultiEValuePtr &args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(const MultiPValuePtr &args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void ensureArity(const MultiCValuePtr &args, size_t size) {
    if (args->size() != size)
        arityError(size, args->size());
}

void arityMismatchError(size_t leftArity, size_t rightArity, bool hasVarArg) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    if (hasVarArg)
        sout << "left side takes " << leftArity << " or more "
             << valuesStr(leftArity);
    else
        sout << "left side has " << leftArity << " " << valuesStr(leftArity);
    sout << ", right side has " << rightArity << " " << valuesStr(rightArity);
    error(sout.str());
}

// the function whose body is currently being compiled, if its declaration is
// known and useful as a secondary diagnostic span
static ExternalProcedurePtr enclosingExternalProcedure() {
    for (auto i = contextStack.rbegin(); i != contextStack.rend(); ++i) {
        ObjectPtr obj = i->callable;
        if (obj.ptr() != nullptr && obj->objKind == EXTERNAL_PROCEDURE)
            return (ExternalProcedure *)obj.ptr();
    }
    return nullptr;
}

void returnArityError(Statement const *stmt, size_t expected, size_t received) {
    string label;
    llvm::raw_string_ostream lout(label);
    lout << "have " << received << " " << valuesStr(received) << ", want "
         << expected;

    DiagBuilder bld(received > expected ? "too many return values"
                                        : "not enough return values");
    bld.at(stmt != nullptr ? Span(stmt->location) : topSpan())
        .label(lout.str())
        .noContextNotes();

    // point at a declaration that explains why no value was expected
    ExternalProcedurePtr proc = enclosingExternalProcedure();
    if (expected == 0 && proc.ptr() != nullptr && !proc->returnType &&
        proc->name.ptr() != nullptr && proc->location.ok())
        bld.note(proc->location,
                 "'" + proc->name->str + "' is declared with no return type");
    bld.emit();
}

static string typeErrorMessage(llvm::StringRef expected,
                               const TypePtr &receivedType) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "expected " << expected << ", found " << receivedType->toString();
    return sout.str();
}

static string typeErrorMessage(const TypePtr &expectedType,
                               const TypePtr &receivedType) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << expectedType->toString() << " type";
    return typeErrorMessage(sout.str(), receivedType);
}

// carry the "expected X, found Y" on the caret line when a location is known,
// otherwise keep it as the headline so it is never lost.
static CERAMIC_NORETURN void emitTypeError(string const &expectedFound) {
    Span span = currentSpan();
    DiagBuilder bld(span.ok() ? "type mismatch" : expectedFound.c_str());
    bld.at(span);
    if (span.ok())
        bld.label(expectedFound);
    bld.emit();
}

void typeError(const llvm::StringRef expected, const TypePtr &receivedType) {
    emitTypeError(typeErrorMessage(expected, receivedType));
}

void typeError(const TypePtr &expectedType, const TypePtr &receivedType) {
    emitTypeError(typeErrorMessage(expectedType, receivedType));
}

void argumentTypeError(const unsigned index, const llvm::StringRef expected,
                       const TypePtr &receivedType) {
    argumentError(index, typeErrorMessage(expected, receivedType));
}

void argumentTypeError(const unsigned index, const TypePtr &expectedType,
                       const TypePtr &receivedType) {
    argumentError(index, typeErrorMessage(expectedType, receivedType));
}

void indexRangeError(const llvm::StringRef kind, const size_t value,
                     const size_t maxValue) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << kind << " " << value << " out of range, must be less than "
         << maxValue;
    error(sout.str());
}

void argumentIndexRangeError(const unsigned index, const llvm::StringRef kind,
                             const size_t value, const size_t maxValue) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << kind << " " << value << " out of range, must be less than "
         << maxValue;
    argumentError(index, sout.str());
}

void invalidStaticObjectError(const ObjectPtr &obj) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid static object: " << obj->toString();
    error(sout.str());
}

void argumentInvalidStaticObjectError(const unsigned index,
                                      const ObjectPtr &obj) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid static object: " << obj->toString();
    argumentError(index, sout.str());
}

// the primary span is the use site, since the declaration itself is fine
static CERAMIC_NORETURN void emitUnboundPatternVar(DiagBuilder &bld,
                                                   IdentifierPtr const &name,
                                                   llvm::StringRef declLabel) {
    if (name->location.ok())
        bld.note(name->location, "'" + name->str + "' " + declLabel);
    bld.emit();
}

static Span useSiteSpan(IdentifierPtr const &name) {
    Span span = currentSpan();
    if (!span.ok())
        span = Span(name->location);
    return span;
}

void unboundPatternVarError(IdentifierPtr const &name) {
    DiagBuilder bld("pattern variable '" + name->str + "' cannot be inferred");
    bld.at(useSiteSpan(name));
    emitUnboundPatternVar(bld, name, "declared here");
}

void unboundPatternVarError(IdentifierPtr const &name, ObjectPtr callable,
                            OverloadPtr overload) {
    if (!overload->isBuiltinConstructor || callable->objKind != RECORD_DECL)
        unboundPatternVarError(name);

    RecordDecl *record = (RecordDecl *)callable.ptr();
    string help;
    llvm::raw_string_ostream hout(help);
    hout << "write the parameters explicitly: '" << record->name->str << "[";
    for (size_t i = 0; i < record->params.size(); ++i) {
        if (i > 0)
            hout << ",";
        hout << record->params[i]->str;
    }
    if (record->varParam.ptr()) {
        if (!record->params.empty())
            hout << ",";
        hout << ".." << record->varParam->str;
    }
    hout << "](...)'";

    DiagBuilder bld("cannot infer record parameter '" + name->str +
                    "' in call to '" + record->name->str + "'");
    bld.at(useSiteSpan(name)).help(hout.str());
    emitUnboundPatternVar(bld, name, "is not determined by the field types");
}

void matchBindingError(MatchResultPtr const &result) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    printMatchError(sout, result);
    error(sout.str());
}

// Rank a match-failure kind by how informative it is to the user.
// Higher = more useful (predicate names, type-pattern mismatches).
// Lower = noise (this overload was just for an unrelated callable name).
static int failureScore(MatchCode code) {
    switch (code) {
    case MATCH_PREDICATE_ERROR:
        return 4;
    case MATCH_ARGUMENT_ERROR:
    case MATCH_MULTI_ARGUMENT_ERROR:
    case MATCH_BINDING_ERROR:
    case MATCH_MULTI_BINDING_ERROR:
        return 3;
    case MATCH_ARITY_ERROR:
        return 2;
    case MATCH_CALLABLE_ERROR:
        return 1;
    default:
        return 0;
    }
}

static void printFailureLine(llvm::raw_ostream &sout,
                             const pair<OverloadPtr, MatchResultPtr> &failure) {
    sout << "\n    ";
    Location location = failure.first->location;
    unsigned line, column, tabColumn;
    getLineCol(location, line, column, tabColumn);
    sout << displayPath(location.source->fileName) << "(" << line + 1 << ","
         << column << ")"
         << "\n        ";
    printMatchError(sout, failure.second);
}

static void matchFailureMessage(MatchFailureError const &err, string &outBuf) {
    llvm::raw_string_ostream sout(outBuf);

    // -full-match-errors preserves the original verbatim dump.
    if (shouldPrintFullMatchErrors) {
        for (const auto &failure : err.failures)
            printFailureLine(sout, failure);
        sout.flush();
        return;
    }

    // Default path: hide universal pattern overloads, then rank what's left.
    int hiddenPatternOverloads = 0;
    vector<pair<OverloadPtr, MatchResultPtr>> visible;
    for (const auto &failure : err.failures) {
        if (failure.first->nameIsPattern) {
            ++hiddenPatternOverloads;
            continue;
        }
        visible.push_back(failure);
    }

    std::stable_sort(visible.begin(), visible.end(),
                     [](const pair<OverloadPtr, MatchResultPtr> &a,
                        const pair<OverloadPtr, MatchResultPtr> &b) {
                         return failureScore(a.second->matchCode) >
                                failureScore(b.second->matchCode);
                     });

    const size_t MAX_SHOW = 5;
    size_t shown = std::min(visible.size(), MAX_SHOW);
    size_t lessSpecific = visible.size() - shown;

    for (size_t i = 0; i < shown; ++i)
        printFailureLine(sout, visible[i]);

    if (lessSpecific > 0) {
        sout << "\n    " << lessSpecific
             << " other less-specific overloads not shown";
        if (hiddenPatternOverloads > 0)
            sout << " (plus " << hiddenPatternOverloads
                 << " universal overloads)";
        sout << " (use -full-match-errors for all)";
    } else if (hiddenPatternOverloads > 0) {
        sout << "\n    " << hiddenPatternOverloads
             << " universal overloads not shown (use -full-match-errors for "
                "all)";
    }
    sout.flush();
}

// position of the first argument/binding that failed to match, used to break
// ties within a failure tier: a candidate that failed later matched more
// leading arguments and is the likelier near-miss.
static unsigned matchFirstFailIndex(const MatchResultPtr &r) {
    switch (r->matchCode) {
    case MATCH_ARGUMENT_ERROR:
        return ((MatchArgumentError *)r.ptr())->argIndex;
    case MATCH_BINDING_ERROR:
        return ((MatchBindingError *)r.ptr())->argIndex;
    case MATCH_MULTI_ARGUMENT_ERROR:
        return ((MatchMultiArgumentError *)r.ptr())->argIndex;
    case MATCH_MULTI_BINDING_ERROR:
        return ((MatchMultiBindingError *)r.ptr())->argIndex;
    default:
        return 0;
    }
}

// when exactly one concrete overload matched arity and failed on a single
// argument's type, the candidate list is noise: the call has one obvious
// intended overload. return that near-miss so the error renders as a plain
// type mismatch instead of an overload-resolution dump.
static MatchArgumentError *singleNearMiss(MatchFailureError const &err,
                                          OverloadPtr &candidate) {
    MatchArgumentError *near = nullptr;
    for (const auto &failure : err.failures) {
        if (failure.first->nameIsPattern)
            continue;
        if (failure.second->matchCode != MATCH_ARGUMENT_ERROR)
            return nullptr;
        if (near != nullptr)
            return nullptr;
        near = (MatchArgumentError *)failure.second.ptr();
        candidate = failure.first;
    }
    return near;
}

static void appendOverloadSignature(llvm::raw_ostream &os, llvm::StringRef name,
                                    OverloadPtr ov) {
    os << name << "(";
    CodePtr code = ov->code;
    for (size_t i = 0; i < code->formalArgs.size(); ++i) {
        if (i != 0)
            os << ", ";
        FormalArgPtr fa = code->formalArgs[i];
        if (fa->type.ptr() != nullptr)
            os << shortString(fa->type->asString());
        else
            os << "_";
        if (fa->varArg)
            os << "..";
    }
    os << ")";
}

// the compact "candidates:" block: each ranked overload as one line,
// "file:line  name(types)  <reason>"
static string buildMatchDetail(MatchFailureError const &err,
                               llvm::StringRef name) {
    string buf;
    llvm::raw_string_ostream sout(buf);

    if (shouldPrintFullMatchErrors) {
        for (const auto &failure : err.failures)
            printFailureLine(sout, failure);
        sout << "\n";
        sout.flush();
        return buf;
    }

    int hiddenPatternOverloads = 0;
    vector<pair<OverloadPtr, MatchResultPtr>> visible;
    for (const auto &failure : err.failures) {
        if (failure.first->nameIsPattern) {
            ++hiddenPatternOverloads;
            continue;
        }
        visible.push_back(failure);
    }

    std::stable_sort(visible.begin(), visible.end(),
                     [](const pair<OverloadPtr, MatchResultPtr> &a,
                        const pair<OverloadPtr, MatchResultPtr> &b) {
                         int sa = failureScore(a.second->matchCode);
                         int sb = failureScore(b.second->matchCode);
                         if (sa != sb)
                             return sa > sb;
                         return matchFirstFailIndex(a.second) >
                                matchFirstFailIndex(b.second);
                     });

    const size_t MAX_SHOW = 5;
    size_t shown = std::min(visible.size(), MAX_SHOW);
    size_t lessSpecific = visible.size() - shown;

    if (shown > 0)
        sout << "  candidates:";
    for (size_t i = 0; i < shown; ++i) {
        const auto &f = visible[i];
        Location loc = f.first->location;
        unsigned line, column, tabColumn;
        getLineCol(loc, line, column, tabColumn);
        sout << "\n    " << displayPath(loc.source->fileName) << ":" << line + 1
             << "  ";
        appendOverloadSignature(sout, name, f.first);
        sout << "  ";
        printMatchErrorCompact(sout, f.second);
    }

    if (lessSpecific > 0) {
        sout << "\n  " << lessSpecific << " less-specific not shown";
        if (hiddenPatternOverloads > 0)
            sout << " (plus " << hiddenPatternOverloads << " universal)";
        sout << " (-full-match-errors for all)";
    } else if (hiddenPatternOverloads > 0) {
        sout << "\n  " << hiddenPatternOverloads
             << " universal overloads not shown (-full-match-errors for all)";
    }
    sout << "\n";
    sout.flush();
    return buf;
}

static bool paramOfKind(ObjectPtr const &param, TypeKind kind) {
    return param.ptr() != nullptr && param->objKind == TYPE &&
           ((Type *)param.ptr())->typeKind == kind;
}

// suggest the sibling division operator across the int/float split
static string operatorHint(llvm::StringRef name) {
    if (contextStack.empty() || !contextStack.back().hasParams)
        return string();
    const vector<ObjectPtr> &params = contextStack.back().params;
    if (params.empty())
        return string();

    if (name == "/") {
        for (const auto &param : params)
            if (!paramOfKind(param, INTEGER_TYPE))
                return string();
        return "use '\\' for integer division";
    }
    if (name == "\\") {
        for (const auto &param : params)
            if (paramOfKind(param, FLOAT_TYPE))
                return "use '/' for floating point division";
    }
    return string();
}

void matchFailureError(MatchFailureError const &err) {
    string name;
    {
        llvm::raw_string_ostream nameOut(name);
        if (!contextStack.empty())
            printName(nameOut, contextStack.back().callable);
        nameOut.flush();
    }

    bool noMatch = !err.failedInterface && !err.ambiguousMatch;
    string headline;
    {
        llvm::raw_string_ostream sout(headline);
        if (err.failedInterface)
            sout << "call does not conform to function interface";
        else if (err.ambiguousMatch)
            sout << "call matches ambiguous overloads";
        else {
            sout << "no matching overload found";
            if (!contextStack.empty()) {
                const auto &top = contextStack.back();
                sout << " for " << name;
                if (top.hasParams) {
                    sout << "(";
                    printNameList(sout, top.params, top.dispatchIndices);
                    sout << ")";
                }
            }
        }
        sout.flush();
    }

    // Pick the deepest blameable frame.
    Location blame;
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (!it->location.ok() || !it->location.source)
            continue;
        if (it->overload.ptr() == nullptr)
            continue;
        if (it->overload->isDiagnosticTransparent)
            continue;
        blame = it->location;
        break;
    }

    OverloadPtr nearCandidate;
    MatchArgumentError *near =
        noMatch ? singleNearMiss(err, nearCandidate) : nullptr;
    if (near != nullptr) {
        string expectedFound;
        {
            llvm::raw_string_ostream sout(expectedFound);
            sout << "expected " << shortString(near->arg->type->asString())
                 << ", found ";
            printStaticName(sout, near->type.ptr());
            sout.flush();
        }
        LocationContext lc(blame);
        Span span = currentSpan();
        DiagBuilder bld(span.ok() ? "type mismatch" : expectedFound.c_str());
        bld.at(span);
        if (span.ok())
            bld.label(expectedFound);
        if (nearCandidate->location.ok() &&
            nearCandidate->location.source.ptr())
            bld.note(nearCandidate->location, "defined here");
        bld.skipInnermostContextNote();
        bld.emit();
    }

    string hint =
        noMatch && !shouldPrintFullMatchErrors ? operatorHint(name) : string();
    string detail =
        noMatch && hint.empty() ? buildMatchDetail(err, name) : string();

    LocationContext lc(blame);
    DiagBuilder bld(headline);
    bld.detail(std::move(detail));
    if (!hint.empty())
        bld.help(hint);
    if (noMatch)
        bld.skipInnermostContextNote();
    bld.emit();
}

void matchFailureLog(MatchFailureError const &err) {
    if (err.failures.empty())
        return;
    string buf = "matched";
    matchFailureMessage(err, buf);
    note(buf);
}

void printFileLineCol(llvm::raw_ostream &out, Location const &location) {
    unsigned line, column, tabColumn;
    getLineCol(location, line, column, tabColumn);
    out << displayPath(location.source->fileName) << "(" << line + 1 << ","
        << column << ")";
}
} // namespace ceramic
