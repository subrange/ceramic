# Primitive Types

## `Bool`

A boolean value: `true` or `false`. Corresponds to LLVM `i1`, C99 `_Bool`, C++ `bool`.

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

- `Imag32`, `Imag64`, `Imag80`: share LLVM/C representation with their floating type but are semantically imaginary.
- `Complex32`, `Complex64`, `Complex80`: LLVM `{float, float}` etc., C99 `_Complex float` etc.

## `Pointer`

```ceramic
Pointer[T]
```

A pointer to a value of type `T`. Corresponds to LLVM `%T*` or C `T*`. Created with prefix `@` or [`addressOf`](pointers.md#addressof).

## `CodePointer`

```ceramic
CodePointer[[..In], [..Out]]
```

A pointer to a Ceramic function instance. Created with [`makeCodePointer`](pointers.md#makecodepointer). The Ceramic calling convention is unspecified, so it has no fixed LLVM/C equivalent.

## External Code Pointer Types

```ceramic
CCodePointer[[..In], [..Out]]
```

A pointer to a C function. `CCodePointer[[A,B,C],[]]` corresponds to `void (*)(A,B,C)`. `CCodePointer[[A,B,C],[D]]` corresponds to `D (*)(A,B,C)`.

Variants for other conventions:

- `LLVMCodePointer[[..In],[..Out]]`: LLVM `ccc` convention.
- `VarArgsCCodePointer[[..In],[..Out]]`: variadic C: `D (*)(A,B,C,...)`.
- `StdCallCodePointer`, `FastCallCodePointer`, `ThisCallCodePointer`: legacy Windows x86 conventions.

These pointers are obtained by evaluating external function names, returning them from C functions, or via [`makeCCodePointer`](pointers.md#makeccodepointer). They are invoked through [`callCCodePointer`](pointers.md#callccodepointer).

## `Array`

```ceramic
Array[T, n]
```

Fixed-size, locally-allocated array of `n` elements of type `T`. `n` must be `Int32`. Corresponds to LLVM `[n x %T]` or C `T[n]`.

Unlike C arrays, Ceramic arrays do **not** decay to pointers. Use [`arrayRef`](data-access.md#arrayref) and [`arrayElements`](data-access.md#arrayelements) for access.

## `Vec`

```ceramic
Vec[T, n]
```

SIMD vector of `n` elements of type `T`. `n` must be `Int32`. Corresponds to LLVM `<n x %T>` or the GCC extension `T __attribute__((vector_size(...)))`.

No high-level primitives are provided. Use `Vec` with LLVM vector intrinsics.

## `Tuple`

```ceramic
Tuple[..T]
```

Anonymous, ordered aggregate. Laid out like a naturally-aligned C `struct`. `Tuple[A,B,C]` corresponds to the LLVM struct `{%A, %B, %C}`.

## `Union`

```ceramic
Union[..T]
```

Anonymous, non-discriminated union. Laid out like a naturally-aligned C `union`. LLVM has no union type. The compiler picks an LLVM type with the correct size and alignment.

## `Static`

```ceramic
Static[x]
```

A stateless type representing a compile-time value. Ceramic symbols, static strings, and `static` expressions evaluate to instances of `Static[…]`.

`Static` values emit as LLVM `i8 undef` and still take space inside tuples and records.
