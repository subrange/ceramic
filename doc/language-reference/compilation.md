# Compilation Strategy

Ceramic uses **whole-program compilation**. Starting from an entry-point source file, the compiler:

1. Loads all imported modules recursively.
2. Populates each module's namespace, enabling free forward and circular references.
3. Establishes entry points: a public `main` symbol and any `external` function definitions.
4. Compiles only definitions reachable from those entry points. Unreachable definitions are never visited after parsing.

### Entry Points

If the entry-point module contains a public symbol named `main`, it is passed to the `callMain` operator function. `callMain` is responsible for calling `main` with its command-line arguments. The instantiated `callMain(#main)` becomes the program's entry point and corresponds to the C ABI `main` symbol.

`external` function definitions also become entry points and are emitted with C linkage.

#### Command-Line Arguments

The signature of `main` determines what arguments it receives. Three forms are supported:

```ceramic
// no arguments
main() { ... }
```

```ceramic
// receives all arguments as a sequence of strings, including argv[0]
main(args) {
    println(size(args));   // number of arguments
    println(args[0]);      // program name
    println(args[1]);      // first user argument
}
```

```ceramic
// receives raw C-style argc and argv
main(argc, argv) { ... }
```

### Compile-Time Evaluation

The compiler includes an evaluator that runs certain things at compile time:

- Pattern guard predicates
- Parameters of parameterized symbols
- Operands of `#` static expressions and `eval` statements and expressions
- Declared return types in function definitions
- Declared instance types in variant definitions
- Computed record layouts

The evaluator matches runtime semantics with these restrictions. It cannot call `external` functions or `__llvm__` functions. It does not support exception handling and always behaves as if exceptions are disabled. It does not call `destroy`. It does not initialize global variables.

### Pattern Matching

Ceramic uses unification-based pattern matching to select overloads and bind variant instances. A pattern may be a literal, a named symbol, a free pattern variable, or a symbol with a parameter suffix. The match fails if structure doesn't match, or if a pattern variable is bound to two different values.

```ceramic
define foo;
define bar;

define pattern;
[T]
overload pattern(#T) { println("a"); }
overload pattern(#bar) { println("b"); }

testPattern() {
    pattern(foo); // prints a
    pattern(bar); // prints b
}
```
Multiple-value patterns may end with `..name`: a variadic variable that greedily matches all remaining values.

---
