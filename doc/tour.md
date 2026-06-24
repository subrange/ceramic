# A Tour of Ceramic

Ceramic is a systems language built for generic programming. This tour covers enough so you can start writing Ceramic code and follow the [Language Reference](language-reference/index.md). It assumes prior programming experience in any language. Most examples are complete programs.

### Hello, World

The smallest Ceramic program is a `main` function:

```ceramic
import printer.(println);

main() {
    println("Hello, World!");
}
```

Save it as `hello.crm` and compile it to an executable:

```
ceramic hello.crm
./hello
```

Using `-run` skips the binary and runs immediately:

```sh
ceramic -run hello.crm
```

The compiler is built from source with CMake. Run `cmake -B build && cmake --build build` from the repository root to produce `build/compiler/ceramic`.

### Variables

A `var` creates a mutable local where types are inferred from the initializer or declared explicitly after a `:`.

```ceramic
import printer.(println);

main() {
    var x = 1;
    var y : Int = 2;
    x = 10;           // assignments are statements, not expressions
    println(x + y);   // 12
}
```

### Control Flow

Ceramic has `if`/`else`, `while`, and `for`. A `for` loop iterates over a sequence, most commonly a `range`.

```ceramic
import printer.(println);

main() {
    var sum = 0;
    for (i in range(5))
        sum +: i;         // 0 + 1 + 2 + 3 + 4

    if (sum > 5)
        println("big: ", sum);
    else
        println("small: ", sum);

    var n = 3;
    while (n > 0) {
        println(n);
        n -: 1;
    }
}
```

Ceramic also has `switch`, `break`, `continue`, and labelled `goto`. See
[Statements](language-reference/statements.md).

### Functions

A function is a name, an argument list, and a body. A `=` body is shorthand for a single `return`.

```ceramic
square(x) = x*x;

add(a, b) {
    return a + b;
}
```

The return types are inferred from the body, or declared after a `:`. The arguments are passed by references, so a function can change what the caller passes.

```ceramic
import printer.(println);

inc(x:Int) { x +: 1; }

main() {
    var x = 2;
    inc(x);
    println(x); // 3
}
```

Named returns, variadics, and reference qualifiers are in
[Function Definitions](language-reference/functions.md).
