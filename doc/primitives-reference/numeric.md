# Numeric Operations

Arithmetic, comparison, bitwise, and conversion primitives for [`Bool`](types.md#bool), [integer](types.md#integer-types), [floating-point](types.md#floating-point-types), and [imaginary](types.md#imaginary-and-complex-types) types.

Binary numeric primitives require both operands to have the same type. If you need to mix types, convert first with `numericConvert`. Complex arithmetic is provided by the standard library. None of these primitives may be overloaded.

## `boolNot`

```ceramic
boolNot(x:Bool) : Bool;
```

`boolNot` is the same operation as the `not` operator. It gives you `true` when `x` is `false`, and `false` when `x` is `true`.

## `integerEquals?` / `integerLesser?`

```ceramic
[T when Integer?(T)]
integerEquals?(a:T, b:T) : Bool;
integerLesser?(a:T, b:T) : Bool;
```

These are the two basic integer comparison primitives. `integerEquals?` checks whether `a` and `b` have the same value. `integerLesser?` checks whether `a` is less than `b`, treating signed integers as signed and unsigned integers as unsigned. That means `-1 < 0` is `true` for `Int32` but `false` for `UInt32`.

## Floating-point comparison

```ceramic
[T when Float?(T)]
floatOrderedEquals?(a:T, b:T)        : Bool;
floatOrderedNotEquals?(a:T, b:T)     : Bool;
floatOrderedLesser?(a:T, b:T)        : Bool;
floatOrderedLesserEquals?(a:T, b:T)  : Bool;
floatOrderedGreater?(a:T, b:T)       : Bool;
floatOrderedGreaterEquals?(a:T, b:T) : Bool;
floatOrdered?(a:T, b:T)              : Bool;

floatUnorderedEquals?(a:T, b:T)        : Bool;
floatUnorderedNotEquals?(a:T, b:T)     : Bool;
floatUnorderedLesser?(a:T, b:T)        : Bool;
floatUnorderedLesserEquals?(a:T, b:T)  : Bool;
floatUnorderedGreater?(a:T, b:T)       : Bool;
floatUnorderedGreaterEquals?(a:T, b:T) : Bool;
floatUnordered?(a:T, b:T)              : Bool;
```

Floating-point has two families of comparison primitives, and they differ in how they handle NaN.

The `floatOrdered*` family gives you `false` if either operand is NaN. The `floatUnordered*` family gives you `true` if either operand is NaN. `floatOrdered?` tells you whether both operands are real numbers (neither is NaN). `floatUnordered?` tells you whether at least one operand is NaN.

`+0.0` and `-0.0` compare as equal in both families.

## `numericAdd` / `numericSubtract` / `numericMultiply`

```ceramic
[T when Numeric?(T)]
numericAdd(a:T, b:T) : T;
numericSubtract(a:T, b:T) : T;
numericMultiply(a:T, b:T) : T;
```

Standard addition, subtraction, and multiplication. For integers, overflow wraps silently using two's-complement rules. If you want overflow to be an error at runtime rather than wrapping, use the checked variants below.

## `floatDivide`

```ceramic
[T when Float?(T)]
floatDivide(a:T, b:T) : T;
```

Floating-point division following IEEE 754. Dividing by zero gives you infinity or NaN rather than an error.

## `integerQuotient`

```ceramic
[T when Integer?(T)]
integerQuotient(a:T, b:T) : T;
```

Integer division truncating toward zero. Dividing by zero is undefined behavior. So is dividing the minimum signed value by `-1` (for example, `-0x8000_0000 / -1` on `Int32`), because the result does not fit in the type.

## `numericNegate`

```ceramic
[T when Numeric?(T)]
numericNegate(a:T) : T;
```

`numericNegate` gives you the negative of `a`. For integers, negating the minimum signed value wraps silently back to itself. For floating-point, negating zero gives you the other zero (`-0.0` becomes `+0.0` and vice versa), and negating a NaN gives an unspecified NaN.

## `integerRemainder`

```ceramic
[T when Integer?(T)]
integerRemainder(a:T, b:T) : T;
```

The remainder after dividing `a` by `b`. For signed types, a nonzero result takes the sign of `a`. Division by zero and signed overflow are undefined behavior.

## `integerShiftLeft` / `integerShiftRight`

```ceramic
[T when Integer?(T)]
integerShiftLeft(a:T, b:T) : T;
integerShiftRight(a:T, b:T) : T;
```

These shift the bits of `a` by `b` positions. It is undefined behavior if `b` is negative or greater than or equal to the bit width of `T`.

`integerShiftLeft` fills the vacated low bits with zeros and discards any bits that shift out the top. `integerShiftRight` fills the vacated high bits with the sign bit for signed types and with zeros for unsigned types.

## `integerBitwiseAnd` / `Or` / `Xor`

```ceramic
[T when Integer?(T)]
integerBitwiseAnd(a:T, b:T) : T;
integerBitwiseOr(a:T, b:T)  : T;
integerBitwiseXor(a:T, b:T) : T;
```

These operate on each bit of `a` and `b` independently: AND, OR, and XOR respectively.

## `integerBitwiseNot`

```ceramic
[T when Integer?(T)]
integerBitwiseNot(a:T) : T;
```

`integerBitwiseNot` flips every bit of `a`.

## `numericConvert`

```ceramic
[T, U when Numeric?(T) and Numeric?(U)]
numericConvert(#T, a:U) : T;
```

`numericConvert` converts `a` to type `T`, preserving its numeric value as closely as possible. If `T` and `U` are the same type, the value is copied unchanged. The conversion rules depend on the source and destination types:

### Integer to Integer

| Direction                           | Behavior                        |
| ----------------------------------- | ------------------------------- |
| Narrowing (e.g. `Int64` to `Int32`) | High bits are discarded         |
| Widening to signed                  | Sign-extended (sign bit copied) |
| Widening to unsigned                | Zero-extended                   |
| Same width, different sign          | Bits are reinterpreted as-is    |

### Float to Float

Narrowing truncates toward the nearest representable value. Widening is exact. Overflowing truncation is undefined behavior.

### Integer to Float

Signed integers are converted as signed. Unsigned integers are converted as unsigned. Overflowing conversion is undefined behavior.

### Float to Integer

Truncates toward zero. Overflowing conversion is undefined behavior.

## Checked Integer Operations

These are overflow-safe versions of the integer primitives. Instead of wrapping silently on overflow, they raise a runtime error (`invalid integer math: ...`). Use them when you want overflow to be a hard failure rather than silent wraparound. None may be overloaded.

```ceramic
[T when Integer?(T)]
integerAddChecked(a:T, b:T)       : T;
integerSubtractChecked(a:T, b:T)  : T;
integerMultiplyChecked(a:T, b:T)  : T;
integerQuotientChecked(a:T, b:T)  : T;
integerNegateChecked(a:T)         : T;
integerRemainderChecked(a:T, b:T) : T;
integerShiftLeftChecked(a:T, b:T) : T;

[T, U when Integer?(T) and Integer?(U)]
integerConvertChecked(#T, a:U) : T;
```
