# Primitive Types

## `Bool`

A boolean value: either `true` or `false`. Equivalent to `bool` in C++ and `_Bool` in C99.

## Integer Types

Signed and unsigned integer types are provided at 8, 16, 32, 64, and 128 bits:

| Signed   | Unsigned  | LLVM   | C99 (`<stdint.h>`)    |
| -------- | --------- | ------ | --------------------- |
| `Int8`   | `UInt8`   | `i8`   | `int8_t`, `uint8_t`   |
| `Int16`  | `UInt16`  | `i16`  | `int16_t`, `uint16_t` |
| `Int32`  | `UInt32`  | `i32`  | `int32_t`, `uint32_t` |
| `Int64`  | `UInt64`  | `i64`  | `int64_t`, `uint64_t` |
| `Int128` | `UInt128` | `i128` | (extension)           |

LLVM itself does not distinguish signed and unsigned integer types. Ceramic enforces the distinction at the type level.

The unsigned integer type whose width matches a pointer is internally referred to as `SizeT` and is used as the return type of indexing primitives. `SizeT` is **not** exported from `__primitives__`.

## Floating-Point Types

| Type      | LLVM       | C                             |
| --------- | ---------- | ----------------------------- |
| `Float32` | `float`    | `float`                       |
| `Float64` | `double`   | `double`                      |
| `Float80` | `x86_fp80` | `long double` (Unix x86 only) |

## Imaginary and Complex Types

For each floating-point width:

- `Imag32`, `Imag64`, `Imag80`: use the same memory layout as their corresponding float type but are treated as imaginary numbers by the type system.
- `Complex32`, `Complex64`, `Complex80`: a pair of floats representing a complex number. Equivalent to `_Complex float` and friends in C99.

## `Pointer`

```ceramic
Pointer[T]
```

A pointer to a value of type `T`, equivalent to `T*` in C. You create one with the prefix `@` operator or with [`addressOf`](pointers.md#addressof).

## `CodePointer`

```ceramic
CodePointer[[..In], [..Out]]
```

A pointer to a specific compiled instance of a Ceramic function. You create one with [`makeCodePointer`](pointers.md#makecodepointer). Because Ceramic's internal calling convention is unspecified, `CodePointer` has no C equivalent and cannot be passed directly to C code.

## External Code Pointer Types

```ceramic
ExternalCodePointer[CC, V?, [..In], [..Out]]
```

A pointer to a function using a foreign calling convention `CC`, optionally variadic (`V?`). `ExternalCodePointer[cdecl, false, [A,B,C], []]` corresponds to `void (*)(A,B,C)`; with `[D]` outputs, `D (*)(A,B,C)`.

The common conventions have aliases:

- `CCodePointer[[..In],[..Out]]` = `ExternalCodePointer[cdecl, false, …]`.
- `VarArgsCCodePointer[[..In],[..Out]]` = `ExternalCodePointer[cdecl, true, …]`: variadic C, `D (*)(A,B,C,...)`.
- `LLVMCodePointer[[..In],[..Out]]` = `ExternalCodePointer[llvm, false, …]`.
- `StdCallCodePointer`, `FastCallCodePointer`, `ThisCallCodePointer`: legacy Windows x86 conventions.

These pointers are obtained by evaluating external function names, returning them from C functions, or via [`makeExternalCodePointer`](pointers.md#makeexternalcodepointer). They are invoked through [`callExternalCodePointer`](pointers.md#callexternalcodepointer).

## `Array`

```ceramic
Array[T, n]
```

A fixed-size array of `n` elements of type `T`, equivalent to `T[n]` in C. `n` must be `Int32`.

Unlike C arrays, Ceramic arrays do **not** decay to pointers. Use [`arrayRef`](data-access.md#arrayref) and [`arrayElements`](data-access.md#arrayelements) to access elements.

## `Vec`

```ceramic
Vec[T, n]
```

A SIMD vector of `n` elements of type `T`. `n` must be `Int32`. Equivalent to the GCC extension `T __attribute__((vector_size(...)))`.

No high-level primitives are provided for `Vec`. You use it together with LLVM vector intrinsics declared in a top-level `__llvm__` block.

## `Tuple`

```ceramic
Tuple[..T]
```

An anonymous, ordered grouping of values of different types. `Tuple[A,B,C]` is laid out in memory like a naturally-aligned C `struct` with three fields of types `A`, `B`, and `C`.

## `Union`

```ceramic
Union[..T]
```

An anonymous union type that can hold a value of any of its member types. Unlike a variant, it does not track which type it currently holds. Laid out like a naturally-aligned C `union`.

## `Static`

```ceramic
Static[x]
```

A stateless type that carries a compile-time value. Ceramic symbols, static strings, and `#` static expressions all have a `Static[…]` type. `Static` values have no meaningful runtime representation but still occupy space inside tuples and records.

## `ByRef`

```ceramic
ByRef[T]
```

A marker used in return type declarations to say that a function returns a reference to a `T` rather than a copy of one. It only appears in return type specifications and has no runtime representation.

```ceramic
[T]
overload index(a:Array[T, n], i:Int) : ByRef[T] = ref arrayRef(a, i);
```

A function declared to return `ByRef[T]` must use `return ref`. Callers receive a reference; the returned address must outlive the call. All `return` statements in the function must use the same ref qualification.

## `RecordWithProperties`

```ceramic
RecordWithProperties[Properties, Fields]
```

A compiler-internal record type. When used as the result of a computed record body, it attaches compile-time `Properties` metadata to the record layout described by `Fields`. Library wrappers `recordWithProperties`, `recordWithProperty`, and `recordWithPredicate` (from `core.records`) are the intended API; using `RecordWithProperties` directly is rarely necessary.
