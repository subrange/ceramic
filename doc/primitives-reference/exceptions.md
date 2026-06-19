# Exception Handling

Implementation hooks for the exception-handling runtime. Not overloadable.

## `activeException`

```ceramic
activeException() : Pointer[Int8];
```

`activeException` gives you a pointer to the exception object currently driving the unwinding process. It is only valid during unwinding itself, not inside a `catch` clause. This is an implementation detail used by the exception runtime and is not intended for user code.
