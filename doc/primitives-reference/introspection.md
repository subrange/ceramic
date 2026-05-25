# Introspection

Compile-time queries over symbols, types, records, variants, enums, and static strings. None of these may be overloaded.

## Symbol and Function Introspection

### `Type?`

```ceramic
[T]
Type?(static T) : Bool;
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
    println(Type?(static 3)); // false
}
```

### `CallDefined?`

```ceramic
[F, ..T]
CallDefined?(static F, static ..T) : Bool;
```

`true` if `F` has an overload matching input types `..T`.

To probe a non-symbol callable type, use `CallDefined?(call, FunctionType, ..T)`.

### `ModuleName`

```ceramic
[S]
ModuleName(static S) : StringConstant;
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
IdentifierModuleName(static S);
```

Like `ModuleName`, but returns a static string instead of a string literal.

### `StaticName`

```ceramic
[x]
StaticName(static x) : StringConstant;
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
IdentifierStaticName(static x);
```

Like `StaticName`, but returns a static string.

### `staticFieldRef`

```ceramic
[M, name | Identifier?(name)]
staticFieldRef(static M, static name);
```

Looks up a public global value named `name` in module `M` and evaluates as if it were referenced by name directly. Errors if `name` is not a public member of `M`.

## Static String Manipulation

### `Identifier?`

```ceramic
[S]
Identifier?(static S) : Bool;
```

`true` if `S` is a static string.

### `IdentifierSize`

```ceramic
[S | Identifier?(S)]
IdentifierSize(static S) : SizeT;
```

Number of characters in static string `S`.

### `IdentifierConcat`

```ceramic
[..SS | allValues?(Identifier?, ..SS)]
IdentifierConcat(static ..SS);
```

Concatenation of all argument static strings.

### `IdentifierSlice`

```ceramic
[S, n, m |
    Identifier?(S)
    and n >= 0 and n < IdentifierSize(S)
    and m >= 0 and m < IdentifierSize(S)
]
IdentifierSlice(static S, static n, static m);
```

Substring of `S` from index `n` up to (but not including) `m`.

## Type Introspection

### `TypeSize`

```ceramic
[T | Type?(T)]
TypeSize(static T) : SizeT;
```

Size in bytes of a value of type `T`.

### `TypeAlignment`

```ceramic
[T | Type?(T)]
TypeAlignment(static T) : SizeT;
```

Natural alignment in bytes of a value of type `T`.

### `CCodePointer?`

```ceramic
[T]
CCodePointer?(static T) : Bool;
```

`true` if `T` is a symbol and an instance of one of the [external code pointer types](types.md#external-code-pointer-types) (`CCodePointer`, `LLVMCodePointer`, …).

### `TupleElementCount`

```ceramic
[..T]
TupleElementCount(static Tuple[..T]) : SizeT;
```

Number of elements in the tuple type.

### `UnionMemberCount`

```ceramic
[..T]
UnionMemberCount(static Union[..T]) : SizeT;
```

Number of member types in the union type.

### Record Introspection

```ceramic
[R]
Record?(static R) : Bool;

[R | Record?(R)]
RecordFieldCount(static R) : SizeT;

[R, n | Record?(R) and n >= 0 and n < RecordFieldCount(R)]
RecordFieldName(static R, static n);                // static string

[R, name | Record?(R) and Identifier?(name)]
RecordWithField?(static R, static name) : Bool;
```

- `Record?`: `true` if `R` names a record type.
- `RecordFieldCount`: field count for record type `R`.
- `RecordFieldName`: name of the `n`th field as a static string.
- `RecordWithField?`: `true` if `R` has a field named `name`.

### Variant Introspection

```ceramic
[V]
Variant?(static V) : Bool;

[V | Variant?(V)]
VariantMemberCount(static V) : SizeT;

[V, n | Variant?(V) and n >= 0 and n < VariantMemberCount(V)]
VariantMemberIndex(static V, static n);
```

- `Variant?`: `true` if `V` names a variant type.
- `VariantMemberCount`: number of instance types.
- `VariantMemberIndex`: the `n`th instance type. The mapping from index to instance is unspecified, but iterating `0 .. VariantMemberCount(V)` visits each instance exactly once.

### Enum Introspection

```ceramic
[E]
Enum?(static E) : Bool;

[E | Enum?(E)]
EnumMemberCount(static E) : SizeT;

[E, n | Enum?(E) and n >= 0 and n < EnumMemberCount(E)]
EnumMemberName(static E, static n) : StringConstant;
```

- `Enum?`: `true` if `E` names an enum type.
- `EnumMemberCount`: number of values.
- `EnumMemberName`: string literal naming the `n`th value, evaluated via `StringConstant`.
