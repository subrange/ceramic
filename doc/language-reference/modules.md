# Modules

A **module** is a single Ceramic source file. Module names are hierarchical dotted identifiers that map to filesystem paths:

- `foo.bar` resolves to `foo/bar.crm` or `foo/bar/bar.crm` under a compiler search path.

Modules are the basis of Ceramic's namespacing and encapsulation. Each module has its own namespace and can mark symbols `public` or `private`.

### Special Modules

| Module           | Description                                                                                                                                                              |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `__primitives__` | Synthesized by the compiler. Contains fundamental types (`Int`, `Pointer[T]`, `Bool`), basic operations, and compile-time introspection. See the *Primitives Reference*. |
| `prelude`        | Loaded automatically and implicitly imported by every module. The location searched for [operator functions](#operator-functions).                                       |
| `__main__`       | Default name of the entry-point module if it declares no name of its own.                                                                                                |

### Operator Functions

Operator functions are symbols in library code that the language uses internally to implement syntactic forms. They must be publicly reachable through the `prelude` module.

**Overloadable operators:**
`add` `bitand` `bitor` `bitshl` `bitshr` `bitxor` `call` `cat` `dereference` `divide` `equals?` `fieldRef` `greater?` `greaterEquals?` `index` `infixOperator` `lesser?` `lesserEquals?` `minus` `multiply` `notEquals?` `plus` `prefixOperator` `quotient` `remainder` `staticIndex` `subtract` `tupleLiteral`

**Literals:**
`charLiteral` `stringLiteral`

**Value lifecycle:**
`copy` `destroy` `move`

**Switch:** `case?`

**Assignment:**
`assign` `fieldRefAssign` `fieldRefUpdateAssign` `indexAssign` `indexUpdateAssign` `prefixUpdateAssign` `staticIndexAssign` `staticIndexUpdateAssign` `updateAssign`

**For loops:** `iterator` `nextValue` `hasValue?` `getValue`

**Entry point:** `callMain`

**Exceptions:**
`continueException` `exceptionIs?` `exceptionAs` `exceptionAsAny` `throwValue`

**Finalizer/external handlers:**
`exceptionInFinalizer` `exceptionInInitializer` `unhandledExceptionInExternal`

---

## Source File Layout

A Ceramic source file must be laid out in this order:

1. Zero or more [import declarations](#import-declarations)
2. An optional [module declaration](#module-declaration)
3. An optional [top-level LLVM block](#top-level-llvm)
4. Zero or more [top-level definitions](#top-level-definitions)

### List Syntactic Forms

Comma-delimited lists appear throughout Ceramic's grammar and may always end with an optional trailing comma.

```ceramic
record US_Address (
    name:String,
    street:String,
    city:String,
    state:String,
    zip:String,   // trailing comma ok
);
```
In pattern-matching contexts, a variadic tail item may also appear at the end of the list.

### Import Declarations

Import declarations bring other modules' definitions into the current namespace. There are four forms:

```ceramic
import foo.bar;                                  // import as foo.bar; access via foo.bar.thing()
import foo.bar as bar;                           // alias; access via bar.thing()
import foo.bar.(apple, mandarin as tangerine);   // import specific members
import foo.bar.*;                                // import all public members
```
Imports are `private` by default. Use `public import` to re-export through the current module:

```ceramic
public import foo.bar;
```
Private members of another module can be force-imported explicitly:

```ceramic
import foo.bar.(private banana);  // use sparingly
```

#### Conflict Resolution

Importing two things under the same name is an error:

```ceramic
import malkevich;
import bar as malkevich;        // ERROR
```
`.*` imports from multiple modules may overlap without error, as long as ambiguous names are never actually used:

```ceramic
import foo.*;  // exports: a, b
import bar.*;  // exports: b, c

main() {
    a();  // ok: only in foo
    c();  // ok: only in bar
    b();  // ERROR; ambiguous
}
```
Resolve ambiguities by explicitly importing the desired name, or by defining a local override (which shadows wildcard imports):

```ceramic
import foo.*;
import bar.*;
import bar.(b);  // use bar.b specifically
```

### Module Declaration

A module may optionally declare its own name using the `in` keyword. This must appear after imports and before any top-level definitions.

```ceramic
in foo.bas;
```
The declaration may include **module attributes** in parentheses. Currently supported:

- A primitive floating-point type. Sets the default type of untyped float literals in this module.
- A primitive integer type. Sets the default type of untyped integer literals.

```ceramic
in mymodule (Float32, Int64);

main() {
    println(Type(1.0));  // Float32
    println(Type(3));    // Int64
}
```
The attribute list may reference any imported symbols:

```ceramic
// foo.crm
GraphicsModuleAttributes() = Float32, Int32;

// bar.crm
import foo;
in bar (..foo.GraphicsModuleAttributes());
```

### Top-Level LLVM

A module may include a block of raw LLVM assembly emitted directly into the generated LLVM module. This is used to declare intrinsics or global symbols needed by [`__llvm__` function bodies](functions.md#inline-llvm-functions). It must appear after the module declaration and before any top-level definitions.

```ceramic
in traps;

__llvm__ {
declare void @llvm.trap()
}

trap() __llvm__ {
    call void @llvm.trap()
    ret ptr null
}
```
Ceramic static values can be [interpolated](functions.md#inline-llvm-functions) into LLVM blocks.

### Top-Level Definitions

Top-level definitions fall into three categories:

- **[Type definitions](types.md#type-definitions)**: `record`, `variant`, `instance`, `enum`
- **[Function definitions](functions.md#function-definitions)**: `define`, `overload`, function bodies, `external`
- **[Global value definitions](functions.md#global-value-definitions)**: `var`, `alias`, external variables

Ceramic uses two-pass loading: all module namespaces are fully populated before any definition is evaluated. **Forward and circular references are freely allowed**: no forward declarations needed.

```ceramic
// Mutually recursive functions: no forward declarations needed
hurd() { hird(); }
hird() { hurd(); }

// Mutually recursive record types
record Ping (pong:Pointer[Pong]);
record Pong (ping:Pointer[Ping]);
```

#### Pattern Guards

Most definitions can be made **generic** using a pattern guard: a bracketed list of pattern variables before the definition:

```ceramic
[T]
printTwice(file:File, x:T) {
    printTo(file, x);
    printTo(file, x);
}

[T]
record Point[T] (x:T, y:T);

[Stream, T]
printPoint(s:Stream, p:Point[T]) {
    printTo(s, "(", p.x, ", ", p.y, ")");
}
```
Variadic pattern variables are prefixed with `..`:

```ceramic
[..TT]
printlnTwice(file:File, ..x:TT) {
    printlnTo(file, ..x);
    printlnTo(file, ..x);
}
```
A `when` after the variables adds a **predicate**, constraining which values are valid:

```ceramic
[T when Numeric?(T)]
record Point[T] (x:T, y:T);

// No variables: just a platform condition
[when TypeSize(Pointer[Int]) < 4]
overload platformCheck() { error("Time for a new computer"); }
```

#### Visibility Modifiers

Every definition that creates a new symbol may be marked `public` or `private`. The default is `public`.

- `public`: available to importing modules.
- `private`: not importable by default (but can be force-imported).

Visibility modifiers are not valid on `overload` or `instance` forms, which modify existing symbols rather than creating new ones.

## Symbols

Symbols are module-level global names representing types or functions. A symbol is the only value of the stateless primitive type `Static[symbol]`. It exists entirely at compile time.

```ceramic
define foo;

main() {
    println(Type(foo));  // Static[foo]
}
```
Record and variant type symbols may be **parameterized**. Applying the index operator to the base symbol instantiates a parameterized version:

```ceramic
record Foo[T] ();

main() {
    println(Foo);          // the base symbol
    println(Foo[Int32]);   // a parameterized instance
    println(Foo[Float64]);
}
```

### Static Strings

Static strings are compile-time identifiers with no module affiliation. The same static string is identical everywhere it appears. They are written as a string literal (or a valid identifier) prefixed with `#`.

```ceramic
// a.crm
foo() = #"foo";

// b.crm
foo() = #foo;

// main.crm
import a;
import b;

main() {
    println(Type(#"foo"));        // Static[#foo]
    println(a.foo() == b.foo());  // true; same static string
}
```

They are also the operands to `fieldRef`, which implements the `.` field access operator. The `__primitives__` module provides operations for indexing, composing, and slicing them.

---
