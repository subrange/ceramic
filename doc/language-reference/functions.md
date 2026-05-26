# Function Definitions

Ceramic functions are inherently generic. They can be parameterized over types or compile-time values, and overloaded to provide multiple implementations of a common interface. A runtime function is instantiated for every distinct set of input types with which it is called.

### Simple Function Definitions

A name, argument list, optional return types, and a body. Return types are inferred from the body if not declared.

```ceramic
hello() { println(helloString()); }
private helloString() = "Hello World";

squareInt(x:Int) : Int = x*x;

[T]
square(x:T) : T = x*x;

[T | Float?(T)]
quadraticRoots(a:T, b:T, c:T) : T, T {
    var q = -0.5*(b + signum(b)*sqrt(b*b - 4.*a*c));
    return q/a, c/q;
}
```
Simple definitions always **create a new symbol**. Defining the same name twice is an error. Use `overload` instead.

```ceramic
abs(x:Int)   = if (x < 0) -x else x;
abs(x:Float) = ...;  // ERROR: abs is already defined
```

### Overloaded Function Definitions

`define` creates a symbol with no initial implementation. `overload` adds implementations to an existing symbol.

```ceramic
define abs;
overload abs(x:Int)   = if (x < 0) -x else x;
overload abs(x:Float) = if (x < 0.) -x else if (x == 0.) 0. else x;
```
`define` may also declare an **interface constraint**: all overloads must conform to it:

```ceramic
[T | Numeric?(T)]
define abs(x:T) : T;

[T | Integer?(T)]
overload abs(x:T) = if (x < 0) -x else x;

overload abs(x:String) { ... }  // ERROR: Numeric?(String) is false
```
Overloading a **type name** is the idiomatic way to define constructors:

```ceramic
record LatLong (latitude:Float64, longitude:Float64);
record Address (street:String, city:String, state:String, zip:String);

overload Address(coords:LatLong) = geocode(coords);
```
Overloads bind by pattern matching and can target parameterized types selectively:

```ceramic
record Point[T] (x:T, y:T);

[T | Float?(T)]
overload Point[T]() = Point[T](nan(T), nan(T));    // float default: NaN sentinel
overload Point[Int]() = Point[Int](-0x8000_0000, 0x7FFF_FFFF);

overload Point() = Point[Float]();  // base name: give them a Float point
```
A simple function definition is shorthand for `define` + `overload`:

```ceramic
double(x) = x+x;
// is exactly:
define double;
overload double(x) = x+x;
```

#### Overload Ordering

Within a module, overloads are matched in **reverse definition order**. The last definition wins. Across modules, importing modules' overloads are visited before imported modules' (depth-first). Circular-dependency order is undefined.

#### Universal Overloads

The overloaded name may itself be a pattern variable, matching any call site not already handled by a more specific overload:

```ceramic
// Delegate any call with a MyInt argument to its underlying Int value
[F]
overload F(x:MyInt) = ..F(x.value);

// Default zero-constructor for any Numeric? type
[T | Numeric?(T)]
overload T() = T(0);
```
When the function position of a call is not a symbol, the call desugars to the `call` operator:

```ceramic
record MyCallable ();
overload call(f:MyCallable, x:Int, y:Int) : Int = x + y;

main() {
    var f = MyCallable();
    println(f(1, 2));  // really: call(f, 1, 2)
}
```

### Arguments

Arguments are a parenthesized list of names with optional type patterns. An argument without a type matches any type.

```ceramic
[T]
double(x:T) = x+x;  // explicit pattern variable

double(x) = x+x;    // same, with implicit unbounded variable
```
Arguments are passed **by reference**: mutations inside the function are visible to the caller:

```ceramic
inc(x:Int) { x += 1; }

main() {
    var x = 2;
    inc(x);
    println(x);  // 3
}
```

#### Variadic Arguments

A final argument prefixed with `..` matches all remaining arguments at the call site:

```ceramic
printlnTimes(n:Int, ..stuff) {
    for (i in range(n))
        println(..stuff);
}

main(args) {
    printlnTimes(3, "She loves you ", "yeah yeah yeah");
}
```
A type pattern on the variadic argument binds the types of all matched values to a variadic pattern variable:

```ceramic
[..TT | allValues?(String?, ..TT)]
printlnTimes(n:Int, ..stuff:TT) { ... }

[..In, ..Out]
overload call(f:CodePointer[[..In], [..Out]], ..in:In) : ..Out {
    return ..f(..in);
}
```

#### Reference Qualifiers

Ceramic distinguishes **lvalues** (values with a referenceable identity: variables, pointer dereferences, reference returns) from **rvalues** (unnamed temporaries that exist only for a single expression).

An argument may be qualified to accept only one kind:

| Qualifier | Accepts | Typical use |
|-----------|---------|-------------|
| (none) | lvalue or rvalue, bound as lvalue inside the function | general |
| `ref` | lvalue only | returning a reference into the argument |
| `rvalue` | rvalue only | move optimization, steal resources from a temporary |
| `forward` | either, preserves the caller's lvalue/rvalue-ness | perfect forwarding |

```ceramic
// rvalue: steal the string's buffer instead of copying
foo(rvalue x:String) {
    return move(x) + " world";
}

// ref: return a reference into x; dangerous if x is a temporary
bar(ref x:String) {
    return sliced(x, 0, 5);
}

// forward: pass rvalue-ness through to the next call
baz(forward x:Int) {
    foo(x);  // ok if x was originally an rvalue at the call site
}
```
Inside a function body, an argument has a name and is therefore an lvalue, even if the caller passed an rvalue. To carry rvalue-ness through to another call, use `forward` qualification.

`ref`, `rvalue`, and `forward` can all be applied to variadic argument names:

```ceramic
trace(f, forward ..args) {
    println("enter ", f);
    finally println("exit  ", f);
    return forward ..f(..args);
}
```

#### Static Arguments

`static` arguments match values computed at compile time. The `static` keyword at the call site evaluates an expression at compile time and passes the result as the argument.

```ceramic
define beetlejuice;

[n]
overload beetlejuice(static n) {
    for (i in range(n))
        println("Beetlejuice!");
}

// Unrolled specialization for the common case
overload beetlejuice(static 3) {
    println("Beetlejuice!");
    println("Beetlejuice!");
    println("Beetlejuice!");
}

main() {
    beetlejuice(static 3);
}
```
Symbols and static strings are inherently static and match `static` arguments without an explicit `static` at the call site.

`static T` is syntactic sugar for an unnamed argument of primitive type `Static[T]`.

### Return Types

Declare return types with `:` after the argument list. The expression may reference pattern variables from the arguments.

```ceramic
double(x:Int) : Int = x + x;

[T]
diagonal(x:T) : Point[T] = Point[T](x, x);

[T | Integer?(T)]
safeDouble(x:T) : NextLargerInt(T) {
    alias NextT = NextLargerInt(T);
    return NextT(x) + NextT(x);
}
```
Without a declared return type, types are inferred from the body. An empty declaration means "returns nothing":

```ceramic
foo() { }        // inferred: no return values
foo() : { }      // explicit: no return values
foo() : () { }   // also explicit
```

#### Named Return Values

For cases where constructing a return value all at once is inefficient or impossible, bind names directly to the uninitialized return storage and fill them in piecemeal using `<--`.

```ceramic
record SOAPoint (xs:Vector[Float], ys:Vector[Float]);

overload SOAPoint(size:SizeT) --> returned:SOAPoint
{
    returned.xs <-- Vector[Float]();
    onerror destroy(returned.xs);
    resize(returned.xs, size);

    returned.ys <-- Vector[Float]();
    onerror destroy(returned.ys);
    resize(returned.ys, size);
}
```
Named return values are inherently unsafe. They start uninitialized. Any operation other than `<--` before initialization is undefined behavior. They are **not** automatically destroyed during exception unwinding. Use `onerror` to handle cleanup explicitly.

A variadic named return may be declared with `..`, in which case its type expression evaluates as a multiple value expression.

### Function Body

A function body is one of three forms:

```ceramic
// Block: the general form
demBones(a, b) {
    println(a, " bone's connected to the ", b, " bone");
}

// Expression shorthand; exactly equivalent to a block with a single return
square(x) = x*x;

// Inline LLVM assembly
overload add(x:Int32, y:Int32) --> sum:Int32 __llvm__ {
    ...
}
```

#### Inline LLVM Functions

A function may be implemented directly in LLVM IR with an `__llvm__` block. Arguments and named return values are available as LLVM pointers (e.g., `x:Int32` → `ptr %x`). All exit paths must end with `ret ptr null`.

```ceramic
overload add(x:Int32, y:Int32) --> sum:Int32 __llvm__ {
    %xv = load i32, ptr %x
    %yv = load i32, ptr %y
    %sumv = add i32 %xv, %yv
    store i32 %sumv, ptr %sum
    ret ptr null
}
```
Ceramic static values can be interpolated with `$Name` or `${Expression}`:

- Symbols → their LLVM type
- Static strings → literal text
- Static integer/float/bool → LLVM numeric literal

```ceramic
[T | Integer?(T)]
overload add(x:T, y:T) --> sum:T __llvm__ {
    %xv = load $T, ptr %x
    %yv = load $T, ptr %y
    %sumv = add $T %xv, %yv
    store $T %sumv, ptr %sum
    ret ptr null
}
```
Any LLVM intrinsics or globals referenced must be declared in a [top-level LLVM block](modules.md#top-level-llvm). Inline LLVM functions cannot be evaluated at compile time.

### Inline and Alias Qualifiers

Any function or overload definition may be preceded by `inline` or `alias`.

- **`inline`**: the function is always compiled directly into its call site. If inlining is impossible (e.g., a recursive function), it is a compile-time error. This is a hard guarantee, not a hint like C's `inline`.

- **`alias`**: arguments are received unevaluated and re-evaluated in the caller's scope each time they are used inside the function. Equivalent to a hygienic, precedence-safe C preprocessor macro. Alias functions can query their call site's source location via `__FILE__`, `__LINE__`, `__COLUMN__`, and `__ARG__`.

```ceramic
Debug?() = false;

define assert(x:Bool);

[| not Debug?()]
alias overload assert(x:Bool) { }

[| Debug?()]
alias overload assert(x:Bool) {
    if (not x) {
        printlnTo(stderr, __FILE__, ":", __LINE__, ": assertion failed!");
        abort();
    }
}
```

### Diagnostic Attributes

An attribute list `[[...]]` may appear between the pattern guard and any `inline`/`alias` qualifier. Unknown attributes produce a warning, not an error.

Currently recognized attribute:

- **`transparent`**: marks the function as a pure forwarder. When the compiler locates the source of an error, it skips transparent stack frames and attributes the error to the first non-transparent caller. Only apply this to functions whose body is a single forwarding expression.

### External Functions

External functions bridge Ceramic with code outside the compilation unit.

A declaration **without** a body declares a C symbol for Ceramic to call:

```ceramic
external puts(s:Pointer[Int8]) : Int;
external printf(fmt:Pointer[Int8], ..) : Int;  // variadic C

main() {
    puts(cstring("Hello world!"));
    printf(cstring("1 + 1 = %d"), 1 + 1);
}
```
A declaration **with** a body gives a Ceramic function C linkage, making it callable from C:

```ceramic
// square.crm
external square(x:Float64) : Float64 = x*x;
```
```c
// square.c
double square(double x);
int main() { printf("%g\n", square(2.0)); }
```
Limitations:

- Cannot be generic. Types must be fully specified. No pattern guards. No overloading.
- May return zero or one value only.
- Ceramic exceptions cannot propagate across an external boundary. Unhandled exceptions call `unhandledExceptionInExternal`.
- Types with nontrivial `copy` or `destroy` overloads must be passed by pointer.
- Cannot be called at compile time.

External function names are not true symbols. They evaluate directly to a `CCodePointer` value.

#### External Attributes

An optional parenthesized attribute list after the `external` keyword sets properties on the function. A string value overrides the linkage name:

```ceramic
external ("_start") start() {
    var greeting = "hello world";
    write(STDOUT_FILENO, cstring(greeting), size(greeting));
}
```
Calling convention attributes (from `__primitives__`):

| Attribute | Convention |
|-----------|-----------|
| `AttributeCCall` | Default C |
| `AttributeLLVMCall` | Native LLVM (for intrinsics and other LLVM-based languages) |
| `AttributeStdCall` | x86 stdcall (Windows) |
| `AttributeFastCall` | x86 fastcall (Windows) |
| `AttributeThisCall` | x86 thiscall (Windows) |
---

## Global Value Definitions

Ceramic supports global mutable state initialized before `main()` runs.

### Global Aliases

Global aliases define a name that expands to an expression **on demand**, without allocating any storage.

```ceramic
alias PI = 3.14159265358979323846264338327950288;

degreesToRadians(deg:Double) : Double = (PI/180.) * deg;
```
Aliases may be parameterized (pattern guard optional when no predicate is needed):

```ceramic
[T | Float?(T)]
alias PI[T] = T(3.14159265358979323846264338327950288);

alias ZERO[T] = T(0);  // [T] implied
```
Global alias names are not true symbols. They evaluate directly to the bound expression.

### Global Variables

Global variables are initialized at runtime before `main()`, in dependency order.

```ceramic
var msg = noisyString();

noisyString() {
    println("Initializing...");
    return String();
}

a() { push(msg, "Hello "); }
b() { push(msg, "world!"); }

main() { a(); b(); println(msg); }
```
Initialization order is determined by dependencies:

```ceramic
var a = c + 1;  // runs second
var b = a + c;  // runs third
var c = 0;      // runs first
var d = abc();  // runs fourth
```
Circular initialization dependencies are compile-time errors. A global variable that is never referenced by runtime-executed code is never instantiated. Do not rely on its side effects.

Global variables are destroyed in **reverse initialization order** after `main()` returns. If destruction throws, `exceptionInFinalizer` is called.

Parameterized global variables are supported:

```ceramic
private var TAG_COUNTER = 0;

[T]
private var ANY_TAG[T] = nextTagCounter();
```
Global variable names are not true symbols. They evaluate to a reference to the variable's storage.

Runtime access is subject to the C11/C++11 memory model. See the *Primitives Reference* for atomic operations.

### External Variables

C `extern` variables can be linked with external variable definitions. Like external functions, they cannot be parameterized.

```ceramic
external errno : Int;

main() {
    if (null?(fopen(cstring("hello.txt"), cstring("r"))))
        println("error code ", errno);
}
```
A string attribute overrides the linkage name:

```ceramic
external ("____errno$OBSCURECOMPATIBILITYTAG") errno : Int;
```
Ceramic-defined variables with external linkage are currently unsupported. External variables cannot be evaluated at compile time.
---
