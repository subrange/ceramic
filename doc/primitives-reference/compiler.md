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
[name when Identifier?(name)]
Flag?(#name) : Bool;
```

`true` if the compiler was invoked with a `-D<name>` or `-D<name>=value` matching `name`.

### `Flag`

```ceramic
[name when Identifier?(name)]
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

## Miscellaneous

### `staticIntegers`

```ceramic
[n when n >= 0]
staticIntegers(#n);
```

Returns a multiple-value list of static integers from `#0` up to `#(n - 1)`. `staticIntegers(#0)` returns no values.
