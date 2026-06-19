# Multiple Values

Compile-time operations over multiple-value lists. A multiple-value list is the sequence of values produced by a variadic argument, a comma expression, or another primitive. These primitives let you select, count, and slice such lists at compile time. None may be overloaded.

## `countValues`

```ceramic
[..xs]
countValues(..xs) : Int32;
```

The number of values in `..xs` is available as an `Int32` via `countValues`.

## `nthValue`

```ceramic
[n, ..xs when n >= 0 and n < countValues(..xs)]
nthValue(#n, ..xs);
```

To pick a single value out of a list by position, use `nthValue`. Positions are zero-based.

## `withoutNthValue`

```ceramic
[n, ..xs when n >= 0 and n < countValues(..xs)]
withoutNthValue(#n, ..xs);
```

To drop one value from a list by position, use `withoutNthValue`. It returns all values of `..xs` except the one at position `n`, in order.

## `takeValues` / `dropValues`

```ceramic
[n, ..xs when n >= 0]
takeValues(#n, ..xs);
dropValues(#n, ..xs);
```

`takeValues` returns the first `n` values of `..xs`. `dropValues` returns everything after the first `n`. Both clamp `n` to the list length, so asking for more values than exist is not an error.

## `integers`

```ceramic
[n when n >= 0]
integers(#n);
```

`integers` produces runtime integer values from `0` to `n - 1`, typed as `Int32` or `SizeT` to match the type of `n`. When `n` is `0`, it produces no values.

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

`staticIntegers` works like `integers`, but each value is a static integer (`#0` through `#(n - 1)`).

Use this when the index itself needs to be a static value, for example with `tupleRef` or `staticIndex`:

```ceramic
..for (i in staticIntegers(#3))
    println(tupleRef(t, i));
```
