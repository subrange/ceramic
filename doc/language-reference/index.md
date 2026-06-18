# The Ceramic Programming Language Reference

**Version 0.2**

---

## Conventions

This reference uses two kinds of code blocks:

- **Grammar blocks**: formal BNF syntax. Regular expressions use `/slashes/` with Perl `/x` syntax (whitespace inside is insignificant). Literal strings use `"quotation marks"`.
- **Code blocks**: examples of Ceramic source code.

---

## Sections

| Section                                | Contents                                                                      |
| -------------------------------------- | ----------------------------------------------------------------------------- |
| [Tokenization](tokenization.md)        | Source encoding, whitespace, comments, literals                               |
| [Compilation Strategy](compilation.md) | Whole-program compilation, compile-time evaluation, pattern matching          |
| [Modules & Source Layout](modules.md)  | Modules, imports, symbols, static strings                                     |
| [Type Definitions](types.md)           | Records, variants, enumerations, new types, lambda types                      |
| [Function Definitions](functions.md)   | Simple and overloaded functions, arguments, external functions, global values |
| [Statements](statements.md)            | Blocks, assignment, control flow, loops, exceptions                           |
| [Expressions](expressions.md)          | Operators, precedence, lambdas, multiple values                               |
| [Grammar Reference](grammar.md)        | Full BNF grammar, organized by chapter                                        |
