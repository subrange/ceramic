# Numeric Operations

Arithmetic, comparison, bitwise, and conversion primitives for [`Bool`](types.md#bool), [integer](types.md#integer-types), [floating-point](types.md#floating-point-types), and [imaginary](types.md#imaginary-and-complex-types) types.

Binary numeric primitives require operands of matching types. Heterogeneous-type conversion is left to the library. Complex math is also library-provided. None of these primitives may be overloaded.

## `boolNot`

```ceramic
boolNot(x:Bool) : Bool;
```

Returns the complement of `x`. Equivalent to the `not` operator.

## `numericEquals?`

```ceramic
[T | Numeric?(T)]
numericEquals?(a:T, b:T) : Bool;
```

Numeric equality.

- Integer: LLVM `icmp eq`.
- Floating-point: LLVM `fcmp ueq` (IEEE 754 unordered). `+0.0 == -0.0`. Any comparison with NaN is false.

## `numericLesser?`

```ceramic
[T | Numeric?(T)]
numericLesser?(a:T, b:T) : Bool;
```

`true` if `a < b`.

- Signed integer: LLVM `icmp slt`.
- Unsigned integer: LLVM `icmp ult`.
- Floating-point: LLVM `fcmp ult` (IEEE 754 unordered). `-0.0 < +0.0` is false. NaN comparisons are false.

`__primitives__` does not expose the full set of FP comparisons. The library implements ordered and unordered FP comparison via inline LLVM. These primitives are only used during compile-time evaluation, which cannot run inline LLVM.

## `numericAdd` / `numericSubtract` / `numericMultiply`

```ceramic
[T | Numeric?(T)]
numericAdd(a:T, b:T) : T;
numericSubtract(a:T, b:T) : T;
numericMultiply(a:T, b:T) : T;
```

Standard arithmetic. Integer overflow wraps (two's-complement). Integer ops lower to `add`, `sub`, `mul`. Floating-point ops lower to `fadd`, `fsub`, `fmul`.

## `numericDivide`

```ceramic
[T | Numeric?(T)]
numericDivide(a:T, b:T) : T;
```

Integer division truncates toward zero. Integer division by zero is **undefined**, as is signed overflow (e.g. `-0x8000_0000 / -1`). Floating-point division follows IEEE 754.

- Signed: `sdiv`. Unsigned: `udiv`. Floating-point: `fdiv`.

## `numericNegate`

```ceramic
[T | Numeric?(T)]
numericNegate(a:T) : T;
```

Negation.

- Integer: behaves as two's-complement subtraction from zero (LLVM `sub 0, %a`). Unsigned negation gives the two's complement. Signed overflow (negating `-0x8000_0000`) gives the original value.
- Floating-point: LLVM `fsub -0.0, %a`. Negating a zero yields the other zero. Negating a NaN yields an unspecified other NaN.

## `integerRemainder`

```ceramic
[T | Integer?(T)]
integerRemainder(a:T, b:T) : T;
```

Remainder of `a / b`. For signed types, a nonzero remainder takes the sign of `a`. Division by zero and signed overflow are undefined (LLVM defines the remainder of overflowing division as undefined as well).

- Signed: `srem`. Unsigned: `urem`.

## `integerShiftLeft` / `integerShiftRight`

```ceramic
[T | Integer?(T)]
integerShiftLeft(a:T, b:T) : T;
integerShiftRight(a:T, b:T) : T;
```

Shift `a` by `b` bits. Undefined if `b` is negative or `>= bitwidth(T)`.

- `integerShiftLeft` → LLVM `shl`. Overflowed bits discarded.
- `integerShiftRight` → arithmetic shift (`ashr`) for signed types, logical (`lshr`) for unsigned.

## `integerBitwiseAnd` / `Or` / `Xor`

```ceramic
[T | Integer?(T)]
integerBitwiseAnd(a:T, b:T) : T;
integerBitwiseOr(a:T, b:T)  : T;
integerBitwiseXor(a:T, b:T) : T;
```

Bitwise AND, OR, XOR. Lower to LLVM `and`, `or`, `xor`.

## `integerBitwiseNot`

```ceramic
[T | Integer?(T)]
integerBitwiseNot(a:T) : T;
```

Bitwise complement. Lowers to LLVM `xor %T %a, -1`.

## `numericConvert`

```ceramic
[T, U | Numeric?(T) and Numeric?(U)]
numericConvert(static T, a:U) : T;
```

Converts `a` to type `T` while preserving its numeric value. If `T == U`, the value is copied. Otherwise, the conversion depends on the kinds of `T` and `U`:

### Integer → Integer

| Direction | LLVM |
|-----------|------|
| Narrowing | `trunc` (bitwise truncation) |
| Widening to signed | `sext` (sign-extend) |
| Widening to unsigned | `zext` (zero-extend) |
| Same width, sign change | `bitcast` |

### Float → Float

- Narrowing: `fptrunc`. Overflowing truncation is undefined.
- Widening: `fpext`.

### Integer → Float

- Signed: `sitofp`. Unsigned: `uitofp`.
- Overflowing conversion is undefined.

### Float → Integer

- Signed: `fptosi`. Unsigned: `fptoui`.
- Overflowing conversion is undefined.

## Checked Integer Operations

Variants of the integer primitives that also return a `Bool` overflow flag. On overflow, the numeric result is **undefined** and the flag is `true`. Otherwise the result matches the unchecked version and the flag is `false`. None may be overloaded.

```ceramic
[T | Integer?(T)]
integerAddChecked(a:T, b:T)      : T, Bool;
integerSubtractChecked(a:T, b:T) : T, Bool;
integerMultiplyChecked(a:T, b:T) : T, Bool;
integerDivideChecked(a:T, b:T)   : T, Bool;
integerNegateChecked(a:T)        : T, Bool;
integerRemainderChecked(a:T, b:T): T, Bool;
integerShiftLeftChecked(a:T, b:T): T, Bool;

[T, U | Integer?(T) and Integer?(U)]
integerConvertChecked(static T, a:U) : T, Bool;
```
