# Expressions

Ceramic's expression hierarchy, from highest to lowest precedence:

| Level                 | Forms                                                | Operator functions                                    |
| --------------------- | ---------------------------------------------------- | ----------------------------------------------------- |
| Atomic                | names, literals, `()`, `[]`, `__FILE__` etc., `eval` | (none)                                                |
| Suffix                | `a(b)` `a[b]` `a.0` `a.field` `a^`                   | `call` `index` `staticIndex` `fieldRef` `dereference` |
| Prefix                | `+a` `-a` `@a` `*a`                                  | `plus` `minus`, address and dispatch are primitive    |
| Multiplicative        | `a*b` `a/b` `a\b` `a%b`                              | `multiply` `divide` `quotient` `remainder`            |
| Additive              | `a+b` `a-b`                                          | `add` `subtract`                                      |
| Shift                 | `a<<b` `a>>b`                                        | `bitshl` `bitshr`                                     |
| Bitwise               | `a&b` `a~b` `a\|b`                                   | `bitand` `bitxor` `bitor`                             |
| Concatenation         | `a++b`                                               | `cat`                                                 |
| Ordered comparison    | `<=` `<` `>` `>=`                                    | `lesserEquals?` `lesser?` `greater?` `greaterEquals?` |
| Equality              | `==` `!=`                                            | `equals?` `notEquals?`                                |
| Boolean               | `not a` `a and b` `a or b`                           | primitive, not overloadable                           |
| Low-precedence prefix | `if (a) b else c`, `name: a`, `#a`, `..a`, `a -> b`  | (none)                                                |
| Multiple value        | `a, b, c`                                            | (none)                                                |

### Atomic Expressions

#### Name References

A bare identifier evaluates to the named local or global entity in the current scope. An error is raised if no match is found.

```ceramic
import a;
import a.(b);
var c = 0;

foo(d) {
    var e = 0;
    println(a, b, c, d, e);
}
```
Names bound to multiple values (variadic variables, variadic arguments) must be referenced with the `..` unpack operator:

```ceramic
[..TT]
foo(..xs:TT) {
    println(..xs, " have the types ", ..TT);
}
```

#### Literal Expressions

| Literal          | Default type                  | Type suffix examples                            |
| ---------------- | ----------------------------- | ----------------------------------------------- |
| `true` / `false` | `Bool`                        | (none)                                          |
| `1`, `0xFF`      | `Int32` (or module default)   | `ss` `s` `i` `l` `ll` `uss` `us` `u` `ul` `ull` |
| `1.0`, `1e2`     | `Float64` (or module default) | `f` `ff` `fl` `fj` `j` `ffj` `flj`              |
| `'x'`            | via `charLiteral` operator    | (none)                                          |
| `"hello"`        | via `stringLiteral` operator  | (none)                                          |
| `#foo`, `#"foo"` | `Static[#foo]`                | (none)                                          |

```ceramic
println(Type(1));      // Int32
println(Type(-1ss));   // Int8
println(Type(+1ul));   // UInt64
println(Type(1.0f));   // Float32
println(Type(1.j));    // Imag64
```
Integer type suffixes may be applied to floating-point literal tokens to produce a float of that type. Floating-point suffixes may not be applied to integer literal tokens.

#### Parentheses

`(expr)` overrides precedence. It has no other effect.

#### Tuple Expressions

`[a, b, c]` constructs a tuple by calling the `tupleLiteral` operator function.

#### Compilation Context Operators

These are only valid inside **alias functions**:

| Operator       | Evaluates to                                                            |
| -------------- | ----------------------------------------------------------------------- |
| `__FILE__`     | Static string: the source file of the call site                         |
| `__LINE__`     | `Int32`: the source line                                                |
| `__COLUMN__`   | `Int32`: the source column                                              |
| `__ARG__ name` | Static string. Textual representation of argument `name`, not evaluated |

```ceramic
alias assert(cond:Bool) {
    if (not cond) {
        println(stderr, "Assertion \"", __ARG__ cond, "\" failed at ",
            __FILE__, ":", __LINE__, ":", __COLUMN__);
        flush(stderr);
        abort();
    }
}
```

#### Eval Expressions

`eval expr` evaluates a compile-time expression to a static string, parses it as an expression, and substitutes the result in place. The generated string must be a complete, parsable expression.

```ceramic
println(eval #""" "hello world" """);
```

### Suffix Operators

#### Call (`a(b, c)`)

If `a` is a symbol, argument types are matched to its overloads and the matching one is called. If `a` is a `CodePointer`, the pointed-to function is invoked. Otherwise, the call desugars to `call(a, b, c)`.

Lambda expressions can be passed as call arguments, including block lambdas:

```ceramic
ifZero(rand(),
    () -> { println("Reply hazy; try again"); },
    x -> { println("Lucky number: ", x); });
```

If any argument is prefixed with `*`, the call becomes a [dynamic dispatch](#dispatch-a) on a variant type.

#### Index (`a[b, c]`)

Desugars to `index(a, b, c)`. If `a` is a parameterized symbol, the operation is primitive: the symbol is instantiated for compile-time parameters.

```ceramic
var xs = Array[Int, 3](0, 111, 222);
println(xs[2]);  // → index(xs, 2)
```

#### Static Index (`a.0`)

Desugars to `staticIndex(a, #0)`. Used for positional tuple field access.

```ceramic
var x = ["hello", "cruel", "world"];
println(x.0, ' ', x.2);
```

#### Field Reference (`a.field`)

Desugars to `fieldRef(a, #"field")`. Used for named field access on records. Overloading `fieldRef` lets you add custom accessors.

If `a` is an imported module name, the operation is primitive and looks up the name directly in that module's namespace.

```ceramic
import foo;
foo.bar();  // module field reference; primitive

var p = Point(array(1.0, 2.0));
println(p.x, p.y);  // fieldRef(p, #"x"), fieldRef(p, #"y")

// Custom swizzle accessors:
overload fieldRef(p:Point, #"xy") = ref p.coords[0], p.coords[1];
```

#### Dereference (`a^`)

Desugars to `dereference(a)`. Used to get a reference to the value behind a pointer.

### Prefix Operators

| Operator | Behavior                                                                          |
| -------- | --------------------------------------------------------------------------------- |
| `+a`     | desugars to `plus(a)`                                                             |
| `-a`     | desugars to `minus(a)`                                                            |
| `@a`     | primitive: returns `Pointer[T]` to `a`, which must be an lvalue. Not overloadable |
| `*a`     | dispatch operator. Only valid as an argument to a call expression                 |

#### Dispatch (`*a`)

Transforms a call into dynamic dispatch on a variant type. Each instance type of the dispatched argument has an overload looked up and compiled into a dispatch table. All overloads must have matching return types and `ref`-ness.

```ceramic
variant Shape (Circle, Square);

define draw;
overload draw(s:Circle) { println("()"); }
overload draw(s:Square) { println("[]"); }

drawShapes(ss:Vector[Shape]) {
    for (s in ss)
        draw(*s);  // dispatches over Circle and Square
}
```

### Arithmetic Operators

| Operator | Desugars to       |
| -------- | ----------------- |
| `a * b`  | `multiply(a, b)`  |
| `a / b`  | `divide(a, b)`    |
| `a \ b`  | `quotient(a, b)`  |
| `a % b`  | `remainder(a, b)` |
| `a + b`  | `add(a, b)`       |
| `a - b`  | `subtract(a, b)`  |

`\` is integer division truncating toward zero. All arithmetic operators are left-associative within their precedence group.

### Bitwise and Shift Operators

These are user-defined operators with predefined precedence (shift tighter than bitwise):

| Operator | Desugars to      |
| -------- | ---------------- |
| `a << b` | `bitshl(a, b)`   |
| `a >> b` | `bitshr(a, b)`   |
| `a & b`  | `bitand(a, b)`   |
| `a ~ b`  | `bitxor(a, b)`   |
| `a \| b` | `bitor(a, b)`    |
| `a ++ b` | `cat(a, b)`      |

`~` is also the unary bitwise complement: `~a` desugars to `prefixOperator(#(~), a)`, which calls `(~)(a)` = `bitnot(a)` by default.

### Comparison Operators

| Operator | Desugars to            |
| -------- | ---------------------- |
| `a <= b` | `lesserEquals?(a, b)`  |
| `a < b`  | `lesser?(a, b)`        |
| `a > b`  | `greater?(a, b)`       |
| `a >= b` | `greaterEquals?(a, b)` |
| `a == b` | `equals?(a, b)`        |
| `a != b` | `notEquals?(a, b)`     |

All comparison operators are left-associative within their precedence group.

### User-Defined Operators

Any sequence of the characters `= ! < > + - * / \ % ~ | &` forms a valid operator token. The sequences `<--`, `-->`, `=>`, `->`, `~>`, and `=` are reserved and cannot be used as operator names.

`a op b` desugars to `infixOperator(a, #op, b)`, which calls `op(a, b)` by default. `op a` desugars to `prefixOperator(#op, a)`, which calls `op(a)` by default. Declare the operator with `define` and provide an implementation with `overload`:

```ceramic
define (**);
overload (**)(base:Int32, exp:Int32) : Int32 {
    var result = 1;
    for (i in range(exp))
        result *: base;
    return result;
}

println(2 ** 10);  // 1024
```

### Boolean Operators

| Operator  | Behavior                                         |
| --------- | ------------------------------------------------ |
| `not a`   | Complement. `a` must be `Bool`. Not overloadable |
| `a and b` | Short-circuit conjunction. Right-associative     |
| `a or b`  | Short-circuit disjunction. Right-associative     |

Both `and` and `or` require `Bool` operands and are not overloadable.

### Low-Precedence Prefix Operators

#### If Expressions

```ceramic
if (condition) thenExpr else elseExpr
```
Both branches must have the same type. Unlike `if` statements, the `else` clause is required.

#### Keyword Pair Expressions

`name: expr` is sugar for `[#"name", expr]`: a tuple with a static string key. Useful for named-parameter style arguments to higher-order functions.

#### Static Expressions

`#expr` evaluates `expr` at compile time and wraps the result in `Static[result]`. Used to pass compile-time values to [static arguments](functions.md#static-arguments). Applied to a symbol or static string, it is a no-op.

```ceramic
log(#LOG, "starting program");
```

#### Unpack (`..a`)

Evaluates `a` in multiple-value context and interpolates its values into the surrounding expression list.

```ceramic
twoThroughFour() = 2, 3, 4;
oneThroughFive() = 1, ..twoThroughFour(), 5;
```

#### Lambda Expressions

An anonymous function: argument list, arrow, body.

```ceramic
var squares = mapped(x -> x*x, range(10));
```
Two capture modes:

- **`->`**: captures by reference. Mutations are visible outside the lambda. The lambda must not outlive its enclosing scope.
- **`=>`**: captures by copying. The lambda is independent of its origin scope.

```ceramic
// by reference; sum accumulates outside
var sum = 0;
var squares = mapped(x -> { var sq = x*x; sum +: sq; return sq; }, range(10));

// by value; closure is self-contained
curriedAdd(x) = y => x + y;
var plus3 = curriedAdd(3);
```

A lambda with a single untyped argument may omit parentheses: `x -> x*x`. A lambda that does not capture is equivalent to an anonymous named function.

Lambda has higher precedence than the multi-value comma. `a -> b, c` parses as `(a -> b), c`. To return multiple values from a lambda, use a block body or explicit parentheses: `a -> (b, c)`.

### Multiple Value Expressions

Most Ceramic functions can return multiple values. The comma operator builds a multiple-value list:

```ceramic
twoThroughFour() = 2, 3, 4;
```
Expressions are normally constrained to a **single value**. To use a multiple-value expression inside another expression, unpack it with `..`:

```ceramic
oneThroughFive() = 1, ..twoThroughFour(), 5;  // ok
oneThroughFive() = 1, twoThroughFour(), 5;    // ERROR
```
The following contexts provide **implicit** multiple-value context at the outermost level. No `..` is needed there:

- Expression statements
- Local variable bindings with multiple variables
- Assignment with multiple left-hand values
- `..for` value lists

Within a concatenating expression, sub-expressions still require explicit `..`:

```ceramic
var a, b, c, d = 0, ..oneTwoThree();          // .. required inside concat
..for (i in ..oneTwoThree(), ..fourFiveSix())
    println(i);
```
