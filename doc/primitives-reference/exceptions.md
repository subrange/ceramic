# Exception Handling

Implementation hooks for the exception-handling runtime. Not overloadable.

## `activeException`

```ceramic
activeException() : Pointer[Int8];
```

Returns a pointer to the exception currently driving unwinding. Valid only during unwinding itself, not inside `catch` clauses. Implementation detail, not for user code.
