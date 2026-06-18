# Compiler Interface

Compilation-unit settings, external-function attributes, and assorted utilities. None of these may be overloaded.

## Compiler Flags

### `ExceptionsEnabled?`

```ceramic
// If exceptions are enabled in this compilation unit:
alias ExceptionsEnabled? = true;
// Otherwise:
alias ExceptionsEnabled? = false;
```

A global alias set to `true` when exceptions are enabled for the current compilation, `false` otherwise.

### `Flag?`

```ceramic
[name when StringLiteral?(name)]
Flag?(#name) : Bool;
```

`true` if the compiler was invoked with a `-D<name>` or `-D<name>=value` matching `name`.

### `Flag`

```ceramic
[name when StringLiteral?(name)]
Flag(#name);
```

Returns the value of the compiler flag `-D<name>=value` as a static string. If no such flag was given, or the flag was given without a value, returns the empty static string `#""`.

## External Function Attributes

These symbols may be used as attributes on external function declarations.

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

Returns `value` unchanged, but annotates the LLVM branch weight so the compiler treats `expected` as the likely outcome. Lowers to an LLVM `expect` intrinsic call. The second argument must be a static value.

Used by the standard library's `likely` and `unlikely` wrappers (in `hints`):

```ceramic
likely(x:Bool)   : Bool = usuallyEquals(x, #true);
unlikely(x:Bool) : Bool = usuallyEquals(x, #false);
```

The compile-time evaluator returns `value` without any branch annotation.
