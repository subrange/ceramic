# Pointer Operations

Create, dereference, compare, and convert pointers, plus function-pointer construction and invocation. None of these may be overloaded.

## `addressOf`

```ceramic
[T]
addressOf(ref x:T) : Pointer[T];
```

Returns the address of `x`. `x` must be an lvalue. Equivalent to the prefix `&` operator.

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
[T, I | Integer?(I)]
pointerToInt(static I, p:Pointer[T]) : I;
```

Converts the address of `p` to integer type `I`. Zero-extends if `I` is wider than a pointer, truncates if narrower. Lowers to LLVM `ptrtoint`.

## `intToPointer`

```ceramic
[T, I | Integer?(I)]
intToPointer(static T, address:I) : Pointer[T];
```

Converts `address` to a `Pointer[T]`. Truncates if `I` is wider than a pointer, zero-extends if narrower. Lowers to LLVM `inttoptr`.

## `pointerCast`

```ceramic
[P1, P2 | Pointer?(P1) and Pointer?(P2)]
pointerCast(static P1, p:P2) : P1;
```

Converts `p` to another pointer type sharing the same address. Lowers to LLVM `bitcast`.

Works between data pointers (`Pointer[T]` ↔ `Pointer[U]`), between code-pointer types (`CodePointer`, `CCodePointer`, …), and between data and code pointers.

## Function Pointer Operations

### `makeCodePointer`

```ceramic
[F, ..T]
makeCodePointer(static F, static ..T) : CodePointer[[..T], [..CallType(F, ..T)]];
```

Resolves an overload of `F` matching input types `..T`, instantiates it, and returns a [`CodePointer`](types.md#codepointer) to that instance.

- `F` must be a symbol or a non-capturing lambda (equivalent to a symbol).
- Errors if `F` is not a symbol, or if no overload matches.
- Always matches as if inputs are lvalues. Taking `CodePointer`s to rvalue functions is unsupported.

### `makeCCodePointer`

```ceramic
[F, ..T]
makeCCodePointer(static F, static ..T) : CCodePointer[[..T], [..CallType(F, ..T)]];
```

Like `makeCodePointer`, but additionally generates a thunk that adapts the matched overload to the C calling convention, and returns a [`CCodePointer`](types.md#external-code-pointer-types).

The matched overload must be **C-compatible**:

- Returns zero or one values.
- No arguments with nontrivial `copy`, `move`, or `destroy` operations.

If a Ceramic exception escapes the pointed-to overload, the `unhandledExceptionInExternal` operator function is called (same as for external functions).

### `callCCodePointer`

```ceramic
define callCCodePointer;

[..In, ..Out]
overload callCCodePointer(f:CCodePointer[[..In], [..Out]],          ..args:In)              : ..Out;
[..In, ..Out]
overload callCCodePointer(f:VarArgsCCodePointer[[..In], [..Out]],   ..args:In, ..varArgs)   : ..Out;
[..In, ..Out]
overload callCCodePointer(f:LLVMCodePointer[[..In], [..Out]],       ..args:In)              : ..Out;
// and so on for StdCallCodePointer, FastCallCodePointer, ThisCallCodePointer.
```

Invokes an external function pointer using the appropriate calling convention.
