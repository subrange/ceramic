# Statements

Statements form the basic unit of control flow within function bodies.

### Blocks

A **block** groups statements and introduces a new scope for local variables. Statements execute sequentially unless modified by control flow.

```ceramic
main() {
    println("VENI");
    println("VIDI");
    println("VICI");
}
```
Blocks may also contain labels for [`goto`](#goto) targets: a label is an identifier followed by `:`.

### Expression Statements

An expression followed by `;`. Return values are discarded via the `destroy` operator. Reference returns are simply dropped: the referenced value is not destroyed.

```ceramic
main() {
    1 + 2;         // computed, then discarded
    println("Hi");
}
```
If a call's final argument is a block lambda, the trailing `;` may be omitted:

```ceramic
maybe(maybeMode): mode -> {
    println(mode.name, " mode selected");
} :: () -> {
    println("Please select a mode");
}
```

### Return Statements

Ends the current function and provides its return values.

```ceramic
foo(x, y) {
    return x + y;
}

// Shorthand: a single return as the whole body:
foo(x, y) = x + y;
```
Multiple `return` statements are allowed. All must return the same types. Code after all return paths are covered is a compile-time error.

#### Return by Reference

`return ref` returns lvalue references. All returned values must be lvalues, and all `return` statements in the function must agree on `ref`-ness.

```ceramic
[T]
overload index(pv:PitchedVector[T], n) {
    return ref pv.vec[n*pv.pitch];
}
```
`return forward` generalizes this: each value is returned by reference if it is an lvalue, by value otherwise.

### Local Variable Bindings

Local variables are introduced with a binding statement. There are four kinds:

#### `var`: new independent value

```ceramic
var x = 1;
var y = 2;
```
`var`s are destroyed at the end of their enclosing block on any exit path: normal flow, `return`, `break`, `continue`, `goto`, or exception.

#### `ref`: reference to an existing lvalue

```ceramic
var x = 1;
ref y = x;
y = 3;
println(x);  // 3; same underlying value
```
A `ref` does not affect the lifetime of the bound value. Accessing a `ref` after the underlying value is freed is undefined behavior.

#### `forward`: generic over lvalue/rvalue

Behaves like `ref` if the bound value is an lvalue, and like `var` if it is an rvalue.

```ceramic
forward x2, y2 = xs[2], ys[2];
// xs[2] is an rvalue → x2 is a var
// ys[2] is an lvalue → y2 is a ref
```

#### `alias`: call-by-name binding

Like alias functions: the name expands to the bound expression, re-evaluated in the original lexical context each time it is referenced.

---

Multiple variables can be bound from a multiple-value expression:

```ceramic
var x, y = 1, 2;
var a, b, c, d = 1, ..twoAndThree(), 4;

// A lone multi-value expression on the right is automatically unpacked:
var x, y = oneAndTwo();
```
Variable names come into scope after the right-hand side is evaluated, so a name may be shadowed by its own value:

```ceramic
var x = 1;
var x = x + 1;   // right side sees the outer x; result: x = 2
```
Binding statements must appear inside a block. They cannot be the single-statement body of `if`, `while`, or other compound statements.

### Assignment Statements

Assignment is a **statement** in Ceramic, not an expression. Using `=` in an expression context is a syntax error.

```ceramic
var x = 1;
x = 2;     // desugars to: assign(x, 2)
```
Multiple-value assignment evaluates the entire right-hand side into temporaries first, then assigns each. This makes shuffles safe:

```ceramic
x, y = y, x;  // safe swap: no aliasing issues
```
Special property assignment desugars differently when the left-hand side is an index, static-index, or field reference:

```ceramic
a[..b]  = c;     // → indexAssign(a, ..b, c)
a.0     = c;     // → staticIndexAssign(a, #0, c)
a.field = c;     // → fieldRefAssign(a, #"field", c)
```

#### Update Assignment

An operator symbol followed by `:` is an update assignment. The arithmetic forms `+:`, `-:`, `*:`, `/:`, `%:` desugar to calls to `updateAssign`:

```ceramic
x +: 1;   // updateAssign(add, x, 1)
x -: 2;   // updateAssign(subtract, x, 2)
x *: 4;   // updateAssign(multiply, x, 4)
```
Property update forms also exist:

```ceramic
a[..b]  +: c;   // → indexUpdateAssign(add, a, ..b, c)
a.field +: c;   // → fieldRefUpdateAssign(add, a, #"field", c)
```

<!-- TODO: document that updateAssign can be overloaded for user-defined ops, e.g. overload updateAssign(#(**), ref x, exp) { ... } -->


#### Initialization Statements

Use `<--` to initialize **uninitialized** storage (from a raw allocator, a named return value, etc.). Unlike `=`, it assumes the destination has no prior state.

```ceramic
var p = allocateRawMemory(Foo);
finally freeRawMemory(p);

p^ <-- Foo();
```
If the right side is a function call that returns by value, the return value is written directly into the destination. If the right side is an lvalue, `copy` is called. If it is a `forward`-bound rvalue, `move` is called.

Initializing an already-initialized value with `<--`, or assigning an uninitialized value with `=`, is undefined behavior.

### Conditional Statements

#### `if`

Executes a branch based on a `Bool` expression.

```ceramic
if (asFoghornLeghorn?)
    print(", I say, that");

if (condition)
    thenBranch();
else
    elseBranch();
```

#### `switch`

Dispatches to the first matching `case` clause using the `case?` operator. If no case matches, the `else` clause runs if present. Unlike C, there is no fall-through between cases.

```ceramic
switch (card.rank)
case (1)                        printTo(stream, "Ace");
case (2)                        printTo(stream, "Deuce");
case (3, 4, 5, 6, 7, 8, 9, 10) printTo(stream, card.rank);
case (11)                       printTo(stream, "Jack");
case (12)                       printTo(stream, "Queen");
case (13)                       printTo(stream, "King");
else                            assert(false);
```

### Loop Statements

#### `while`

Loops while a `Bool` expression is true.

```ceramic
var x = 0;
while (x < 10) {
    println(x);
    x += 1;
}
```

#### `for`

Iterates over a sequence using the `iterator`, `hasNext?`, and `next` operator functions.

```ceramic
for (x in range(10))
    println(x);

// desugars to:
{
    forward _iter = iterator(range(10));
    while (hasNext?(_iter)) {
        forward x = next(_iter);
        println(x);
    }
}
```

#### `..for`: Multiple-Value For

Unrolls over each value of a multiple-value expression at **compile time**. The loop variable's type may differ between iterations.

```ceramic
[..TT | countValues(..TT) != 1]
overload printTo(stream, ..xs:TT) {
    ..for (x in xs)
        printTo(stream, x);
}
```
`..for` is not a runtime loop. The body is instantiated once per value at compile time, like template unrolling.

### Branch Statements

#### `break` and `continue`

`break` exits the innermost loop. `continue` skips to the next iteration. Both are invalid outside a loop.

#### `goto`

Jumps to a label within the current function.

```ceramic
main() {
second_verse:
    println("I'm Henry VIII I am");
    goto second_verse;
}
```
There are two restrictions. A `goto` cannot jump into a `var` binding's scope from outside it, since that would skip initialization. Jumping from an outer block into a label inside an inner block is also unsupported.

### Exception Handling Statements

Exception handling is optional. The `ExceptionsEnabled?` alias in `__primitives__` reports whether it is active for the current compilation. The compile-time evaluator always behaves as if exceptions are disabled.

#### `throw`

Throws an exception by calling `throwValue` with the given value. Unwinds the call stack to the nearest matching `catch` clause.

```ceramic
safeDivide(x:Int, y:Int) {
    if (y == 0)
        throw DivisionByZero();
    return x/y;
}
```

#### `try` / `catch`

Executes the `try` block and, if an exception is thrown, tests it against each `catch` clause in order. A clause without a type is a catch-all. If no clause matches, the exception is rethrown to the next enclosing scope. A caught exception may be rethrown by re-throwing the bound exception object.

```ceramic
try {
    var file = File("hello.txt", CREATE);
    printlnTo(file, "hello world");
}
catch (ex:IOError) {
    printlnTo(stderr, "Unable to open hello.txt: ", ex);
}
catch (ex) {
    printlnTo(stderr, "Unexpected exception!");
    abort();
}
```
When exceptions are disabled, the `try` body runs as a plain block and all `catch` clauses are ignored.

#### `finally` and `onerror`

**`finally`**: runs when the enclosing block exits for *any* reason: normal flow, `return`, `break`, `continue`, `goto`, or exception.

```ceramic
var p = malloc(SizeT(128));
finally free(p);
```
**`onerror`**: runs only when the enclosing block exits due to an exception. Normal exits do not trigger it.

```ceramic
overload SomeType(size:SizeT) {
    var p = malloc(size);
    onerror free(p);       // only if potentiallyFail() throws

    potentiallyFail();
    return SomeType(p);
}
```
When exceptions are disabled, `onerror` guards are silently ignored. `finally` guards continue to work normally.

### Eval Statements

`eval` parses and expands a compile-time string as Ceramic source, then executes it in place of the `eval` statement. The string must be a complete, parsable statement or sequence of statements. Partial constructs are not allowed.

```ceramic
main() {
    eval #"""var x = "hello world";""";
    eval #"""println(x);""";
}
```
[Eval expressions](expressions.md#eval-expressions) work the same way in expression context.
---
