# Primitives Reference

**Version 0.1**

The `__primitives__` module is synthesized by the compiler and implicitly available to every Ceramic program. It provides primitive types, fundamental operations, and compile-time introspection.

For the language itself, see the [Language Reference](../language-reference/index.md).

## Conventions

- Each entry is introduced by its signature in a fenced `ceramic` block.
- Pattern guards (`[T | ...]`) and the trailing semicolon mark forward declarations.
- Functions documented here may not be overloaded unless otherwise noted.
- `SizeT` refers to the compiler-internal unsigned integer whose size matches a pointer. It is not actually exported by `__primitives__`.

## Sections

| Section                           | Contents                                                                          |
| --------------------------------- | --------------------------------------------------------------------------------- |
| [Primitive Types](types.md)       | `Bool`, integers, floats, pointers, `Array`, `Vec`, `Tuple`, `Union`, `Static`, … |
| [Data Access](data-access.md)     | `primitiveCopy`, `arrayRef`, `tupleRef`, `recordFieldRef`, enum conversions       |
| [Numeric Operations](numeric.md)  | Arithmetic, comparison, bitwise, conversion, checked integer ops                  |
| [Pointer Operations](pointers.md) | Pointer arithmetic, casts, function pointers                                      |
| [Atomic Operations](atomic.md)    | Memory orders, loads/stores, RMW, compare-exchange, fences                        |
| [Exceptions](exceptions.md)       | `activeException`                                                                 |
| [Introspection](introspection.md) | Symbols, types, records, variants, enums, static strings                          |
| [Compiler Interface](compiler.md) | Flags, external function attributes, miscellaneous                                |
