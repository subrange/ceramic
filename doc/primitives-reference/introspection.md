# Introspection

Compile-time queries over symbols, types, records, variants, enums, and static strings. None of these may be overloaded.

## Symbol and Function Introspection

### `Type?`

```ceramic
[T]
Type?(#T) : Bool;
```

`true` if `T` is a symbol that names a type.

```ceramic
define foo;
record bar ();

main() {
    println(Type?(Type?));    // false
    println(Type?(Int32));    // true
    println(Type?(foo));      // false
    println(Type?(bar));      // true
    println(Type?(#3));       // false
}
```

### `CallDefined?`

```ceramic
[F, ..T]
CallDefined?(#F, #..T) : Bool;
```

`true` if `F` has an overload matching input types `..T`.

To probe a non-symbol callable type, use `CallDefined?(call, FunctionType, ..T)`.

### `ModuleName`

```ceramic
[S]
ModuleName(#S) : StringConstant;
```

Generates a string literal containing the fully-qualified module name containing the symbol `S`. Evaluated via the `StringConstant` operator function. If `S` is itself a module, returns the module's own name. Errors if `S` is not a symbol.

```ceramic
import foo;
import foo.bar as bar;

in baz;

main() {
    println(ModuleName(main));    // "baz"
    println(ModuleName(foo.a));   // "foo"
    println(ModuleName(bar.a));   // "foo.bar"
    println(ModuleName(bar));     // "foo.bar"
}
```

### `ModuleMemberNames`

```ceramic
[M]
ModuleMemberNames(#M);                    // static strings
```

A multiple-value list of static strings naming every public global in module `M`, in alphabetical order. `M` must be a module object (e.g., from `MainModule()` or `StaticModule(S)`).

```ceramic
import __primitives__.(MainModule, ModuleMemberNames);
import printer.*;

main() {
    println(..ModuleMemberNames(MainModule()));
}
```

### `StaticName`

```ceramic
[x]
StaticName(#x);                           // static string
```

Returns a static string naming the static value `x`:

- Symbol: its name (without module, with parameters).
- Static string: its string value.
- Numeric value: its decimal representation.
- Tuple: comma-delimited inside square brackets (`[a, b, c]`).

### `GetOverload`

```ceramic
[F, ..T]
GetOverload(#F, #..T);
```

Selects the overload of symbol `F` that matches argument types `..T` and returns it as a new callable procedure. The returned value can be called like any function. Unlike `makeCodePointer`, the result is still a fully generic Ceramic callable, not a fixed function pointer.

```ceramic
define foo;
overload foo(x:Int)   { println("Int ",   x); }
overload foo(x:Float) { println("Float ", x); }

main() {
    GetOverload(foo, Float)(123);  // prints: Float 123
}
```

### `staticFieldRef`

```ceramic
[M, name when StringLiteral?(name)]
staticFieldRef(#M, #name);
```

Looks up a public global value named `name` in module `M` and evaluates as if it were referenced by name directly. Errors if `name` is not a public member of `M`.

## Static String Manipulation

### `StringLiteral?`

```ceramic
[S]
StringLiteral?(#S) : Bool;
```

`true` if `S` is a static string.

### `stringLiteralByteSize`

```ceramic
[S when StringLiteral?(S)]
stringLiteralByteSize(#S) : SizeT;
```

Number of characters in static string `S`.

### `stringLiteralConcat`

```ceramic
[..SS when allValues?(StringLiteral?, ..SS)]
stringLiteralConcat(#..SS);
```

Concatenation of all argument static strings.

### `stringLiteralByteSlice`

```ceramic
[S, n, m when
    StringLiteral?(S)
    and n >= 0 and n < stringLiteralByteSize(S)
    and m >= 0 and m < stringLiteralByteSize(S)
]
stringLiteralByteSlice(#S, #n, #m);
```

Substring of `S` from index `n` up to (but not including) `m`.

### `stringLiteralByteIndex`

```ceramic
[S, n when StringLiteral?(S) and n >= 0 and n < stringLiteralByteSize(S)]
stringLiteralByteIndex(#S, #n) : Int32;
```

The byte at index `n` of static string `S`, as an `Int32`.

### `stringLiteralBytes`

```ceramic
[S when StringLiteral?(S)]
stringLiteralBytes(#S) : ..Int32;
```

A multiple-value list of every byte of `S` in order, each an `Int32`.

### `stringLiteralFromBytes`

```ceramic
[..bytes]
stringLiteralFromBytes(#..bytes);         // static string
```

Builds a static string from the given byte values. Each argument is a static integer in `0 .. 255`.

## Type Introspection

### `TypeSize`

```ceramic
[T when Type?(T)]
TypeSize(#T) : SizeT;
```

Size in bytes of a value of type `T`.

### `TypeAlignment`

```ceramic
[T when Type?(T)]
TypeAlignment(#T) : SizeT;
```

Natural alignment in bytes of a value of type `T`.

### `BaseType`

```ceramic
[T when Type?(T)]
BaseType(#T);                           // static type
```

The underlying representation type of `T`. For a new type, this is the type it wraps. For any other type, it is `T` itself.

### `TupleElementCount`

```ceramic
[..T]
TupleElementCount(#Tuple[..T]) : SizeT;
```

Number of elements in the tuple type.

### `UnionMemberCount`

```ceramic
[..T]
UnionMemberCount(#Union[..T]) : SizeT;
```

Number of member types in the union type.

### Record Introspection

```ceramic
[R]
Record?(#R) : Bool;

[R when Record?(R)]
RecordFieldCount(#R) : SizeT;

[R, n when Record?(R) and n >= 0 and n < RecordFieldCount(R)]
RecordFieldName(#R, #n);                // static string

[R, name when Record?(R) and StringLiteral?(name)]
RecordWithField?(#R, #name) : Bool;
```

- `Record?`: `true` if `R` names a record type.
- `RecordFieldCount`: field count for record type `R`.
- `RecordFieldName`: name of the `n`th field as a static string.
- `RecordWithField?`: `true` if `R` has a field named `name`.

### Variant Introspection

```ceramic
[V]
Variant?(#V) : Bool;

[V | Variant?(V)]
VariantMemberCount(#V) : SizeT;

[V, n | Variant?(V) and n >= 0 and n < VariantMemberCount(V)]
VariantMemberIndex(#V, #n);
```

- `Variant?`: `true` if `V` names a variant type.
- `VariantMemberCount`: number of instance types.
- `VariantMemberIndex`: the `n`th instance type. The mapping from index to instance is unspecified, but iterating `0 .. VariantMemberCount(V)` visits each instance exactly once.

### Enum Introspection

```ceramic
[E]
Enum?(#E) : Bool;

[E | Enum?(E)]
EnumMemberCount(#E) : SizeT;

[E, n | Enum?(E) and n >= 0 and n < EnumMemberCount(E)]
EnumMemberName(#E, #n) : StringConstant;
```

- `Enum?`: `true` if `E` names an enum type.
- `EnumMemberCount`: number of values.
- `EnumMemberName`: string literal naming the `n`th value, evaluated via `StringConstant`.
