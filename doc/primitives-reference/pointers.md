# Pointer Operations

Primitives for creating, dereferencing, and converting pointers, plus for constructing and calling function pointers. None of these may be overloaded. Pointer comparison is provided by the standard library through the `equals?` and `lesser?` operators.

## `addressOf`

```ceramic
[T]
addressOf(ref x:T) : Pointer[T];
```

The address of an lvalue `x` is available as a `Pointer[T]` via `addressOf`. `x` must be a variable, a field, or a pointer dereference. This is the same as the prefix `@` operator.

## `pointerDereference`

```ceramic
[T]
pointerDereference(p:Pointer[T]) : ref T;
```

To dereference a pointer, use `pointerDereference`. It returns a reference to the value that `p` points to. This is the same as the `^` operator.

## `pointerOffset`

```ceramic
[T, I when Integer?(I)]
pointerOffset(p:Pointer[T], i:I) : Pointer[T];
```

To move a pointer forward or backward by some number of elements, use `pointerOffset`. Each position is `TypeSize(T)` bytes. Negative values move backward. This does not bounds-check.

## `pointerToInt`

```ceramic
[T, I when Integer?(I)]
pointerToInt(#I, p:Pointer[T]) : I;
```

`pointerToInt` converts the address stored in `p` to integer type `I`. If `I` is wider than a pointer, the value is zero-extended. If narrower, it is truncated.

## `intToPointer`

```ceramic
[T, I when Integer?(I)]
intToPointer(#T, address:I) : Pointer[T];
```

`intToPointer` converts an integer `address` to a `Pointer[T]`. If `I` is wider than a pointer, the address is truncated. If narrower, it is zero-extended.

## `nullPointer`

```ceramic
[T]
nullPointer(#T) : T;
```

The null value for any pointer-like type is available via `nullPointer`. Works with `Pointer`, `CodePointer`, and `ExternalCodePointer` types.

## `bitcast`

```ceramic
[T, U]
bitcast(#T, x:U) : ref T;
```

`bitcast` reinterprets the bytes of `x` as a value of type `T` without any conversion. It gives you back a reference that aliases the same memory as `x`. `T` must be no larger than `U` in bytes and must not require stricter alignment.

This works between pointer types, between code-pointer types, and between data and code pointers.

## `memcpy` / `memmove`

```ceramic
[T, U, I when Integer?(I)]
memcpy(dest:Pointer[T], src:Pointer[U], n:I) :;
memmove(dest:Pointer[T], src:Pointer[U], n:I) :;
```

Both `memcpy` and `memmove` copy `n` bytes from `src` to `dest`. The difference is how they handle overlapping regions: `memcpy` requires the source and destination not to overlap, while `memmove` handles overlap correctly.

## Function Pointer Operations

### `makeCodePointer`

```ceramic
[F, ..T]
makeCodePointer(#F, #..T) : CodePointer[[..T], [..CallOutputTypes(F, ..T)]];
```

`makeCodePointer` picks the overload of symbol `F` that matches input types `..T`, compiles it to a concrete function instance, and gives you a [`CodePointer`](types.md#codepointer) to it.

`F` must be a symbol or a non-capturing lambda. If no overload of `F` matches the given types, you get a compile error. Arguments are always matched as lvalues.

### `makeExternalCodePointer`

```ceramic
[F, CC, V?, ..T]
makeExternalCodePointer(#F, #CC, #V?, #..T)
    : ExternalCodePointer[CC, V?, [..T], [..CallOutputTypes(F, ..T)]];
```

`makeExternalCodePointer` works like `makeCodePointer`, but also generates a thunk that adapts the matched overload to a foreign calling convention `CC` (such as `cdecl` or `stdcall`), and gives you an [`ExternalCodePointer`](types.md#external-code-pointer-types). Set `V?` to `true` to mark the pointer variadic. The `makeCCodePointer` library alias chooses `cdecl` and non-variadic for you.

The matched overload must be C-compatible: it returns zero or one values, and none of its argument types have non-trivial `copy`, `move`, or `destroy` operations.

If a Ceramic exception escapes the pointed-to function, `unhandledExceptionInExternal` is called, the same as for `external` functions.

### `callExternalCodePointer`

```ceramic
[CC, V?, ..In, ..Out]
callExternalCodePointer(f:ExternalCodePointer[CC, V?, [..In], [..Out]], ..args:In) : ..Out;
```

`callExternalCodePointer` invokes an external function pointer using its declared calling convention. Variadic pointers (where `V?` is `true`) also accept trailing variadic arguments beyond the declared parameter list.
