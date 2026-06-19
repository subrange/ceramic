# Type Definitions

Ceramic has four kinds of user-defined types:

- [Records](#records): general-purpose aggregates
- [Variants](#variants): discriminated unions
- [Enumerations](#enumerations): named symbolic constants
- [Lambda types](#lambda-types): implicit capture types, created by lambda expressions

### Records

A **record** is a type that groups named fields together into a single value, similar to a struct in C or a class with only data members.

```ceramic
record Point (x:Int, y:Int);
```
Records may be parameterized. When no predicate is needed, the pattern guard is optional: unrecognized names in brackets are taken as unbounded pattern variables:

```ceramic
record Point[T] (x:T, y:T);          // [T] guard is implied

[T when Float?(T)]
record FloatPoint[T] (x:T, y:T);     // explicit predicate
```

#### Computed Layouts

A record's layout can be computed from an expression that evaluates to a list of `[fieldName, fieldType]` pairs:

```ceramic
// Equivalent to: record Point[T] (x:T, y:T)
record Point[T] = [#"x", T], [#"y", T];

// Custom coordinate names
record PointWithCoordinates[T, xy] = [xy.0, T], [xy.1, T];
```
This pattern also enables template specialization via an overloaded helper function:

```ceramic
record Vec3D[T] = ..Vec3DBody(T);

private define Vec3DBody;
[T when T != Double]
overload Vec3DBody(#T) = [#"coords", Array[T, 3]];
overload Vec3DBody(#Float) = [#"coords", Vec[Float, 4]];  // SIMD path
```

### Variants

A **variant** is a type that can hold a value of exactly one of several possible types. It always knows which type it currently holds, so you can dispatch on it safely at runtime without casts.

```ceramic
variant Fruit (Apple, Orange, Banana);
```
Variants may be parameterized (pattern guard optional when no predicate is needed):

```ceramic
variant Maybe[T] (Nothing, T);          // [T] implied
variant Either[T, U] (T, U);           // [T, U] implied

[C when Color?(C)]
variant Fruit[C] (Apple[C], Orange[C], Banana[C]);
```
The instance list may be any expression evaluated at compile time:

```ceramic
private RainbowTypes(Base) =
    Base[Red], Base[Orange], Base[Yellow], Base[Green],
    Base[Blue], Base[Indigo], Base[Violet];

variant Fruit (..RainbowTypes(Apple), ..RainbowTypes(Banana));
```

#### Extending Variants

Variants are **open**. New instance types are added with `instance`:

```ceramic
variant Exception;

record RangeError (lowerBound:Int, upperBound:Int, attemptedValue:Int);
record TypeError  (expectedTypeName:String, attemptedTypeName:String);
instance Exception (RangeError, TypeError);
```
`instance` binds to variants by pattern matching, so parameterized variants can be extended selectively:

```ceramic
[C when Color?(C)]
variant Fruit[C];

instance Fruit[Yellow] (Banana);          // only Yellow

[C when C == Red or C == Green]
instance Fruit[C] (Apple);               // Red and Green only

[C]
instance Fruit[C] (Berry[C]);            // all Fruit[C]
```
The pattern guard on `instance` is **not optional** for generic extension. Without it, `instance Variant[T]` attempts to match only the concrete type `Variant[T]` (where `T` must already be defined), not all parameterized instances.

### Enumerations

An **enumeration** defines a type whose values are one of a fixed set of named constants. The constant names are defined in the current module with the same visibility as the type.

```ceramic
enum ThreatLevel (Green, Blue, Yellow, Orange, Red, Midnight);

private enum SecurityLevel (
    Angel_0A, Archangel_1B,
    Principal_2C, Power_3D,
    Virtue_4E, Domination_5F,
    Throne_6G, Cherubic_7H,
    Seraphic_8X,
);
```
Enumerations cannot currently be parameterized and do not allow pattern guards.

### New Types

`newtype` declares a distinct type with the same memory representation as an existing type. The new type is **not** assignment-compatible with the original; only `bitcast` bridges them.

```ceramic
newtype Celsius = Float64;
newtype Fahrenheit = Float64;

// Type-safe: cannot accidentally add Celsius and Fahrenheit
main() {
    var boiling = bitcast(Celsius, Float64(100.0));
    println(Type(boiling));       // Celsius
    println(BaseType(#Celsius));  // Float64
    println(NewType?(Celsius));   // true
}
```

`BaseType` (from `__primitives__`) returns the underlying type. `NewType?` (from `core.types`) tests whether a type is a newtype. Both types have the same size and alignment; `bitcast` converts between them.

Newtypes cannot be parameterized (no pattern guard).

### Lambda Types

Lambda types are record-like types that capture values from their enclosing scope. They are created implicitly by the compiler when [lambda expressions](expressions.md#lambda-expressions) capture variables: there is no explicit syntax for defining them.

---
