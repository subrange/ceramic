# Data Access

Fundamental operations on aggregates and enums. None may be overloaded.

## `bitcopy`

```ceramic
[T when Primitive?(T)]
bitcopy(dest:T, src:T) :;
```

Bitwise copies `TypeSize(T)` bytes from `src` into `dest`. Lowers to an LLVM `load` followed by `store`. `T` must be a primitive type: `Bool`, an integer or float or complex type, a pointer or code pointer type, an enum, a newtype, or `Static[x]`. Records, arrays, tuples, unions, and vectors are not supported.

## `arrayRef`

```ceramic
[T, n, I when Integer?(I)]
arrayRef(array:Array[T, n], i:I) : ref T;
```

Returns a reference to element `i` of `array`.

- Zero-based. Not bounds-checked.
- Lowers to LLVM `getelementptr`.

## `arrayElements`

```ceramic
[T, n]
arrayElements(array:Array[T, n]) : ref ..replicateValue(T, #n);
```

Returns a multiple-value list of references to every element of `array` in order.

## `tupleRef`

```ceramic
[..T, n when n >= 0 and n < countValues(..T)]
tupleRef(tuple:Tuple[..T], #n) : ref nthValue(#n, ..T);
```

Returns a reference to the `n`th element of `tuple`.

- Zero-based. `n` is checked at compile time.
- Lowers to LLVM `getelementptr`.

## `tupleElements`

```ceramic
[..T]
tupleElements(tuple:Tuple[..T]) : ref ..T;
```

Returns a multiple-value list of references to every tuple element in order.

## `recordFieldRef`

```ceramic
[R, n when Record?(R) and n >= 0 and n < RecordFieldCount(R)]
recordFieldRef(rec:R, #n) : ref RecordFieldType(R, #n);
```

Returns a reference to the `n`th field of a record value.

- Zero-based. `n` is checked at compile time.
- Lowers to LLVM `getelementptr`.

## `recordFieldRefByName`

```ceramic
[R, name when Record?(R) and StringLiteral?(name) and RecordWithField?(R, name)]
recordFieldRefByName(rec:R, #name) : ref;
```

Returns a reference to the field named `name` (a static string) in `rec`.

## `recordFields`

```ceramic
[R when Record?(R)]
recordFields(rec:R) : ref ..RecordFieldTypes(R);
```

Returns a multiple-value list of references to all fields of `rec` in declaration order.

## `recordVariadicField`

```ceramic
[R when Record?(R)]
recordVariadicField(rec:R) : ref ..VariadicFieldTypes(R);
```

Returns a multiple-value list of references to the variadic field elements of `rec`. Errors at compile time if `R` has no variadic field. The number of references equals the number of variadic field elements captured in the record.

```ceramic
record Packet[..T] (header:Int32, ..body:T, checksum:UInt32);

[..T]
overload Packet(h:Int32, ..body:T, cs:UInt32) = Packet[..T](h, ..body, cs);

main() {
    var p = Packet(1, "hello", 42, 0xDEADBEEFu);
    println(..recordVariadicField(p));  // hello42
}
```

## `enumToInt`

```ceramic
[E when Enum?(E)]
enumToInt(en:E) : Int32;
```

Returns the ordinal of `en` as an `Int32`.

## `intToEnum`

```ceramic
[E when Enum?(E)]
intToEnum(#E, n:Int32) : E;
```

Returns the value of enum type `E` with ordinal `n`. Not bounds-checked against the values defined for `E`.
