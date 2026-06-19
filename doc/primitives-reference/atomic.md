# Atomic Memory Operations

When multiple threads access the same memory, you need a way to make sure they do not interfere with each other. Atomic operations solve this by making a read, write, or update happen as a single uninterruptible step. No other thread can observe the memory in a halfway state.

None of these primitives may be overloaded. They cannot be used in the compile-time evaluator.

## Memory Order Symbols

```ceramic
define OrderUnordered;
define OrderMonotonic;
define OrderAcquire;
define OrderRelease;
define OrderAcqRel;
define OrderSeqCst;
```

Every atomic operation takes a memory order as a `static` parameter. Memory ordering controls how the compiler and CPU are allowed to reorder memory accesses around the atomic operation. Stronger orders give stronger guarantees but may be slower on some architectures.

| Ceramic          | C++11                  | Description                                           |
| ---------------- | ---------------------- | ----------------------------------------------------- |
| `OrderUnordered` | (none)                 | No ordering guarantees. Rarely useful.                |
| `OrderMonotonic` | `memory_order_relaxed` | Atomic, but no ordering relative to other operations  |
| `OrderAcquire`   | `memory_order_acquire` | Operations after this load cannot move before it      |
| `OrderRelease`   | `memory_order_release` | Operations before this store cannot move after it     |
| `OrderAcqRel`    | `memory_order_acq_rel` | Both acquire and release                              |
| `OrderSeqCst`    | `memory_order_seq_cst` | Fully sequentially consistent. The strongest option.  |

For a deeper explanation of memory ordering, see the [LLVM Atomic Instructions and Concurrency Guide](http://llvm.org/docs/Atomics.html).

## `atomicLoad`

```ceramic
[Order, T]
atomicLoad(#Order, p:Pointer[T]) : T;
```

An atomic load reads the value at `p` as a single uninterruptible step, so no other thread can write to it mid-read. You always get a complete, consistent value. Errors at compile time if the target platform does not support atomic loads of `T`.

## `atomicStore`

```ceramic
[Order, T]
atomicStore(#Order, p:Pointer[T], value:T) :;
```

An atomic store writes `value` to `p` as a single uninterruptible step, so no other thread can observe a partial write. Errors at compile time if the target platform does not support atomic stores of `T`.

## `atomicRMW`

```ceramic
define RMWXchg;
define RMWAdd;     define RMWSubtract;
define RMWAnd;     define RMWNAnd;
define RMWOr;      define RMWXor;
define RMWMin;     define RMWMax;
define RMWUMin;    define RMWUMax;

[Order, Op, T]
atomicRMW(#Order, #Op, p:Pointer[T], operand:T) : T;
```

Atomically reads the value at `p`, applies an operation to it using `operand`, writes the result back, and returns the original value before the update. The entire sequence is one uninterruptible step.

The `Op` parameter selects the operation:

| `Op`                           | Effect on `p^`                    | Constraints |
| ------------------------------ | --------------------------------- | ----------- |
| `RMWXchg`                      | replaced with `operand`           | any `T`     |
| `RMWAdd` / `RMWSubtract`       | arithmetic add/subtract           | integer `T` |
| `RMWMin` / `RMWMax`            | signed minimum/maximum            | integer `T` |
| `RMWUMin` / `RMWUMax`          | unsigned minimum/maximum          | integer `T` |
| `RMWAnd` / `RMWOr` / `RMWXor` | bitwise and/or/xor                | any `T`     |
| `RMWNAnd`                      | bitwise NAND (`~(p^ & operand)`)  | any `T`     |

Errors at compile time if the target platform does not support the chosen `Op` for `T`.

## `atomicCompareExchange`

```ceramic
[Order, T]
atomicCompareExchange(#Order, p:Pointer[T], old:T, new:T) : T;
```

Atomically checks whether the value at `p` equals `old`. If it does, `new` is written to `p` and `old` is returned. If it does not match, nothing is written and the current value at `p` is returned. The comparison and conditional write happen as one uninterruptible step.

This is the classic compare-and-swap operation. It is the building block for lock-free algorithms: you read a value, compute an updated version, then use `atomicCompareExchange` to write it back only if nothing changed since you read it. If the swap fails, you retry.

Errors at compile time if the target platform does not support compare-and-swap for `T`.

## `atomicFence`

```ceramic
[Order]
atomicFence(#Order);
```

Establishes a memory ordering constraint without reading or writing any particular value. It prevents the compiler and CPU from reordering memory accesses across this point. `Order` must be `OrderAcquire`, `OrderRelease`, `OrderAcqRel`, or `OrderSeqCst`.
