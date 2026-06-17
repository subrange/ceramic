# Data Access

Fundamental operations on aggregates and enums. None may be overloaded.

## `primitiveCopy`

```ceramic
[T]
primitiveCopy(dest:T, src:T) :;
```

Bitwise copies `TypeSize(T)` bytes from `src` into `dest`. Lowers to an LLVM `load` followed by `store`.

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
arrayElements(array:Array[T, n]) : ref ..repeatValue(#n, T);
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
[R, name when Record?(R) and Identifier?(name) and RecordWithField?(R, name)]
recordFieldRefByName(rec:R, #name) : ref RecordFieldTypeByName(R, name);
```

Returns a reference to the field named `name` (a static string) in `rec`.

## `recordFields`

```ceramic
[R when Record?(R)]
recordFields(rec:R) : ref ..RecordFieldTypes(R);
```

Returns a multiple-value list of references to all fields of `rec` in declaration order.

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
