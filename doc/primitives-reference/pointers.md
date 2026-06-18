# Pointer Operations

Create, dereference, compare, and convert pointers, plus function-pointer construction and invocation. None of these may be overloaded.

## `addressOf`

```ceramic
[T]
addressOf(ref x:T) : Pointer[T];
```

Returns the address of `x`. `x` must be an lvalue. Equivalent to the prefix `@` operator.

## `pointerDereference`

```ceramic
[T]
pointerDereference(p:Pointer[T]) : ref T;
```

Returns a reference to the object pointed to by `p`. Effectively a no-op at the LLVM level (references are pointers).

## `pointerEquals?` / `pointerLesser?`

```ceramic
[T, U]
pointerEquals?(p:Pointer[T], q:Pointer[U]) : Bool;
pointerLesser?(p:Pointer[T], q:Pointer[U]) : Bool;
```

- `pointerEquals?`: `true` if `p` and `q` hold the same address. Lowers to LLVM `icmp eq`.
- `pointerLesser?`: `true` if `p`'s address is numerically less than `q`'s. Lowers to LLVM `icmp lt`.

## `pointerOffset`

```ceramic
[T, I | Integer?(I)]
pointerOffset(p:Pointer[T], i:I) : Pointer[T];
```

Returns a pointer offset from `p` by `i * TypeSize(T)` bytes. Lowers to LLVM `getelementptr`.

## `pointerToInt`

```ceramic
[T, I when Integer?(I)]
pointerToInt(#I, p:Pointer[T]) : I;
```

Converts the address of `p` to integer type `I`. Zero-extends if `I` is wider than a pointer, truncates if narrower. Lowers to LLVM `ptrtoint`.

## `intToPointer`

```ceramic
[T, I when Integer?(I)]
intToPointer(#T, address:I) : Pointer[T];
```

Converts `address` to a `Pointer[T]`. Truncates if `I` is wider than a pointer, zero-extends if narrower. Lowers to LLVM `inttoptr`.

## `nullPointer`

```ceramic
[T]
nullPointer(#T) : T;
```

Returns the null value of pointer-like type `T` (`Pointer`, `CodePointer`, or `ExternalCodePointer`).

## `bitcast`

```ceramic
[T, U]
bitcast(#T, x:U) : ref T;
```

Reinterprets `x` as type `T`. Returns a reference (lvalue) aliasing the same memory as `x`. `T` must be no larger than the type of `x`, and may not have stricter alignment. Lowers to LLVM `bitcast`.

Works between many types, including data pointers (`Pointer[T]` ↔ `Pointer[U]`), code-pointer types (`CodePointer`, `CCodePointer`, …), and between data and code pointers.

## `memcpy` / `memmove`

```ceramic
[T, U, I when Integer?(I)]
memcpy(dest:Pointer[T], src:Pointer[U], n:I) :;
memmove(dest:Pointer[T], src:Pointer[U], n:I) :;
```

Copies `n` bytes from `src` to `dest`. `memcpy` requires the regions not to overlap; `memmove` handles overlap correctly. Lower to the LLVM `memcpy`/`memmove` intrinsics.

## Function Pointer Operations

### `makeCodePointer`

```ceramic
[F, ..T]
makeCodePointer(#F, #..T) : CodePointer[[..T], [..CallOutputTypes(F, ..T)]];
```

Resolves an overload of `F` matching input types `..T`, instantiates it, and returns a [`CodePointer`](types.md#codepointer) to that instance.

- `F` must be a symbol or a non-capturing lambda (equivalent to a symbol).
- Errors if `F` is not a symbol, or if no overload matches.
- Always matches as if inputs are lvalues. Taking `CodePointer`s to rvalue functions is unsupported.

### `makeExternalCodePointer`

```ceramic
[F, CC, V?, ..T]
makeExternalCodePointer(#F, #CC, #V?, #..T)
    : ExternalCodePointer[CC, V?, [..T], [..CallOutputTypes(F, ..T)]];
```

Like `makeCodePointer`, but additionally generates a thunk that adapts the matched overload to the calling convention `CC` (`cdecl`, `stdcall`, …), and returns an [`ExternalCodePointer`](types.md#external-code-pointer-types). `V?` marks the pointer variadic. The `makeCCodePointer` library alias supplies `cdecl` and non-variadic.

The matched overload must be **C-compatible**:

- Returns zero or one values.
- No arguments with nontrivial `copy`, `move`, or `destroy` operations.

If a Ceramic exception escapes the pointed-to overload, the `unhandledExceptionInExternal` operator function is called (same as for external functions).

### `callExternalCodePointer`

```ceramic
[CC, V?, ..In, ..Out]
callExternalCodePointer(f:ExternalCodePointer[CC, V?, [..In], [..Out]], ..args:In) : ..Out;
```

Invokes an external function pointer using its calling convention. Variadic pointers (`V?` is `true`) additionally accept trailing variadic arguments.
