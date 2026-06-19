# Compiler Interface

Primitives for querying compiler settings, attaching attributes to external functions, and hinting to the optimizer. None of these may be overloaded.

## Compiler Flags

### `ExceptionsEnabled?`

```ceramic
// If exceptions are enabled in this compilation unit:
alias ExceptionsEnabled? = true;
// Otherwise:
alias ExceptionsEnabled? = false;
```

A global alias that tells you whether exceptions are enabled for the current compilation. You can use this to write code that compiles and works correctly both with and without exception support.

### `Flag?`

```ceramic
[name when StringLiteral?(name)]
Flag?(#name) : Bool;
```

To check whether the build was invoked with a given compiler flag, use `Flag?`. It is `true` if `-D<name>` or `-D<name>=value` was passed. Useful for conditional compilation based on build configuration.

### `Flag`

```ceramic
[name when StringLiteral?(name)]
Flag(#name);
```

To read the value of a compiler flag, use `Flag`. It returns the value of `-D<name>=value` as a static string, or an empty string if the flag was not set or had no value.

## External Function Attributes

These symbols are used as attributes on `external` function declarations to control their calling convention and linkage.

### Calling Convention

| Attribute           | Effect                        |
| ------------------- | ----------------------------- |
| `AttributeCCall`    | C calling convention          |
| `AttributeStdCall`  | `__stdcall` (Windows x86)     |
| `AttributeFastCall` | `__fastcall` (Windows x86)    |
| `AttributeThisCall` | `__thiscall` (Windows x86)    |
| `AttributeLLVMCall` | LLVM `ccc` calling convention |

### Linkage

| Attribute            | Effect                                   |
| -------------------- | ---------------------------------------- |
| `AttributeDLLImport` | `__dllimport` linkage on Windows targets |
| `AttributeDLLExport` | `__dllexport` linkage on Windows targets |

## Branch Prediction

### `usuallyEquals`

```ceramic
[T when Primitive?(T)]
usuallyEquals(value:T, #expected:T) : T;
```

`usuallyEquals` returns `value` unchanged, but passes a branch prediction hint to the optimizer: `expected` is the value you expect to see most of the time. The compiler uses this to generate faster code for the common path. The second argument must be a static value known at compile time.

The standard library's `likely` and `unlikely` functions in `hints` are built on this:

```ceramic
likely(x:Bool)   : Bool = usuallyEquals(x, #true);
unlikely(x:Bool) : Bool = usuallyEquals(x, #false);
```

The compile-time evaluator ignores the hint and returns `value` as-is.
