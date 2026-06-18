# Multiple Values

Compile-time operations over multiple-value lists. A multiple-value list is the sequence of values produced by a variadic argument, a comma expression, or another primitive. These primitives select, count, and slice such lists. All selection is resolved at compile time. None may be overloaded.

## `countValues`

```ceramic
[..xs]
countValues(..xs) : Int32;
```

The number of values in `..xs`, as an `Int32`.

## `nthValue`

```ceramic
[n, ..xs when n >= 0 and n < countValues(..xs)]
nthValue(#n, ..xs);
```

The `n`th value of `..xs`, zero-based.

## `withoutNthValue`

```ceramic
[n, ..xs when n >= 0 and n < countValues(..xs)]
withoutNthValue(#n, ..xs);
```

All values of `..xs` except the `n`th, in order.

## `takeValues` / `dropValues`

```ceramic
[n, ..xs when n >= 0]
takeValues(#n, ..xs);
dropValues(#n, ..xs);
```

`takeValues` returns the first `n` values of `..xs`. `dropValues` returns the rest. `n` is clamped to the list length.

## `integers`

```ceramic
[n when n >= 0]
integers(#n);
```

Runtime integers from `0` to `n - 1`, typed as `Int32` or `SizeT` to match `n`. Returns no values when `n` is `0`.

Typical use with `..for`:

```ceramic
..for (i in integers(#5))
    a[i] = x;
```

## `staticIntegers`

```ceramic
[n when n >= 0]
staticIntegers(#n);
```

Like `integers`, but each value is a static integer (`#0` through `#(n - 1)`).

Useful when the index itself must be a static value (e.g., for `tupleRef` or `staticIndex`):

```ceramic
..for (i in staticIntegers(#3))
    println(tupleRef(t, i));
```
