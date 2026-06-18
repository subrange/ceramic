# Grammar Reference

The complete BNF grammar for Ceramic, organized by chapter.

- Regular expressions use `/slashes/` with Perl `/x` syntax (whitespace inside is insignificant).
- Literal strings are written in `"quotation marks"`.

## Tokenization

### Whitespace

[→ context in tokenization.md](tokenization.md#whitespace)

```text
ws -> /[ \t\r\n\f]+/
```

### Comments

[→ context in tokenization.md](tokenization.md#comments)

```text
Comment -> "/*" /.*?/ "*/"
         | "//" /.*$/
```

### Identifiers

[→ context in tokenization.md](tokenization.md#identifiers)

```text
Identifier -> !Keyword, /[A-Za-z_?][A-Za-z_0-9?]*/
```

### Integer Literals

[→ context in tokenization.md](tokenization.md#integer-literals)

```text
IntToken      -> "0x" HexDigits | DecimalDigits
HexDigits     -> /([0-9A-Fa-f]_*)+/
DecimalDigits -> /([0-9]_*)+/
```

### Floating-Point Literals

[→ context in tokenization.md](tokenization.md#floating-point-literals)

```text
FloatToken -> "0x" HexDigits ("." HexDigits?)? /[pP] [+-]?/ DecimalDigits
            | DecimalDigits ("." DecimalDigits?)? (/[eE] [+-]?/ DecimalDigits)?
```

### Character Literals

[→ context in tokenization.md](tokenization.md#character-literals)

```text
CharToken  -> "'" CharChar "'"
CharChar   -> /[^\\']/
           | EscapeCode
EscapeCode -> /\\ ([nrtf\\'"0] | x [0-9A-Fa-f]{2})/
```

### String Literals

[→ context in tokenization.md](tokenization.md#string-literals)

```text
StringToken      -> "\"" StringChar* "\""
                  | "\"\"\"" TripleStringChar* "\"\"\""
StringChar       -> /[^\\"]/  | EscapeCode
TripleStringChar -> /(?!=""" ([^"]|$)) [^\\]/ | EscapeCode
```

## Compilation Strategy

### Pattern Matching

[→ context in compilation.md](compilation.md#pattern-matching)

```text
Pattern        -> AtomicPattern PatternSuffix?
AtomicPattern  -> Literal | PatternNameRef
PatternNameRef -> DottedName
PatternSuffix  -> "[" comma_list(Pattern) "]"
```

## Modules & Source Layout

### Source File Layout

[→ context in modules.md](modules.md#source-file-layout)

```text
Module -> Import* ModuleDeclaration? TopLevelLLVM? TopLevelItem*
```

### List Syntactic Forms

[→ context in modules.md](modules.md#list-syntactic-forms)

```text
comma_list(Rule) -> (Rule ("," Rule)* ","?)?

variadic_list(Rule, LastRule) -> Rule ("," Rule)* ("," (LastRule)?)?
                               | LastRule
                               | nil
```

### Conflict Resolution

[→ context in modules.md](modules.md#conflict-resolution)

```text
Import       -> Visibility? "import" DottedName ImportSpec? ";"
ImportSpec   -> "as" Identifier
              | "." "(" comma_list(ImportedItem) ")"
              | "." "*"
DottedName   -> Identifier ("." Identifier)*
ImportedItem -> Visibility? Identifier ("as" Identifier)?
```

### Module Declaration

[→ context in modules.md](modules.md#module-declaration)

```text
ModuleDeclaration -> "in" DottedName AttributeList? ";"
AttributeList     -> "(" ExprList ")"
```

### Top-Level LLVM

[→ context in modules.md](modules.md#top-level-llvm)

```text
TopLevelLLVM -> LLVMBlock
LLVMBlock    -> "__llvm__" "{" /.*/ "}"
```

### Pattern Guards

[→ context in modules.md](modules.md#pattern-guards)

```text
PatternGuard -> "[" comma_list(PatternVar) ("when" Expression)? "]"
PatternVar   -> Identifier | ".." Identifier
```

### Visibility Modifiers

[→ context in modules.md](modules.md#visibility-modifiers)

```text
Visibility -> "public" | "private"
```

## Type Definitions

### Computed Layouts

[→ context in types.md](types.md#computed-layouts)

```text
Record             -> PatternGuard? Visibility? "record" TypeDefinitionName RecordBody
TypeDefinitionName -> Identifier PatternVars?
PatternVars        -> "[" comma_list(PatternVar) "]"
NormalRecordBody   -> "(" comma_list(RecordField) ")" ";"
ComputedRecordBody -> "=" comma_list(Expression) ";"
RecordField        -> Identifier TypeSpec
TypeSpec           -> ":" Pattern
```

### Extending Variants

[→ context in types.md](types.md#extending-variants)

```text
Variant  -> PatternGuard? Visibility? "variant" TypeDefinitionName ("(" ExprList ")")? ";"
Instance -> PatternGuard? "instance" Pattern "(" ExprList ")" ";"
```

### Enumerations

[→ context in types.md](types.md#enumerations)

```text
Enumeration -> Visibility? "enum" Identifier "(" comma_list(Identifier) ")" ";"
```

## Function Definitions

### Simple Function Definitions

[→ context in functions.md](functions.md#simple-function-definitions)

```text
Function -> PatternGuard? Visibility? CodegenAttribute?
            Identifier Arguments ReturnSpec? FunctionBody

CodegenAttribute -> DiagnosticAttrList? InlineAttr? "alias"?
DiagnosticAttrList -> "[[" Identifier ("," Identifier)* "]]"
InlineAttr -> "inline" | "forceinline" | "noinline"
```

### Universal Overloads

[→ context in functions.md](functions.md#universal-overloads)

```text
Define   -> PatternGuard? "define" Identifier (Arguments ReturnSpec?)? ";"
Overload -> PatternGuard? CodegenAttribute? "overload"
            Pattern Arguments ReturnSpec? FunctionBody
```

### Static Arguments

[→ context in functions.md](functions.md#static-arguments)

```text
Arguments          -> "(" ArgumentList ")"
ArgumentList       -> variadic_list(Argument, VarArgument)
Argument           -> NamedArgument | StaticArgument
NamedArgument      -> ReferenceQualifier? Identifier TypeSpec?
VarArgument        -> ReferenceQualifier? ".." Identifier TypeSpec?
StaticArgument     -> "#" Pattern
ReferenceQualifier -> "ref" | "rvalue" | "forward"
```

### Named Return Values

[→ context in functions.md](functions.md#named-return-values)

```text
ReturnSpec      -> ReturnTypeSpec | NamedReturnSpec
ReturnTypeSpec  -> ":" ExprList
NamedReturnSpec -> "-->" comma_list(NamedReturn)
NamedReturn     -> ".."? Identifier ":" Expression
```

### Inline LLVM Functions

[→ context in functions.md](functions.md#inline-llvm-functions)

```text
FunctionBody -> Block | "=" ReturnExpression ";" | LLVMBlock
LLVMBlock    -> "__llvm__" "{" /.*/ "}"
```

### Diagnostic Attributes

[→ context in functions.md](functions.md#diagnostic-attributes)

```text
Attributes -> "[[" Identifier ("," Identifier)* "]]"
```

### External Attributes

[→ context in functions.md](functions.md#external-attributes)

```text
ExternalFunction -> Visibility? "external" AttributeList?
                    Identifier "(" ExternalArgs ")"
                    ":" Type? (FunctionBody | ";")
ExternalArgs     -> variadic_list(ExternalArg, "..")
ExternalArg      -> Identifier TypeSpec
```

### Global Aliases

[→ context in functions.md](functions.md#global-aliases)

```text
GlobalAlias -> PatternGuard? Visibility?
               "alias" Identifier PatternVars? "=" Expression ";"
```

### Global Variables

[→ context in functions.md](functions.md#global-variables)

```text
GlobalVariable -> PatternGuard? Visibility?
                  "var" Identifier PatternVars? "=" Expression ";"
```

### External Variables

[→ context in functions.md](functions.md#external-variables)

```text
ExternalVariable -> Visibility? "external" AttributeList? Identifier TypeSpec ";"
```

## Statements

### Blocks

[→ context in statements.md](statements.md#blocks)

```text
Block    -> "{" (Statement | Binding | LabelDef)* "}"
LabelDef -> Identifier ":"
```

### Return by Reference

[→ context in statements.md](statements.md#return-by-reference)

```text
ReturnStatement  -> "return" ReturnExpression? ";"
ReturnExpression -> ReturnKind? ExprList
ReturnKind       -> "ref" | "forward"
```

### `alias`: call-by-name binding

[→ context in statements.md](statements.md#alias-call-by-name-binding)

```text
Binding     -> BindingKind comma_list(Identifier) "=" ExprList ";"
BindingKind -> "var" | "ref" | "forward" | "alias"
```

### Initialization Statements

[→ context in statements.md](statements.md#initialization-statements)

```text
Assignment   -> ExprList AssignmentOp ExprList ";"
AssignmentOp -> "=" | OpChars ":" | "<--"
OpChars      -> /[=!<>+\-*\/\\%~|&]+/
```

### `switch`

[→ context in statements.md](statements.md#switch)

```text
IfStatement     -> "if" "(" StatementExpression ")" Statement ("else" Statement)?
SwitchStatement -> "switch" "(" Expression ")"
                   ("case" "(" ExprList ")" Statement)*
                   ("else" Statement)?
```

### `..for`: Multiple-Value For

[→ context in statements.md](statements.md#for-multiple-value-for)

```text
WhileStatement         -> "while" "(" StatementExpression ")" Statement
ForStatement           -> "for" "(" comma_list(Identifier) "in" Expression ")" Statement
MultiValueForStatement -> ".." "for" "(" Identifier "in" ExprList ")" Statement

StatementExpression    -> (StatementExprStatement ";")* Expression
StatementExprStatement -> Binding | Assignment
```

### `goto`

[→ context in statements.md](statements.md#goto)

```text
BreakStatement    -> "break" ";"
ContinueStatement -> "continue" ";"
GotoStatement     -> "goto" Identifier ";"
```

### `finally` and `onerror`

[→ context in statements.md](statements.md#finally-and-onerror)

```text
ThrowStatement      -> "throw" Expression ";"
TryStatement        -> "try" Block ("catch" "(" (Identifier (":" Type)?) ")" Block)+
ScopeGuardStatement -> ScopeGuardKind Statement
ScopeGuardKind      -> "finally" | "onerror"
```

### Eval Statements

[→ context in statements.md](statements.md#eval-statements)

```text
EvalStatement -> "eval" ExprList ";"
```

## Expressions

### Lambda Expressions

[→ context in expressions.md](expressions.md#lambda-expressions)

```text
Lambda          -> LambdaArguments LambdaArrow LambdaBody
LambdaArguments -> ".."? Identifier | Arguments
LambdaArrow     -> "=>" | "->"
LambdaBody      -> Block | ReturnExpression
```

### User-Defined Operators

[→ context in expressions.md](expressions.md#user-defined-operators)

```text
InfixExpr  -> Expr UserOp Expr
PrefixExpr -> UserOp Expr
UserOp     -> !("<--" | "-->" | "=>" | "->" | "~>" | "="), OpChars
OpChars    -> /[=!<>+\-*\/\\%~|&]+/
```

