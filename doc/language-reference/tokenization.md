# Tokenization

### Source Encoding

Ceramic source files are ASCII text. Non-ASCII bytes in string and character literals are passed through as opaque bytes.

### Whitespace

ASCII space, tab, carriage return, newline, and form feed are whitespace. Whitespace separates tokens and is otherwise ignored.

### Comments

Ceramic has two comment styles: **block comments** (`/* … */`) and **line comments** (`// …`). Both are treated as a single whitespace character by the lexer.

```ceramic
/* This is a block comment.
   It can span multiple lines. */

// This is a line comment.
```
Block comments are not nestable.

### Identifiers

Identifiers start with a letter, underscore (`_`), or question mark (`?`), followed by zero or more letters, digits, underscores, or question marks.

```ceramic
a    a1    a_1    abc123    a?    ?a    ?
```
The following are **reserved keywords** and may not be used as identifiers:

`__ARG__` `__COLUMN__` `__FILE__` `__LINE__` `__llvm__` `alias` `and` `as` `break` `case` `catch` `continue` `define` `else` `enum` `eval` `external` `false` `finally` `for` `forward` `goto` `if` `import` `in` `inline` `instance` `not` `onerror` `or` `overload` `private` `public` `record` `ref` `return` `rvalue` `static` `switch` `throw` `true` `try` `var` `variant` `while`

### Integer Literals

Integer literals can be decimal or hexadecimal (prefixed with `0x`). Underscores may appear after any digit for readability and have no effect on the value.

```ceramic
0    1    23    0x45abc    1_000_000    0xFFFF_FFFF
```

### Floating-Point Literals

A decimal float literal is distinguished from an integer by including a `.` or an exponent (`e`/`E`). Hexadecimal float literals require a binary exponent (`p`/`P`). Underscores are allowed after any digit.

```ceramic
// Decimal
1.    1.0    1e0    1e-2    0.000_001

// Hexadecimal
0x1p0    0x1.0p0    0x1.0000_0000_0000_1p1_023
```

### Character Literals

A character literal represents a single ASCII character, written between single quotes.

```ceramic
'x'    ' '    '\n'    '\''    '\x7F'
```
Supported escape codes:

| Escape | Meaning                         |
| ------ | ------------------------------- |
| `\0`   | Null                            |
| `\t`   | Tab                             |
| `\n`   | Newline                         |
| `\f`   | Form feed                       |
| `\r`   | Carriage return                 |
| `\"`   | Double quote                    |
| `\'`   | Single quote                    |
| `\\`   | Backslash                       |
| `\xNN` | Arbitrary byte (two hex digits) |

### String Literals

String literals hold a sequence of ASCII text. Ceramic has two forms:

- **`"`-delimited**: `"` and `\` must be escaped.
- **`"""`-delimited**: only `\` must be escaped. Useful for strings that contain quotes.

```ceramic
"hello world"
"\"hello world\""
"""the string "hello world""""

"""
"But not with you, Derek, this star nonsense."
"Yes, yes."
"""
```

---
