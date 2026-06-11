#pragma once

#include "ceramic.hpp"
#include "printer.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define CERAMIC_NORETURN __attribute__((noreturn))
#endif

#ifndef CERAMIC_NORETURN
#define CERAMIC_NORETURN
#endif

namespace ceramic {
void error(llvm::Twine const &msg) CERAMIC_NORETURN;
void error(Location const &location, llvm::Twine const &msg) CERAMIC_NORETURN;
void error(Expr const *context, llvm::Twine const &msg) CERAMIC_NORETURN;
void error(Pointer<Expr> context, llvm::Twine const &msg) CERAMIC_NORETURN;

void warning(llvm::Twine const &msg);

void fmtError(const char *fmt, ...) CERAMIC_NORETURN;

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
