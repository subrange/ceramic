# Data Access

Primitives for reading and writing values inside aggregates (arrays, tuples, records) and for converting enums to and from integers. None may be overloaded.

## `bitcopy`

```ceramic
[T when Primitive?(T)]
bitcopy(dest:T, src:T) :;
```

`bitcopy` copies the raw bytes of `src` directly into `dest` without calling any `copy` operator. `T` must be a primitive type: `Bool`, any integer or float, any pointer type, an enum, a newtype, or `Static[x]`. Records, arrays, tuples, unions, and vectors are not supported.

## `arrayRef`

```ceramic
[T, n, I when Integer?(I)]
arrayRef(array:Array[T, n], i:I) : ref T;
```

To read or write a single element of an array, use `arrayRef`. It returns a reference to element `i`, so you can both read from and assign through it. Indexing is zero-based and not bounds-checked.

## `arrayElements`

```ceramic
[T, n]
arrayElements(array:Array[T, n]) : ref ..replicateValue(T, #n);
```

When you need to work with all elements of an array at once, `arrayElements` unpacks them as a multiple-value list of references, in order.

## `tupleRef`

```ceramic
[..T, n when n >= 0 and n < countValues(..T)]
tupleRef(tuple:Tuple[..T], #n) : ref nthValue(#n, ..T);
```

Element access on a tuple is done through `tupleRef`. The index `n` must be a static value, so an out-of-bounds index is caught at compile time rather than causing a runtime crash.

## `tupleElements`

```ceramic
[..T]
tupleElements(tuple:Tuple[..T]) : ref ..T;
```

To unpack an entire tuple into its components, use `tupleElements`. It returns a reference to every element as a multiple-value list, in order.

## `recordFieldRef`

```ceramic
[R, n when Record?(R) and n >= 0 and n < RecordFieldCount(R)]
recordFieldRef(rec:R, #n) : ref RecordFieldType(R, #n);
```

To access a record field by position, use `recordFieldRef`. The index `n` is checked at compile time, so an out-of-bounds access is a compile error.

## `recordFieldRefByName`

```ceramic
[R, name when Record?(R) and StringLiteral?(name) and RecordWithField?(R, name)]
recordFieldRefByName(rec:R, #name) : ref;
```

To access a record field by a name that is only known at compile time, use `recordFieldRefByName`. It is a compile error if `R` has no field with that name.

## `recordFields`

```ceramic
[R when Record?(R)]
recordFields(rec:R) : ref ..RecordFieldTypes(R);
```

To unpack every field of a record into a multiple-value list, use `recordFields`. The fields are returned as references in declaration order.

## `recordVariadicField`

```ceramic
[R when Record?(R)]
recordVariadicField(rec:R) : ref ..VariadicFieldTypes(R);
```

To unpack the variadic field of a record, use `recordVariadicField`. It returns a reference to each element as a multiple-value list. It is a compile error if `R` has no variadic field. The number of references equals the number of elements the variadic field was instantiated with.

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

The underlying integer ordinal of enum value `en` is available via `enumToInt`. Ordinals are assigned in declaration order starting from zero.

## `intToEnum`

```ceramic
[E when Enum?(E)]
intToEnum(#E, n:Int32) : E;
```

To construct an enum value from an integer, use `intToEnum`. The integer is not checked against the values defined for `E`.
