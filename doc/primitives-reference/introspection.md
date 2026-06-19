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

### `Symbol?`

```ceramic
[x]
Symbol?(#x) : Bool;
```

`true` if `x` names a symbol: a type, record, variant, procedure, intrinsic, or global alias. `false` for static values such as numbers or static strings.

### `Operator?`

```ceramic
[x]
Operator?(#x) : Bool;
```

`true` if `x` is a symbol declared as an operator (with `define (op)`). `false` for ordinary symbols and non-symbols.

### `StaticCallDefined?`

```ceramic
[F, ..T]
StaticCallDefined?(#F, #..T) : Bool;
```

`true` if symbol `F` has an overload matching input types `..T`. The first argument must be a symbol (not a callable value).

The library function `CallDefined?` (from `core.operators`) wraps this primitive and additionally handles callable record types via `StaticCallDefined?(call, F, ..T)`.

### `StaticCallOutputTypes`

```ceramic
[F, ..T]
StaticCallOutputTypes(#F, #..T);         // static types
```

When you need the output types of a call at compile time, use `StaticCallOutputTypes`. It resolves which overload of `F` matches `..T` and returns the output types as a multiple-value list. It is a compile error if no matching overload exists.

The library alias `CallOutputTypes` (from `core.operators`) wraps this for both symbols and callable types.

### `StaticMono?`

```ceramic
[F]
StaticMono?(#F) : Bool;
```

`true` if symbol `F` has exactly one monomorphic overload (no pattern variables). The counterpart to `LambdaMono?` for symbols.

### `StaticMonoInputTypes`

```ceramic
[F when StaticMono?(F)]
StaticMonoInputTypes(#F);                // static types
```

For a symbol with exactly one monomorphic overload, the argument types of that overload are available at compile time via `StaticMonoInputTypes`. It is a compile error if `F` is not monomorphic.

### `MainModule`

```ceramic
MainModule() : module;
```

Every Ceramic program has a designated entry-point module. `MainModule()` returns the module object for it. Useful for writing module-generic test runners:

```ceramic
import test.module.(testMainModule);
main() = testMainModule();
```

### `StaticModule`

```ceramic
[S]
StaticModule(#S) : module;
```

To find which module owns a given symbol, call `StaticModule`. The result is the module object for the module that contains `S`. It is a compile error if `S` has no associated module.

### `ModuleName`

```ceramic
[S]
ModuleName(#S);                           // static string
```

The fully-qualified module name for the module that contains `S` is available at compile time as a static string. If `S` is itself a module, you get that module's own name. It is a compile error if `S` is not a symbol.

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

To enumerate the public globals of a module, use `ModuleMemberNames`. It returns every public global in `M` as a multiple-value list of static strings, in alphabetical order. `M` must be a module object, for example one obtained from `MainModule()` or `StaticModule(S)`.

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

The name of any compile-time value is available as a static string via `StaticName`. What the string contains depends on what `x` is:

- Symbol: its name (without module, with parameters).
- Static string: its string value.
- Numeric value: its decimal representation.
- Tuple: comma-delimited inside square brackets (`[a, b, c]`).

### `GetOverload`

```ceramic
[F, ..T]
GetOverload(#F, #..T);
```

When you want to capture a specific overload of a symbol as a callable value, use `GetOverload`. It selects the overload of `F` that matches argument types `..T` and returns it as a new callable procedure. You can call the result just like any other function. Unlike `makeCodePointer`, the result is still a fully generic Ceramic callable rather than a fixed function pointer.

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

To look up a public global by a name that is only known at compile time, use `staticFieldRef`. The result is the global's value, exactly as if you had written the name directly in code. It is a compile error if `name` is not a public member of `M`.

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

The length in bytes of static string `S` is available at compile time via `stringLiteralByteSize`.

### `stringLiteralConcat`

```ceramic
[..SS when allValues?(StringLiteral?, ..SS)]
stringLiteralConcat(#..SS);
```

To join several static strings into one at compile time, use `stringLiteralConcat`. It concatenates all its arguments in order and returns the result as a new static string.

### `stringLiteralByteSlice`

```ceramic
[S, n, m when
    StringLiteral?(S)
    and n >= 0 and n < stringLiteralByteSize(S)
    and m >= 0 and m < stringLiteralByteSize(S)
]
stringLiteralByteSlice(#S, #n, #m);
```

To extract a substring at compile time, use `stringLiteralByteSlice`. It returns the bytes of `S` from index `n` up to but not including `m`.

### `stringLiteralByteIndex`

```ceramic
[S, n when StringLiteral?(S) and n >= 0 and n < stringLiteralByteSize(S)]
stringLiteralByteIndex(#S, #n) : Int32;
```

The byte at position `n` of `S` is available at compile time as an `Int32`.

### `stringLiteralBytes`

```ceramic
[S when StringLiteral?(S)]
stringLiteralBytes(#S) : ..Int32;
```

To iterate over the bytes of a static string at compile time, use `stringLiteralBytes`. It returns the contents of `S` as a multiple-value list of `Int32` values, one per byte, in order.

### `stringLiteralFromBytes`

```ceramic
[..bytes]
stringLiteralFromBytes(#..bytes);         // static string
```

When you need to construct a static string from individual bytes at compile time, `stringLiteralFromBytes` assembles a list of static integer arguments (each in `0 .. 255`) into a static string.

### `stringTableConstant`

```ceramic
[S when StringLiteral?(S)]
stringTableConstant(#S) : Pointer[SizeT];
```

Static strings only exist at compile time. To access one from running code, use `stringTableConstant`. It returns a pointer into the program's string table, where the data is laid out as a `SizeT` length prefix followed by the string's bytes. In practice you won't call this directly — `StringLiteralRef` does it for you.

## Type Introspection

### `TypeSize`

```ceramic
[T when Type?(T)]
TypeSize(#T) : SizeT;
```

The size in bytes of a value of type `T` is available at compile time via `TypeSize`.

### `TypeAlignment`

```ceramic
[T when Type?(T)]
TypeAlignment(#T) : SizeT;
```

The natural alignment of type `T`, in bytes, is available at compile time via `TypeAlignment`.

### `BaseType`

```ceramic
[T when Type?(T)]
BaseType(#T);                           // static type
```

For a newtype, the underlying representation type is available via `BaseType`. For any other type, `BaseType(T)` is just `T` itself.

### `TupleElementCount`

```ceramic
[..T]
TupleElementCount(#Tuple[..T]) : SizeT;
```

The number of elements in a tuple type is available at compile time via `TupleElementCount`.

### `UnionMemberCount`

```ceramic
[..T]
UnionMemberCount(#Union[..T]) : SizeT;
```

The number of member types in a union is available at compile time via `UnionMemberCount`.

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
- `RecordFieldCount`: the number of fields in record type `R`.
- `RecordFieldName`: the name of the `n`th field as a static string.
- `RecordWithField?`: `true` if `R` has a field named `name`.

### Variant Introspection

```ceramic
[V]
Variant?(#V) : Bool;

[V when Variant?(V)]
VariantMemberCount(#V) : SizeT;

[V, M when Variant?(V)]
VariantMemberIndex(#V, #M) : SizeT;

[V when Variant?(V)]
VariantMembers(#V);                      // static types
```

- `Variant?`: `true` if `V` names a variant type.
- `VariantMemberCount`: the number of instance types.
- `VariantMemberIndex`: the ordinal index of instance type `M` within `V`. Each instance maps to a distinct index in `0 .. VariantMemberCount(V)`; the mapping is unspecified but stable.
- `VariantMembers`: a multiple-value list of the instance types of `V`, in index order.

### Enum Introspection

```ceramic
[E]
Enum?(#E) : Bool;

[E when Enum?(E)]
EnumMemberCount(#E) : SizeT;

[E, n when Enum?(E) and n >= 0 and n < EnumMemberCount(E)]
EnumMemberName(#E, #n);                  // static string
```

- `Enum?`: `true` if `E` names an enum type.
- `EnumMemberCount`: the number of values.
- `EnumMemberName`: the name of the `n`th value as a static string.

## Lambda Introspection

Compile-time predicates over lambda types. A **lambda record** is the anonymous record type created when a lambda expression captures variables. A **lambda symbol** is a non-capturing lambda equivalent to a named function.

```ceramic
[F]
LambdaRecord?(#F) : Bool;

[F]
LambdaSymbol?(#F) : Bool;

[F]
LambdaMono?(#F) : Bool;

[F when LambdaMono?(F)]
LambdaMonoInputTypes(#F);                // static types
```

- `LambdaRecord?`: `true` if `F` is the type of a capturing lambda.
- `LambdaSymbol?`: `true` if `F` is a procedure symbol created from a non-capturing lambda.
- `LambdaMono?`: `true` if the lambda record type `F` is monomorphic (its single overload has no pattern variables).
- `LambdaMonoInputTypes`: the argument types of the monomorphic overload of lambda record type `F`, as a multiple-value list.
