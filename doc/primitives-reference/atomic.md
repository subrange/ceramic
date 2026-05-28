# Atomic Memory Operations

Uninterruptible, lock-free memory access and synchronization. None may be overloaded.

The compile-time evaluator does not support atomic operations. Evaluating one raises an error.

## Memory Order Symbols

```ceramic
define OrderUnordered;
define OrderMonotonic;
define OrderAcquire;
define OrderRelease;
define OrderAcqRel;
define OrderSeqCst;
```

Every atomic operation takes one of these as a `static` parameter. They correspond to LLVM orderings, which are a superset of the C11/C++11 memory model. See the [LLVM Atomic Instructions and Concurrency Guide](http://llvm.org/docs/Atomics.html).

| Ceramic          | LLVM        | C++11                  |
| ---------------- | ----------- | ---------------------- |
| `OrderUnordered` | `unordered` | (none)                 |
| `OrderMonotonic` | `monotonic` | `memory_order_relaxed` |
| `OrderAcquire`   | `acquire`   | `memory_order_acquire` |
| `OrderRelease`   | `release`   | `memory_order_release` |
| `OrderAcqRel`    | `acq_rel`   | `memory_order_acq_rel` |
| `OrderSeqCst`    | `seq_cst`   | `memory_order_seq_cst` |

## `atomicLoad`

```ceramic
[Order, T | Order?(Order)]
atomicLoad(#Order, p:Pointer[T]) : T;
```

Atomically loads the value at `p`. Bitwise-copied. Errors if the target does not support atomic loads of `T`. Lowers to LLVM `load atomic`.

## `atomicStore`

```ceramic
[Order, T | Order?(Order)]
atomicStore(#Order, p:Pointer[T], value:T) :;
```

Atomically stores `value` at `p`. Bitwise-copied. Errors if the target does not support atomic stores of `T`. Lowers to LLVM `store atomic`.

## `atomicRMW`

```ceramic
define RMWXchg;
define RMWAdd;     define RMWSubtract;
define RMWAnd;     define RMWNAnd;
define RMWOr;      define RMWXor;
define RMWMin;     define RMWMax;
define RMWUMin;    define RMWUMax;

[Order, Op, T | Order?(Order) and RMWOp?(Op)]
atomicRMW(#Order, #Op, p:Pointer[T], operand:T) : T;
```

Atomic read-modify-write. Returns the value at `p` **before** the update. Errors if the target does not atomically support `Op` for `T`. Lowers to LLVM `atomicrmw`.

The update semantics for each `Op`:

| `Op`                          | Effect on `p^`                      | Constraints |
| ----------------------------- | ----------------------------------- | ----------- |
| `RMWXchg`                     | written to `operand` (bitwise copy) | any `T`     |
| `RMWAdd` / `RMWSubtract`      | arithmetic add/subtract             | integer `T` |
| `RMWMin` / `RMWMax`           | signed min/max                      | integer `T` |
| `RMWUMin` / `RMWUMax`         | unsigned min/max                    | integer `T` |
| `RMWAnd` / `RMWOr` / `RMWXor` | bitwise and/or/xor                  | any `T`     |
| `RMWNAnd`                     | bitwise NAND (`~(p^ & operand)`)    | any `T`     |

## `atomicCompareExchange`

```ceramic
[Order, T | Order?(Order)]
atomicCompareExchange(#Order, p:Pointer[T], old:T, new:T) : T;
```

Atomic compare-and-swap. If `p^` is bitwise equal to `old`, `new` is written to `p` and `old` is returned. Otherwise, `p^` is unchanged and its current value is returned. Errors if the target does not support CAS for `T`. Lowers to LLVM `cmpxchg`.

## `atomicFence`

```ceramic
[Order | Order?(Order)]
atomicFence(#Order);
```

Introduces a happens-before edge without an associated memory operation. `Order` must be `OrderAcquire`, `OrderRelease`, `OrderAcqRel`, or `OrderSeqCst`. Lowers to LLVM `fence`.
