#pragma once

#include "ceramic.hpp"
#include "diagnostic.hpp"
#include "printer.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define CERAMIC_NORETURN __attribute__((noreturn))
#endif

#ifndef CERAMIC_NORETURN
#define CERAMIC_NORETURN
#endif

namespace ceramic {
struct CompileContextEntry {
    vector<ObjectPtr> params;
    vector<unsigned> dispatchIndices;

    Location location;

    ObjectPtr callable;
    OverloadPtr overload;

    bool hasParams : 1;

    CompileContextEntry(ObjectPtr callable)
        : callable(callable), hasParams(false) {}

    CompileContextEntry(ObjectPtr callable, llvm::ArrayRef<ObjectPtr> params)
        : params(params), callable(callable), hasParams(!params.empty()) {}

    CompileContextEntry(ObjectPtr callable, llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices)
        : params(params), dispatchIndices(dispatchIndices), callable(callable),
          hasParams(!params.empty()) {}
};

void pushCompileContext(const ObjectPtr &obj);

void pushCompileContext(const ObjectPtr &obj, llvm::ArrayRef<ObjectPtr> params);

void pushCompileContext(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices);

void popCompileContext();

void setCurrentOverload(OverloadPtr overload);

vector<CompileContextEntry> getCompileContext();

void setCompileContext(llvm::ArrayRef<CompileContextEntry> x);

struct PVData;

struct CompileContextPusher {
    CompileContextPusher(const ObjectPtr &obj) { pushCompileContext(obj); }

    CompileContextPusher(const ObjectPtr &obj,
                         llvm::ArrayRef<ObjectPtr> params) {
        pushCompileContext(obj, params);
    }

    CompileContextPusher(
        ObjectPtr obj, llvm::ArrayRef<PVData> params,
        llvm::ArrayRef<unsigned> dispatchIndices = llvm::ArrayRef<unsigned>());

    ~CompileContextPusher() { popCompileContext(); }
};

void pushLocation(Location const &location);

void popLocation();

Location topLocation();

struct LocationContext {
    Location loc;

    LocationContext(Location const &loc) : loc(loc) {
        if (loc.ok())
            pushLocation(loc);
    }

    ~LocationContext() {
        if (loc.ok())
            popLocation();
    }

  private:
    LocationContext(const LocationContext &) {}

    void operator=(const LocationContext &) {}
};

void getLineCol(Location const &location, unsigned &line, unsigned &column,
                unsigned &tabColumn);

llvm::DIFile *getDebugLineCol(Location const &location, unsigned &line,
                              unsigned &column);

void printFileLineCol(llvm::raw_ostream &out, Location const &location);

void invalidStaticObjectError(const ObjectPtr &obj);

void argumentInvalidStaticObjectError(unsigned index, const ObjectPtr &obj);

struct DebugPrinter {
    static int indent;
    const ObjectPtr obj;

    DebugPrinter(ObjectPtr obj);

    ~DebugPrinter();
};

extern "C" void displayCompileContext();

class DiagBuilder {
  public:
    // REQUIRED. headline: the "error: <headline>" first line
    explicit DiagBuilder(llvm::Twine const &headline,
                         Severity severity = Severity::Error);

    // OPTIONAL. where the caret snippet points. without it, emit() falls
    // back to the location the compiler is currently processing
    DiagBuilder &at(Span span);
    // same, and drops the "while compiling" note that would repeat it
    DiagBuilder &at(Location const &location);

    // OPTIONAL. short text rendered right after the caret: ^ <text>
    DiagBuilder &label(llvm::Twine const &text);

    // OPTIONAL, repeatable. "note: <text>" snippet at another location
    DiagBuilder &note(Location const &location, llvm::Twine const &text);
    DiagBuilder &note(Span span, llvm::Twine const &text);

    // OPTIONAL. preformatted block between the caret snippet and the notes
    DiagBuilder &detail(string text);

    // OPTIONAL. closing "help: <text>" line
    DiagBuilder &help(llvm::Twine const &text);

    // OPTIONAL. suppress the automatic "while compiling ..." notes
    DiagBuilder &noContextNotes();

    // OPTIONAL. drop the innermost "while compiling" note, for errors
    // whose headline already names that call
    DiagBuilder &skipInnermostContextNote();

    // REQUIRED terminator. renders, then aborts compilation
    void emit() CERAMIC_NORETURN;
    // REQUIRED terminator for warnings and notes. renders without throwing
    void display();

  private:
    Diagnostic diag;
    Location skipLocation;
    bool explicitSpan = false;
    bool explicitSkip = false;
    bool contextNotes = true;
    bool innermostContextNote = true;

    void finish();
};

// caret falls back to the current compile context
void error(llvm::Twine const &msg) CERAMIC_NORETURN;
// point caret at one location
void error(Location const &location, llvm::Twine const &msg) CERAMIC_NORETURN;
// wide caret across a span
void error(Span span, llvm::Twine const &msg) CERAMIC_NORETURN;
// caret spans the whole expression node
void error(Expr const *context, llvm::Twine const &msg) CERAMIC_NORETURN;
void error(Pointer<Expr> context, llvm::Twine const &msg) CERAMIC_NORETURN;

void warning(llvm::Twine const &msg);
void warning(Location const &location, llvm::Twine const &msg);
void warning(Span span, llvm::Twine const &msg);

template <class T> void warning(Pointer<T> context, llvm::Twine const &msg);

template <class T> void warning(Pointer<T> context, llvm::Twine const &msg) {
    warning(context->location, msg);
}

template <class T>
void error(Pointer<T> context, llvm::Twine const &msg) CERAMIC_NORETURN;

template <class T> void error(Pointer<T> context, llvm::Twine const &msg) {
    if (context->location.ok())
        pushLocation(context->location);
    error(msg);
}

template <class T>
void error(T const *context, llvm::Twine const &msg) CERAMIC_NORETURN;

template <class T> void error(T const *context, llvm::Twine const &msg) {
    error(context->location, msg);
}

void argumentError(size_t index, llvm::StringRef msg) CERAMIC_NORETURN;

template <typename T>
void argumentError(size_t index, llvm::StringRef msg,
                   const T &argument) CERAMIC_NORETURN;

template <typename T>
void argumentError(const size_t index, const llvm::StringRef msg,
                   const T &argument) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "argument " << (index + 1) << ": " << msg << ", actual "
         << argument;
    error(sout.str());
}

void arityError(size_t expected, size_t received) CERAMIC_NORETURN;
void arityError2(size_t minExpected, size_t received) CERAMIC_NORETURN;

template <class T>
void arityError(Pointer<T> context, size_t expected,
                size_t received) CERAMIC_NORETURN;

template <class T>
void arityError(Pointer<T> context, size_t expected, size_t received) {
    if (context->location.ok())
        pushLocation(context->location);
    arityError(expected, received);
}

template <class T>
void arityError2(Pointer<T> context, size_t minExpected,
                 size_t received) CERAMIC_NORETURN;

template <class T>
void arityError2(Pointer<T> context, size_t minExpected, size_t received) {
    if (context->location.ok())
        pushLocation(context->location);
    arityError2(minExpected, received);
}

void ensureArity(const MultiStaticPtr &args, size_t size);
void ensureArity(const MultiEValuePtr &args, size_t size);
void ensureArity(const MultiPValuePtr &args, size_t size);
void ensureArity(const MultiCValuePtr &args, size_t size);

template <class T> void ensureArity(T const &args, size_t size) {
    if (args.size() != size)
        arityError(size, args.size());
}

template <class T>
void ensureArity2(T const &args, size_t size, bool hasVarArgs) {
    if (!hasVarArgs)
        ensureArity(args, size);
    else if (args.size() < size)
        arityError2(size, args.size());
}

void arityMismatchError(size_t leftArity, size_t rightArity,
                        bool hasVarArg) CERAMIC_NORETURN;

void returnArityError(Statement const *stmt, size_t expected,
                      size_t received) CERAMIC_NORETURN;

void typeError(llvm::StringRef expected,
               const TypePtr &receivedType) CERAMIC_NORETURN;
void typeError(const TypePtr &expectedType,
               const TypePtr &receivedType) CERAMIC_NORETURN;

void argumentTypeError(unsigned index, llvm::StringRef expected,
                       const TypePtr &receivedType) CERAMIC_NORETURN;

void argumentTypeError(unsigned index, const TypePtr &expectedType,
                       const TypePtr &receivedType) CERAMIC_NORETURN;

void indexRangeError(llvm::StringRef kind, size_t value,
                     size_t maxValue) CERAMIC_NORETURN;

void argumentIndexRangeError(unsigned index, llvm::StringRef kind, size_t value,
                             size_t maxValue) CERAMIC_NORETURN;

extern bool shouldPrintFullMatchErrors;
extern set<pair<string, string>> logMatchSymbols;

void unboundPatternVarError(IdentifierPtr const &name) CERAMIC_NORETURN;
void unboundPatternVarError(IdentifierPtr const &name, ObjectPtr callable,
                            OverloadPtr overload) CERAMIC_NORETURN;

void matchBindingError(MatchResultPtr const &result) CERAMIC_NORETURN;
void matchFailureLog(MatchFailureError const &err);
void matchFailureError(MatchFailureError const &err) CERAMIC_NORETURN;

class CompilerError : std::exception {};
} // namespace ceramic
