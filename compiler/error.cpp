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

static void splitLines(const SourcePtr &source, vector<string> &lines) {
    lines.clear();
    if (!source || source->data() == source->endData()) {
        return;
    }
    const std::string_view sourceView(source->data(),
                                      source->endData() - source->data());
    lines.emplace_back();

    for (const char c : sourceView) {
        lines.back().push_back(c);
        if (c == '\n')
            lines.emplace_back();
    }
}

static bool endsWithNewline(llvm::StringRef s) {
    if (s.size() == 0)
        return false;
    return s[s.size() - 1] == '\n';
}

static void displayLocation(Location const &location, unsigned &line,
                            unsigned &column) {
    unsigned tabColumn;
    getLineCol(location, line, column, tabColumn);
    vector<string> lines;
    splitLines(location.source, lines);
    llvm::errs() << "###############################\n";
    unsigned i = (line < 2) ? 0 : line - 2;
    for (; i <= line + 2; ++i) {
        if (i >= lines.size())
            continue;
        llvm::errs() << lines[i];
        if (!endsWithNewline(lines[i]))
            llvm::errs() << "\n";
        if (i == line) {
            for (unsigned j = 0; j < tabColumn; ++j)
                llvm::errs() << "-";
            llvm::errs() << "^\n";
        }
    }
    llvm::errs() << "###############################\n";
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

static Severity severityFromKind(llvm::StringRef kind) {
    if (kind == "warning")
        return Severity::Warning;
    if (kind == "note")
        return Severity::Note;
    if (kind == "help")
        return Severity::Help;
    return Severity::Error;
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

static void appendContextNotes(Diagnostic &diag, Location primaryLocation) {
    for (size_t i = 0; i < contextStack.size(); ++i) {
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

void displayError(llvm::Twine const &msg, llvm::StringRef kind) {
    Location location = topLocation();
    Span span = topSpan();
    if (!span.ok())
        span = Span(location);
    Diagnostic diag(severityFromKind(kind), stripTrailingNewline(msg), span);
    appendContextNotes(diag, location);
    displayDiagnostic(diag);
}

void warning(llvm::Twine const &msg) { displayError(msg, "warning"); }

void note(llvm::Twine const &msg) { displayError(msg, "note"); }

void error(llvm::Twine const &msg) {
    displayError(msg, "error");
    throw CompilerError();
}

void error(Location const &location, llvm::Twine const &msg) {
    if (location.ok())
        pushLocation(location);
    error(msg);
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

void fmtError(const char *fmt, ...) {
    va_list ap;
    char s[256];
    va_start(ap, fmt);
    vsnprintf(s, sizeof(s) - 1, fmt, ap);
    va_end(ap);
    error(s);
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

void typeError(const llvm::StringRef expected, const TypePtr &receivedType) {
    error(typeErrorMessage(expected, receivedType));
}

void typeError(const TypePtr &expectedType, const TypePtr &receivedType) {
    error(typeErrorMessage(expectedType, receivedType));
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
    sout << location.source->fileName.c_str() << "(" << line + 1 << ","
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

void matchFailureError(MatchFailureError const &err) {
    string buf;
    {
        llvm::raw_string_ostream sout(buf);
        if (err.failedInterface)
            sout << "call does not conform to function interface";
        else if (err.ambiguousMatch)
            sout << "call matches ambiguous overloads";
        else {
            sout << "no matching overload found";
            // Append the callable + arg types from the deepest context frame,
            // e.g. "no matching overload found for +(Foo, Foo)"
            if (!contextStack.empty()) {
                const auto &top = contextStack.back();
                sout << " for ";
                printName(sout, top.callable);
                if (top.hasParams) {
                    sout << "(";
                    printNameList(sout, top.params, top.dispatchIndices);
                    sout << ")";
                }
            }
        }
        sout.flush();
    }

    matchFailureMessage(err, buf);

    // Pick the deepest blameable frame.
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (!it->location.ok() || !it->location.source)
            continue;
        if (it->overload.ptr() == nullptr)
            continue;
        if (it->overload->isDiagnosticTransparent)
            continue;
        pushLocation(it->location);
        break;
    }

    error(buf);
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
    out << location.source->fileName << "(" << line + 1 << "," << column << ")";
}
} // namespace ceramic
