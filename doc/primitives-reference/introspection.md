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

### `IdentifierModuleName`

```ceramic
[S]
IdentifierModuleName(#S);
```

Like `ModuleName`, but returns a static string instead of a string literal.

### `StaticName`

```ceramic
[x]
StaticName(#x) : StringConstant;
```

Generates a string literal naming the static value `x`:

- Symbol: its name (without module, with parameters).
- Static string: its string value.
- Numeric value: its decimal representation.
- Tuple: comma-delimited inside square brackets (`[a, b, c]`).

Evaluated via `StringConstant`.

### `IdentifierStaticName`

```ceramic
[x]
IdentifierStaticName(#x);
```

Like `StaticName`, but returns a static string.

### `staticFieldRef`

```ceramic
[M, name when Identifier?(name)]
staticFieldRef(#M, #name);
```

Looks up a public global value named `name` in module `M` and evaluates as if it were referenced by name directly. Errors if `name` is not a public member of `M`.

## Static String Manipulation

### `Identifier?`

```ceramic
[S]
Identifier?(#S) : Bool;
```

`true` if `S` is a static string.

### `IdentifierSize`

```ceramic
[S when Identifier?(S)]
IdentifierSize(#S) : SizeT;
```

Number of characters in static string `S`.

### `IdentifierConcat`

```ceramic
[..SS when allValues?(Identifier?, ..SS)]
IdentifierConcat(#..SS);
```

Concatenation of all argument static strings.

### `IdentifierSlice`

```ceramic
[S, n, m |
    Identifier?(S)
    and n >= 0 and n < IdentifierSize(S)
    and m >= 0 and m < IdentifierSize(S)
]
IdentifierSlice(#S, #n, #m);
```

Substring of `S` from index `n` up to (but not including) `m`.

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

### `CCodePointer?`

```ceramic
[T]
CCodePointer?(#T) : Bool;
```

`true` if `T` is a symbol and an instance of one of the [external code pointer types](types.md#external-code-pointer-types) (`CCodePointer`, `LLVMCodePointer`, …).

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

[R, name when Record?(R) and Identifier?(name)]
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

[V when Variant?(V)]
VariantMemberCount(#V) : SizeT;

[V, n when Variant?(V) and n >= 0 and n < VariantMemberCount(V)]
VariantMemberIndex(#V, #n);
```

- `Variant?`: `true` if `V` names a variant type.
- `VariantMemberCount`: number of instance types.
- `VariantMemberIndex`: the `n`th instance type. The mapping from index to instance is unspecified, but iterating `0 .. VariantMemberCount(V)` visits each instance exactly once.

### Enum Introspection

```ceramic
[E]
Enum?(#E) : Bool;

[E when Enum?(E)]
EnumMemberCount(#E) : SizeT;

[E, n when Enum?(E) and n >= 0 and n < EnumMemberCount(E)]
EnumMemberName(#E, #n) : StringConstant;
```

- `Enum?`: `true` if `E` names an enum type.
- `EnumMemberCount`: number of values.
- `EnumMemberName`: string literal naming the `n`th value, evaluated via `StringConstant`.
