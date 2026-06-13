#include "parser.hpp"
#include "ceramic.hpp"
#include "desugar.hpp"
#include "diagnostic.hpp"
#include "error.hpp"

namespace ceramic {
map<llvm::StringRef, IdentifierPtr> Identifier::freeIdentifiers;

static vector<Token> *tokens;
static unsigned position;
static unsigned maxPosition;
// farthest token a terminal tried to match, and what it expected there
static unsigned failPosition;
static vector<llvm::StringRef> failExpected;
// diagnostics collected during recovery, rendered together at the end
static SourcePtr parseSource;
static vector<Diagnostic> parseErrors;
static constexpr unsigned maxParseErrors = 20;
static bool parseErrorOverflow;
static bool parserOptionKeepDocumentation = false;

static AddTokensCallback addTokens = nullptr;

void setAddTokens(AddTokensCallback f) { addTokens = f; }

static bool inRepl = false;

static bool next(Token *&x) {
    if (position == tokens->size()) {
        if (inRepl) {
            assert(addTokens != nullptr);
            vector<Token> toks = addTokens();
            if (toks.empty()) {
                inRepl = false;
                return false;
            }
            tokens->insert(tokens->end(), toks.begin(), toks.end());
        } else {
            return false;
        }
    }
    x = &(*tokens)[position];
    if (position > maxPosition)
        maxPosition = position;
    ++position;
    return true;
}

static unsigned save() { return position; }

static void restore(unsigned p) { position = p; }

// remember the set of terminals expected at the farthest failure point
static void recordExpected(unsigned p, llvm::StringRef what) {
    if (p > failPosition) {
        failPosition = p;
        failExpected.clear();
    }
    if (p != failPosition)
        return;
    for (llvm::StringRef e : failExpected)
        if (e == what)
            return;
    failExpected.push_back(what);
}

// turn the farthest failure into an "expected X, found Y" diagnostic
static Diagnostic buildParseError() {
    const vector<Token> &t = *tokens;
    unsigned pos = failExpected.empty() ? maxPosition : failPosition;

    // rank terminators first, then the expression category, then the rest
    static const char *const terminators[] = {";", "}", ")", "]", ",", ":"};
    vector<llvm::StringRef> ordered;
    for (const char *p : terminators)
        for (llvm::StringRef e : failExpected)
            if (e == p) {
                ordered.push_back(e);
                break;
            }
    for (llvm::StringRef e : failExpected)
        if (e == "expression") {
            ordered.push_back(e);
            break;
        }
    for (llvm::StringRef e : failExpected) {
        bool seen = false;
        for (llvm::StringRef o : ordered)
            if (o == e) {
                seen = true;
                break;
            }
        if (!seen)
            ordered.push_back(e);
    }

    llvm::StringRef lead = ordered.empty() ? llvm::StringRef() : ordered[0];

    // a missing separator beats a closer when another list item follows
    if ((lead == ")" || lead == "]") && pos < t.size()) {
        llvm::StringRef f = t[pos].str;
        bool isCloser =
            f == ")" || f == "]" || f == "}" || f == ";" || f == ",";
        bool commaExpected = false;
        for (llvm::StringRef e : failExpected)
            if (e == ",") {
                commaExpected = true;
                break;
            }
        if (commaExpected && !isCloser)
            lead = ",";
    }

    bool isDelim =
        lead == ";" || lead == "}" || lead == ")" || lead == "]" || lead == ",";

    Span span;
    string buf;
    llvm::raw_string_ostream sout(buf);

    if (isDelim && pos > 0) {
        // a missing delimiter belongs at the gap after the previous token
        const Token &prev = t[pos - 1];
        unsigned gap =
            prev.location.offset + static_cast<unsigned>(prev.str.size());
        span = Span(parseSource, gap, gap);
        sout << "expected `" << lead << "`";
    } else {
        string found;
        if (pos >= t.size()) {
            unsigned end = static_cast<unsigned>(parseSource->size());
            span = Span(parseSource, end, end);
            found = "end of input";
        } else {
            const Token &tok = t[pos];
            unsigned start = tok.location.offset;
            span = Span(parseSource, start,
                        start + static_cast<unsigned>(tok.str.size()));
            found = (llvm::Twine("`") + tok.str.str() + "`").str();
        }
        if (ordered.empty()) {
            sout << "unexpected " << found;
        } else {
            // only the most relevant token, not the whole failure set
            sout << "expected ";
            if (lead == "identifier" || lead == "operator" ||
                lead == "expression")
                sout << lead;
            else
                sout << "`" << lead << "`";
            sout << ", found " << found;
        }
    }

    Diagnostic diag(Severity::Error, sout.str(), span);
    if (lead == ";")
        diag.suggestion = "add `;` here";
    else if (lead == "}")
        diag.suggestion = "add `}` here";
    else if (lead == ")")
        diag.suggestion = "add `)` here";
    else if (lead == "]")
        diag.suggestion = "add `]` here";
    else if (lead == ",")
        diag.suggestion = "add `,` here";
    return diag;
}

static void recordParseError() {
    if (parseErrors.size() >= maxParseErrors && !shouldPrintFullMatchErrors) {
        parseErrorOverflow = true;
        return;
    }
    parseErrors.push_back(buildParseError());
}

// reset failure tracking at the start of a recovered item
static void beginItem() {
    maxPosition = position;
    failPosition = position;
    failExpected.clear();
}

static bool isStmtStartKw(llvm::StringRef s) {
    return s == "if" || s == "while" || s == "for" || s == "return" ||
           s == "switch" || s == "break" || s == "continue" || s == "throw" ||
           s == "var" || s == "ref" || s == "alias" || s == "forward" ||
           s == "try" || s == "goto" || s == "eval" || s == "static";
}

static bool isTopLevelStartKw(llvm::StringRef s) {
    return s == "record" || s == "variant" || s == "instance" || s == "enum" ||
           s == "overload" || s == "external" || s == "alias" ||
           s == "public" || s == "private" || s == "import" || s == "in" ||
           s == "eval" || s == "newtype" || s == "inline" ||
           s == "forceinline" || s == "noinline" || s == "callbyname" ||
           s == "static";
}

// panic-mode: skip a malformed run to the next statement boundary
static void synchronizeBlock() {
    int depth = 0;
    bool first = true;
    while (position < tokens->size()) {
        const Token &cur = (*tokens)[position];
        bool sym = cur.tokenKind == T_SYMBOL;
        if (sym && cur.str == "{") {
            ++depth;
            ++position;
            first = false;
            continue;
        }
        if (sym && cur.str == "}") {
            if (depth == 0)
                return;
            --depth;
            ++position;
            first = false;
            continue;
        }
        if (depth == 0 && !first) {
            if (sym && cur.str == ";") {
                ++position;
                return;
            }
            if (cur.tokenKind == T_KEYWORD && isStmtStartKw(cur.str))
                return;
        }
        ++position;
        first = false;
    }
}

// panic-mode: skip a malformed run to the next top-level item boundary
static void synchronizeTopLevel() {
    int depth = 0;
    bool first = true;
    while (position < tokens->size()) {
        const Token &cur = (*tokens)[position];
        bool sym = cur.tokenKind == T_SYMBOL;
        if (sym && cur.str == "{") {
            ++depth;
            ++position;
            first = false;
            continue;
        }
        if (sym && cur.str == "}") {
            ++position;
            first = false;
            if (depth > 0)
                --depth;
            if (depth == 0)
                return;
            continue;
        }
        if (depth == 0 && !first) {
            if (sym && cur.str == ";") {
                ++position;
                return;
            }
            if (cur.tokenKind == T_KEYWORD && isTopLevelStartKw(cur.str))
                return;
        }
        ++position;
        first = false;
    }
}

// top-level junk gets a declaration-oriented message, with a hint when it
// looks like statement code that belongs inside a function
static void recordTopLevelError() {
    if (parseErrors.size() >= maxParseErrors && !shouldPrintFullMatchErrors) {
        parseErrorOverflow = true;
        return;
    }
    const Token &cur = (*tokens)[position];
    unsigned start = cur.location.offset;
    Span span(parseSource, start,
              start + static_cast<unsigned>(cur.str.size()));
    string found = (llvm::Twine("`") + cur.str.str() + "`").str();
    Diagnostic d(Severity::Error,
                 "expected a top-level declaration, found " + found, span);
    bool looksLikeStmt =
        cur.tokenKind == T_INT_LITERAL || cur.tokenKind == T_FLOAT_LITERAL ||
        cur.tokenKind == T_STRING_LITERAL || cur.tokenKind == T_CHAR_LITERAL ||
        (cur.tokenKind == T_KEYWORD && isStmtStartKw(cur.str) &&
         !isTopLevelStartKw(cur.str));
    if (looksLikeStmt) {
        d.primaryLabel = "statements must go inside a function";
        d.suggestion = "move it inside a function";
    }
    parseErrors.push_back(d);
}

static Location currentLocation() {
    if (position == tokens->size())
        return {};
    return (*tokens)[position].location;
}

//
// symbol, keyword
//

static bool opstring(llvm::StringRef &op) {
    unsigned p = position;
    Token *t;
    if (!next(t) || (t->tokenKind != T_OPSTRING)) {
        recordExpected(p, "operator");
        return false;
    }
    op = llvm::StringRef(t->str);
    return true;
}

static bool uopstring(llvm::StringRef &op) {
    unsigned p = position;
    Token *t;
    if (!next(t) || (t->tokenKind != T_UOPSTRING)) {
        recordExpected(p, "operator");
        return false;
    }
    op = llvm::StringRef(t->str);
    return true;
}

static bool opsymbol(const char *s) {
    unsigned p = position;
    Token *t;
    if (!next(t) || (t->tokenKind != T_OPSTRING) || (t->str != s)) {
        recordExpected(p, s);
        return false;
    }
    return true;
}

static bool symbol(const char *s) {
    unsigned p = position;
    Token *t;
    if (!next(t) || (t->tokenKind != T_SYMBOL) || (t->str != s)) {
        recordExpected(p, s);
        return false;
    }
    return true;
}

static bool keyword(const char *s) {
    unsigned p = position;
    Token *t;
    if (!next(t) || (t->tokenKind != T_KEYWORD) || (t->str != s)) {
        recordExpected(p, s);
        return false;
    }
    return true;
}

static bool ellipsis() { return symbol(".."); }

//
// identifier, identifierList,
// identifierListNoTail, dottedName
//

static bool identifier(IdentifierPtr &x) {
    Location location = currentLocation();
    unsigned p = position;
    Token *t;
    if (!next(t) ||
        (t->tokenKind != T_IDENTIFIER && t->tokenKind != T_OPIDENTIFIER)) {
        recordExpected(p, "identifier");
        return false;
    }
    if (t->tokenKind == T_IDENTIFIER)
        x = Identifier::get(t->str, location);
    else
        x = Identifier::get(t->str, location, true);
    return true;
}

static bool identifierList(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!identifier(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool identifierListNoTail(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !identifier(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool dottedName(DottedNamePtr &x) {
    Location location = currentLocation();
    DottedNamePtr y = new DottedName();
    IdentifierPtr ident;
    if (!identifier(ident))
        return false;
    while (true) {
        y->parts.push_back(ident);
        unsigned p = save();
        if (!symbol(".") || !identifier(ident)) {
            restore(p);
            break;
        }
    }
    x = y;
    x->location = location;
    return true;
}

//
// literals
//

static bool boolLiteral(ExprPtr &x) {
    Location location = currentLocation();
    unsigned p = save();
    if (keyword("true"))
        x = new BoolLiteral(true);
    else if (restore(p), keyword("false"))
        x = new BoolLiteral(false);
    else
        return false;
    x->location = location;
    return true;
}

static string cleanNumericSeparator(llvm::StringRef op, llvm::StringRef s) {
    string out;
    if (op == "-")
        out.push_back('-');
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '_')
            out.push_back(s[i]);
    }
    return out;
}

static bool intLiteral(llvm::StringRef op, ExprPtr &x) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || (t->tokenKind != T_INT_LITERAL))
        return false;
    Token *t2;
    unsigned p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new IntLiteral(cleanNumericSeparator(op, t->str), t2->str);
    } else {
        restore(p);
        x = new IntLiteral(cleanNumericSeparator(op, t->str));
    }
    x->location = location;
    return true;
}

static bool floatLiteral(llvm::StringRef op, ExprPtr &x) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || (t->tokenKind != T_FLOAT_LITERAL))
        return false;
    Token *t2;
    unsigned p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new FloatLiteral(cleanNumericSeparator(op, t->str), t2->str);
    } else {
        restore(p);
        x = new FloatLiteral(cleanNumericSeparator(op, t->str));
    }
    x->location = location;
    return true;
}

static bool charLiteral(ExprPtr &x) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || (t->tokenKind != T_CHAR_LITERAL))
        return false;
    x = new CharLiteral(t->str[0]);
    x->location = location;
    return true;
}

static bool stringLiteral(ExprPtr &x) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || (t->tokenKind != T_STRING_LITERAL))
        return false;
    IdentifierPtr id = Identifier::get(t->str, location);
    x = new StringLiteral(id);
    x->location = location;
    return true;
}

static bool literal(ExprPtr &x) {
    unsigned p = save();
    if (boolLiteral(x))
        return true;
    if (restore(p), intLiteral("+", x))
        return true;
    if (restore(p), floatLiteral("+", x))
        return true;
    if (restore(p), charLiteral(x))
        return true;
    if (restore(p), stringLiteral(x))
        return true;
    return false;
}

//
// expression misc
//

static bool expression(ExprPtr &x, bool = false);

static bool optExpression(ExprPtr &x) {
    unsigned p = save();
    if (!expression(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool expressionList(ExprListPtr &x, bool = false) {
    ExprPtr b;
    if (!expression(b))
        return false;
    ExprListPtr a = new ExprList(b);
    while (true) {
        unsigned p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!expression(b)) {
            restore(p);
            break;
        }
        a->add(b);
    }
    x = a;
    return true;
}

static bool optExpressionList(ExprListPtr &x) {
    unsigned p = save();
    if (!expressionList(x)) {
        restore(p);
        x = new ExprList();
    }
    return true;
}

//
// atomic expr
//

static bool tupleExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("["))
        return false;
    ExprListPtr args;
    if (!optExpressionList(args))
        return false;
    if (!symbol("]"))
        return false;
    x = new Tuple(args);
    x->location = location;
    return true;
}

static bool parenExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("("))
        return false;
    ExprListPtr args;
    if (!optExpressionList(args))
        return false;
    if (!symbol(")"))
        return false;
    x = new Paren(args);
    x->location = location;
    return true;
}

static bool nameRef(ExprPtr &x) {
    Location location = currentLocation();
    IdentifierPtr a;
    if (!identifier(a))
        return false;
    x = new NameRef(a);
    x->location = location;
    return true;
}

static bool fileExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__FILE__"))
        return false;
    x = new FILEExpr();
    x->location = location;
    return true;
}

static bool lineExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__LINE__"))
        return false;
    x = new LINEExpr();
    x->location = location;
    return true;
}

static bool columnExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__COLUMN__"))
        return false;
    x = new COLUMNExpr();
    x->location = location;
    return true;
}

static bool argExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__ARG__"))
        return false;
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    x = new ARGExpr(name);
    x->location = location;
    return true;
}

static bool evalExpr(ExprPtr &ev) {
    Location location = currentLocation();
    if (!keyword("eval"))
        return false;
    ExprPtr args;
    if (!expression(args))
        return false;
    ev = new EvalExpr(args);
    ev->location = location;
    return true;
}

static bool atomicExpr(ExprPtr &x) {
    unsigned p = save();
    if (nameRef(x))
        return true;
    if (restore(p), parenExpr(x))
        return true;
    if (restore(p), literal(x))
        return true;
    if (restore(p), tupleExpr(x))
        return true;
    if (restore(p), fileExpr(x))
        return true;
    if (restore(p), lineExpr(x))
        return true;
    if (restore(p), columnExpr(x))
        return true;
    if (restore(p), argExpr(x))
        return true;
    if (restore(p), evalExpr(x))
        return true;
    recordExpected(p, "expression");
    return false;
}

//
// suffix expr
//

static bool stringLiteralSuffix(ExprPtr &x) {
    Location location = currentLocation();
    ExprPtr str;
    if (!stringLiteral(str))
        return false;
    ExprListPtr strArgs = new ExprList(str);
    x = new Call(nullptr, strArgs);
    x->location = location;
    return true;
}

static bool indexingSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("["))
        return false;
    ExprListPtr args;
    if (!optExpressionList(args))
        return false;
    if (!symbol("]"))
        return false;
    x = new Indexing(nullptr, args);
    x->location = location;
    return true;
}

static bool callSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("("))
        return false;
    ExprListPtr args;
    if (!optExpressionList(args))
        return false;
    if (!symbol(")"))
        return false;
    x = new Call(nullptr, args);
    x->location = location;
    return true;
}

static bool fieldRefSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("."))
        return false;
    IdentifierPtr a;
    if (!identifier(a))
        return false;
    x = new FieldRef(nullptr, a);
    x->location = location;
    return true;
}

static bool staticIndexingSuffix(ExprPtr &x) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || (t->tokenKind != T_STATIC_INDEX))
        return false;
    char *b = const_cast<char *>(t->str.c_str());
    char *end = b;
    unsigned long c = strtoul(b, &end, 0);
    if (*end != 0)
        error(t, "invalid static index value");
    x = new StaticIndexing(nullptr, (size_t)c);
    x->location = location;
    return true;
}

static bool dereferenceSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("^"))
        return false;
    x = new VariadicOp(DEREFERENCE, new ExprList());
    x->location = location;
    return true;
}

static bool suffix(ExprPtr &x) {
    unsigned p = save();
    if (stringLiteralSuffix(x))
        return true;
    if (restore(p), indexingSuffix(x))
        return true;
    if (restore(p), callSuffix(x))
        return true;
    if (restore(p), fieldRefSuffix(x))
        return true;
    if (restore(p), staticIndexingSuffix(x))
        return true;
    if (restore(p), dereferenceSuffix(x))
        return true;
    return false;
}

static void setSuffixBase(Expr *a, const ExprPtr &base) {
    switch (a->exprKind) {
    case INDEXING: {
        auto b = dynamic_cast<Indexing *>(a);
        b->expr = base;
        break;
    }
    case CALL: {
        Call *b = dynamic_cast<Call *>(a);
        b->expr = base;
        break;
    }
    case FIELD_REF: {
        auto b = dynamic_cast<FieldRef *>(a);
        b->expr = base;
        break;
    }
    case STATIC_INDEXING: {
        auto b = dynamic_cast<StaticIndexing *>(a);
        b->expr = base;
        break;
    }
    case VARIADIC_OP: {
        auto *b = dynamic_cast<VariadicOp *>(a);
        assert(b->op == DEREFERENCE);
        b->exprs->add(base);
        break;
    }
    default:
        assert(false);
    }
}

static bool suffixExpr(ExprPtr &x) {
    if (!atomicExpr(x))
        return false;
    while (true) {
        unsigned p = save();
        ExprPtr y;
        if (!suffix(y)) {
            restore(p);
            break;
        }
        setSuffixBase(y.ptr(), x);
        x = y;
    }
    return true;
}

//
// prefix expr
//

static bool prefixExpr(ExprPtr &x);

static bool addressOfExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("@"))
        return false;
    ExprPtr a;
    if (!prefixExpr(a))
        return false;
    x = new VariadicOp(ADDRESS_OF, new ExprList(a));
    x->location = location;
    return true;
}

static bool plusOrMinus(llvm::StringRef &op) {
    unsigned p = save();
    if (opsymbol("+")) {
        op = llvm::StringRef("+");
        return true;
    } else if (restore(p), opsymbol("-")) {
        op = llvm::StringRef("-");
        return true;
    } else
        return false;
}

static bool signedLiteral(llvm::StringRef op, ExprPtr &x) {
    unsigned p = save();
    if (restore(p), intLiteral(op, x))
        return true;
    if (restore(p), floatLiteral(op, x))
        return true;
    return false;
}

static bool dispatchExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!opsymbol("*"))
        return false;
    ExprPtr a;
    if (!prefixExpr(a))
        return false;
    x = new DispatchExpr(a);
    x->location = location;
    return true;
}

static bool staticExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("#"))
        return false;
    ExprPtr y;
    if (!prefixExpr(y))
        return false;
    x = new StaticExpr(y);
    x->location = location;
    return true;
}

static bool operatorOp(llvm::StringRef &op) {
    unsigned p = save();

    const char *s[] = {"<--", "-->", "=>", "->", "~>", "=", nullptr};
    for (const char **a = s; *a; ++a) {
        if (opsymbol(*a))
            return false;
        restore(p);
    }
    if (!opstring(op))
        return false;
    return true;
}

static bool preopExpr(ExprPtr &x) {
    Location location = currentLocation();
    llvm::StringRef op;
    unsigned p = save();
    if (plusOrMinus(op)) {
        if (signedLiteral(op, x))
            return true;
    }
    restore(p);
    if (!operatorOp(op))
        return false;
    ExprPtr y;
    if (!prefixExpr(y))
        return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op, true)));
    exprs->add(y);
    x = new VariadicOp(PREFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool prefixExpr(ExprPtr &x) {
    unsigned p = save();
    if (addressOfExpr(x))
        return true;
    if (restore(p), dispatchExpr(x))
        return true;
    if (restore(p), preopExpr(x))
        return true;
    if (restore(p), staticExpr(x))
        return true;
    if (restore(p), suffixExpr(x))
        return true;
    return false;
}

//
// infix binary operator expr
//

static bool operatorTail(VariadicOpPtr &x) {
    Location location = currentLocation();
    ExprListPtr exprs = new ExprList();
    ExprPtr b;
    llvm::StringRef op;
    if (!operatorOp(op))
        return false;
    unsigned p = save();
    while (true) {
        if (!prefixExpr(b))
            return false;
        exprs->add(new NameRef(Identifier::get(op, true)));
        exprs->add(b);
        p = save();
        if (!operatorOp(op)) {
            restore(p);
            break;
        }
    }
    x = new VariadicOp(INFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool operatorExpr(ExprPtr &x) {
    if (!prefixExpr(x))
        return false;
    while (true) {
        unsigned p = save();
        VariadicOpPtr y;
        if (!operatorTail(y)) {
            restore(p);
            break;
        }
        y->exprs->insert(x);
        x = y.ptr();
    }
    return true;
}

//
// not, and, or
//

static bool notExpr(ExprPtr &x) {
    Location location = currentLocation();
    unsigned p = save();
    if (!keyword("not")) {
        restore(p);
        return operatorExpr(x);
    }
    ExprPtr y;
    if (!operatorExpr(y))
        return false;
    x = new VariadicOp(NOT, new ExprList(y));
    x->location = location;
    return true;
}

static bool andExprTail(AndPtr &x) {
    Location location = currentLocation();
    if (!keyword("and"))
        return false;
    ExprPtr y;
    if (!notExpr(y))
        return false;
    x = new And(nullptr, y);
    x->location = location;
    return true;
}

static bool andExpr(ExprPtr &x) {
    if (!notExpr(x))
        return false;
    while (true) {
        unsigned p = save();
        AndPtr y;
        if (!andExprTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}

static bool orExprTail(OrPtr &x) {
    Location location = currentLocation();
    if (!keyword("or"))
        return false;
    ExprPtr y;
    if (!andExpr(y))
        return false;
    x = new Or(nullptr, y);
    x->location = location;
    return true;
}

static bool orExpr(ExprPtr &x) {
    if (!andExpr(x))
        return false;
    while (true) {
        unsigned p = save();
        OrPtr y;
        if (!orExprTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}

//
// ifExpr
//

static bool ifExpr(ExprPtr &x) {
    Location location = currentLocation();
    ExprPtr expr;
    if (!keyword("if"))
        return false;
    if (!symbol("("))
        return false;
    if (!expression(expr))
        return false;
    ExprListPtr exprs = new ExprList(expr);
    if (!symbol(")"))
        return false;
    if (!expression(expr))
        return false;
    exprs->add(expr);
    if (!keyword("else"))
        return false;
    if (!expression(expr))
        return false;
    exprs->add(expr);
    x = new VariadicOp(IF_EXPR, exprs);
    x->location = location;
    return true;
}

//
// returnKind, returnExprList, returnExpr
//

static bool returnKind(ReturnKind &x) {
    unsigned p = save();
    if (keyword("ref")) {
        x = RETURN_REF;
    } else if (restore(p), keyword("forward")) {
        x = RETURN_FORWARD;
    } else {
        restore(p);
        x = RETURN_VALUE;
    }
    return true;
}

static bool returnExprList(ReturnKind &rkind, ExprListPtr &exprs) {
    if (!returnKind(rkind))
        return false;
    if (!optExpressionList(exprs))
        return false;
    return true;
}

static bool returnExpr(ReturnKind &rkind, ExprPtr &expr) {
    if (!returnKind(rkind))
        return false;
    if (!expression(expr))
        return false;
    return true;
}

//
// lambda
//

static bool block(StatementPtr &x);

static bool arguments(vector<FormalArgPtr> &args, bool &hasVarArg,
                      bool &hasAsConversion);

static bool lambdaArgs(vector<FormalArgPtr> &formalArgs, bool &hasVarArg,
                       bool &hasAsConversion) {
    unsigned p = save();
    IdentifierPtr name;
    if (identifier(name)) {
        formalArgs.clear();
        FormalArgPtr arg = new FormalArg(name, nullptr, TEMPNESS_DONTCARE);
        arg->location = name->location;
        formalArgs.push_back(arg);
        return true;
    }
    restore(p);
    if (arguments(formalArgs, hasVarArg, hasAsConversion))
        return true;
    restore(p);
    return true;
}

static bool lambdaExprBody(StatementPtr &x) {
    Location location = currentLocation();
    ReturnKind rkind;
    ExprPtr expr;
    if (!returnExpr(rkind, expr))
        return false;
    x = new Return(rkind, new ExprList(expr));
    x->location = location;
    return true;
}

static bool lambdaArrow(LambdaCapture &captureBy) {
    unsigned p = save();
    if (opsymbol("->")) {
        captureBy = REF_CAPTURE;
        return true;
    } else if (restore(p), opsymbol("=>")) {
        captureBy = VALUE_CAPTURE;
        return true;
    } else if (restore(p), opsymbol("~>")) {
        captureBy = STATELESS;
        return true;
    }
    return false;
}

static bool lambdaBody(StatementPtr &x) {
    unsigned p = save();
    if (lambdaExprBody(x))
        return true;
    if (restore(p), block(x))
        return true;
    return false;
}

static bool lambda(ExprPtr &x) {
    Location location = currentLocation();
    vector<FormalArgPtr> formalArgs;
    bool hasVarArg = false;
    bool hasAsConversion = false;
    LambdaCapture captureBy;
    StatementPtr body;
    if (!lambdaArgs(formalArgs, hasVarArg, hasAsConversion))
        return false;
    if (!lambdaArrow(captureBy))
        return false;
    if (!lambdaBody(body))
        return false;
    x = new Lambda(captureBy, formalArgs, hasVarArg, hasAsConversion, body);
    x->location = location;
    return true;
}

//
// unpack
//

static bool unpack(ExprPtr &x) {
    Location location = currentLocation();
    if (!ellipsis())
        return false;
    ExprPtr y;
    if (!expression(y))
        return false;
    x = new Unpack(y);
    x->location = location;
    return true;
}

//
// pairExpr
//

static bool pairExpr(ExprPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    if (!symbol(":"))
        return false;
    ExprPtr z;
    if (!expression(z))
        return false;
    ExprPtr ident = new StringLiteral(y);
    ident->location = location;
    ExprListPtr args = new ExprList();
    args->add(ident);
    args->add(z);
    x = new Tuple(args);
    x->location = location;
    return true;
}

//
// expression
//

static bool expression(ExprPtr &x, bool) {
    Location startLocation = currentLocation();
    unsigned p = save();
    if (restore(p), lambda(x))
        goto success;
    if (restore(p), pairExpr(x))
        goto success;
    if (restore(p), orExpr(x))
        goto success;
    if (restore(p), ifExpr(x))
        goto success;
    if (restore(p), unpack(x))
        goto success;
    return false;

success:
    x->startLocation = startLocation;
    x->endLocation = currentLocation();
    return true;
}

//
// pattern
//

static bool dottedNameRef(ExprPtr &x) {
    if (!nameRef(x))
        return false;
    while (true) {
        unsigned p = save();
        ExprPtr y;
        if (!fieldRefSuffix(y)) {
            restore(p);
            break;
        }
        setSuffixBase(y.ptr(), x);
        x = y;
    }
    return true;
}

static bool atomicPattern(ExprPtr &x) {
    unsigned p = save();
    if (dottedNameRef(x))
        return true;
    if (restore(p), intLiteral("+", x))
        return true;
    return false;
}

static bool patternSuffix(IndexingPtr &x) {
    Location location = currentLocation();
    if (!symbol("["))
        return false;
    ExprListPtr args;
    if (!optExpressionList(args))
        return false;
    if (!symbol("]"))
        return false;
    x = new Indexing(nullptr, args);
    x->location = location;
    return true;
}

static bool pattern(ExprPtr &x) {
    Location start = currentLocation();
    if (!atomicPattern(x))
        return false;
    unsigned p = save();
    IndexingPtr y;
    if (!patternSuffix(y)) {
        restore(p);
        x->startLocation = start;
        x->endLocation = currentLocation();
        return true;
    }
    y->expr = x;
    x = y.ptr();
    x->startLocation = start;
    x->endLocation = currentLocation();
    return true;
}

//
// typeSpec, optTypeSpec, exprTypeSpec, optExprTypeSpec
//

static bool typeSpec(ExprPtr &x) {
    if (!symbol(":"))
        return false;
    return pattern(x);
}

static bool optTypeSpec(ExprPtr &x) {
    unsigned p = save();
    if (!typeSpec(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool exprTypeSpec(ExprPtr &x) {
    if (!symbol(":"))
        return false;
    return expression(x);
}

static bool optExprTypeSpec(ExprPtr &x) {
    unsigned p = save();
    if (!exprTypeSpec(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

//
// statements
//

static bool statement(StatementPtr &x, bool = false);

static bool labelDef(StatementPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    if (!symbol(":"))
        return false;
    x = new Label(y);
    x->location = location;
    return true;
}

static bool bindingKind(BindingKind &bindingKind) {
    unsigned p = save();
    if (keyword("var"))
        bindingKind = VAR;
    else if (restore(p), keyword("ref"))
        bindingKind = REF;
    else if (restore(p), keyword("alias"))
        bindingKind = ALIAS;
    else if (restore(p), keyword("forward"))
        bindingKind = FORWARD;
    else
        return false;
    return true;
}

static bool optPatternVarsWithCond(vector<PatternVar> &x, ExprPtr &y);

static bool bindingsBody(vector<FormalArgPtr> &x, bool &hasVarArg);

static bool localBinding(StatementPtr &x) {
    Location location = currentLocation();
    BindingKind bk;
    if (!bindingKind(bk))
        return false;
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    vector<FormalArgPtr> args;
    bool hasVarArg = false;
    if (!bindingsBody(args, hasVarArg))
        return false;
    unsigned p = save();
    if (!symbol(","))
        restore(p);
    if (!opsymbol("="))
        return false;
    ExprListPtr z;
    if (!expressionList(z))
        return false;
    if (!symbol(";"))
        return false;
    vector<ObjectPtr> patternTypes;
    x = new Binding(bk, patternVars, patternTypes, predicate, args, z,
                    hasVarArg);
    x->location = location;
    return true;
}

static bool blockItem(StatementPtr &x) {
    unsigned p = save();
    if (labelDef(x))
        return true;
    if (restore(p), localBinding(x))
        return true;
    if (restore(p), statement(x))
        return true;
    return false;
}

static bool blockItems(vector<StatementPtr> &stmts, bool) {
    while (true) {
        beginItem();
        unsigned p = save();
        StatementPtr z;
        if (blockItem(z)) {
            stmts.push_back(z);
            continue;
        }
        restore(p);
        if (position >= tokens->size())
            break;
        Token &cur = (*tokens)[position];
        if (cur.tokenKind == T_SYMBOL && cur.str == "}")
            break;
        recordParseError();
        synchronizeBlock();
        if (parseErrors.size() >= maxParseErrors &&
            !shouldPrintFullMatchErrors) {
            parseErrorOverflow = true;
            break;
        }
    }
    return true;
}

static bool statementExprStatement(StatementPtr &stmt);

static bool statementExpression(vector<StatementPtr> &stmts, ExprPtr &expr) {
    ExprPtr tailExpr;
    StatementPtr stmt;
    while (true) {
        unsigned p = save();
        if (statementExprStatement(stmt)) {
            stmts.push_back(stmt);
        } else if (restore(p), expression(tailExpr)) {
            unsigned q = save();
            if (symbol(";")) {
                stmts.push_back(new ExprStatement(tailExpr));
            } else {
                restore(q);
                expr = tailExpr;
                return true;
            }
        } else {
            return false;
        }
    }
}

static bool block(StatementPtr &x) {
    Location location = currentLocation();
    if (!symbol("{"))
        return false;
    // a leading brace can only be a block, so commit and recover inside
    BlockPtr y = new Block();
    blockItems(y->statements, false);
    if (!symbol("}"))
        recordParseError();
    x = y.ptr();
    x->location = location;
    return true;
}

static bool assignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y))
        return false;
    if (!opsymbol("="))
        return false;
    if (!expressionList(z))
        return false;
    if (!symbol(";"))
        return false;
    x = new Assignment(y, z);
    x->location = location;
    return true;
}

static bool initAssignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y))
        return false;
    if (!opsymbol("<--"))
        return false;
    if (!expressionList(z))
        return false;
    if (!symbol(";"))
        return false;
    x = new InitAssignment(y, z);
    x->location = location;
    return true;
}

static bool prefixUpdate(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr z;
    llvm::StringRef op;
    if (!uopstring(op))
        return false;
    if (!expression(z))
        return false;
    if (!symbol(";"))
        return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op, true)));
    exprs->add(z);
    x = new VariadicAssignment(PREFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool updateAssignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr y, z;
    if (!expression(y))
        return false;
    llvm::StringRef op;
    if (!uopstring(op))
        return false;
    if (!expression(z))
        return false;
    if (!symbol(";"))
        return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op, true)));
    exprs->add(y);
    exprs->add(z);
    x = new VariadicAssignment(INFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool gotoStatement(StatementPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!keyword("goto"))
        return false;
    if (!identifier(y))
        return false;
    if (!symbol(";"))
        return false;
    x = new Goto(y);
    x->location = location;
    return true;
}

static bool returnStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("return"))
        return false;
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs))
        return false;
    if (!symbol(";"))
        return false;
    x = new Return(rkind, exprs);
    x->location = location;
    return true;
}

static bool optElse(StatementPtr &x) {
    unsigned p = save();
    if (!keyword("else")) {
        restore(p);
        return true;
    }
    return statement(x);
}

static bool ifStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> condStmts;
    ExprPtr condition;
    StatementPtr thenPart, elsePart;
    if (!keyword("if"))
        return false;
    if (!symbol("("))
        return false;
    if (!statementExpression(condStmts, condition))
        return false;
    if (!symbol(")"))
        return false;
    if (!statement(thenPart))
        return false;
    if (!optElse(elsePart))
        return false;
    x = new If(condStmts, condition, thenPart, elsePart);
    x->location = location;
    return true;
}

static bool caseList(ExprListPtr &x) {
    if (!keyword("case"))
        return false;
    if (!symbol("("))
        return false;
    if (!expressionList(x))
        return false;
    if (!symbol(")"))
        return false;
    return true;
}

static bool caseBlock(CaseBlockPtr &x) {
    Location location = currentLocation();
    ExprListPtr caseLabels;
    StatementPtr body;
    if (!caseList(caseLabels))
        return false;
    if (!statement(body))
        return false;
    x = new CaseBlock(caseLabels, body);
    x->location = location;
    return true;
}

static bool caseBlockList(vector<CaseBlockPtr> &x) {
    CaseBlockPtr a;
    if (!caseBlock(a))
        return false;
    x.clear();
    while (true) {
        x.push_back(a);
        unsigned p = save();
        if (!caseBlock(a)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool switchStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> exprStmts;
    ExprPtr expr;
    if (!keyword("switch"))
        return false;
    if (!symbol("("))
        return false;
    if (!statementExpression(exprStmts, expr))
        return false;
    if (!symbol(")"))
        return false;
    vector<CaseBlockPtr> caseBlocks;
    if (!caseBlockList(caseBlocks))
        return false;
    StatementPtr defaultCase;
    if (!optElse(defaultCase))
        return false;
    x = new Switch(exprStmts, expr, caseBlocks, defaultCase);
    x->location = location;
    return true;
}

static bool exprStatement(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr y;
    if (!expression(y))
        return false;
    if (!symbol(";"))
        return false;
    x = new ExprStatement(y);
    x->location = location;
    return true;
}

static bool whileStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> condStmts;
    ExprPtr cond;
    StatementPtr body;
    if (!keyword("while"))
        return false;
    if (!symbol("("))
        return false;
    if (!statementExpression(condStmts, cond))
        return false;
    if (!symbol(")"))
        return false;
    if (!statement(body))
        return false;
    x = new While(condStmts, cond, body);
    x->location = location;
    return true;
}

static bool breakStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("break"))
        return false;
    if (!symbol(";"))
        return false;
    x = new Break();
    x->location = location;
    return true;
}

static bool continueStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("continue"))
        return false;
    if (!symbol(";"))
        return false;
    x = new Continue();
    x->location = location;
    return true;
}

static bool forStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<IdentifierPtr> a;
    ExprPtr b;
    StatementPtr c;
    if (!keyword("for"))
        return false;
    if (!symbol("("))
        return false;
    if (!identifierList(a))
        return false;
    if (!keyword("in"))
        return false;
    if (!expression(b))
        return false;
    if (!symbol(")"))
        return false;
    if (!statement(c))
        return false;
    x = new For(a, b, c);
    x->location = location;
    return true;
}

static bool catchBlock(CatchPtr &x) {
    Location location = currentLocation();
    if (!keyword("catch"))
        return false;
    if (!symbol("("))
        return false;
    IdentifierPtr evar;
    if (!identifier(evar))
        return false;
    ExprPtr etype;
    if (!optExprTypeSpec(etype))
        return false;
    IdentifierPtr contextVar;
    unsigned p = save();
    if (keyword("in")) {
        if (!identifier(contextVar))
            return false;
    } else {
        restore(p);
    }
    if (!symbol(")"))
        return false;
    StatementPtr body;
    if (!block(body))
        return false;
    x = new Catch(evar, etype, contextVar, body);
    x->location = location;
    return true;
}

static bool catchBlockList(vector<CatchPtr> &x) {
    CatchPtr a;
    if (!catchBlock(a))
        return false;
    x.clear();
    while (true) {
        x.push_back(a);
        unsigned p = save();
        if (!catchBlock(a)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool tryStatement(StatementPtr &x) {
    Location location = currentLocation();
    StatementPtr a;
    if (!keyword("try"))
        return false;
    if (!block(a))
        return false;
    vector<CatchPtr> b;
    if (!catchBlockList(b))
        return false;
    x = new Try(a, b);
    x->location = location;
    return true;
}

static bool throwStatement(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr a;
    ExprPtr context;
    if (!keyword("throw"))
        return false;
    unsigned p = save();
    if (!symbol(";")) {
        restore(p);
        if (!optExpression(a))
            return false;
        unsigned q = save();
        if (keyword("in")) {
            if (!optExpression(context))
                return false;
        } else {
            restore(q);
        }
    } else {
        restore(p);
    }
    if (!symbol(";"))
        return false;
    x = new Throw(a, context);
    x->location = location;
    return true;
}

static bool staticFor(StatementPtr &x) {
    Location location = currentLocation();
    if (!ellipsis())
        return false;
    if (!keyword("for"))
        return false;
    if (!symbol("("))
        return false;
    IdentifierPtr a;
    if (!identifier(a))
        return false;
    if (!keyword("in"))
        return false;
    ExprListPtr b;
    if (!expressionList(b))
        return false;
    if (!symbol(")"))
        return false;
    StatementPtr c;
    if (!statement(c))
        return false;
    x = new StaticFor(a, b, c);
    x->location = location;
    return true;
}

static bool finallyStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("finally"))
        return false;
    StatementPtr body;
    if (!statement(body))
        return false;
    x = new Finally(body);
    x->location = location;
    return true;
}

static bool onerrorStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("onerror"))
        return false;
    StatementPtr body;
    if (!statement(body))
        return false;
    x = new OnError(body);
    x->location = location;
    return true;
}

static bool evalStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("eval"))
        return false;
    ExprListPtr args;
    if (!expressionList(args))
        return false;
    if (!symbol(";"))
        return false;
    x = new EvalStatement(args);
    x->location = location;
    return true;
}

// parse staticassert top level or statement
static bool staticAssert(ExprPtr &cond, ExprListPtr &message,
                         Location &location) {
    location = currentLocation();
    if (!keyword("staticassert"))
        return false;
    if (!symbol("("))
        return false;
    if (!expression(cond))
        return false;

    message = new ExprList();

    unsigned s = save();
    while (symbol(",")) {
        ExprPtr expr;
        if (!expression(expr))
            return false;
        message->add(expr);
        s = save();
    }
    restore(s);

    if (!symbol(")"))
        return false;
    if (!symbol(";"))
        return false;
    return true;
}

static bool staticAssertTopLevel(TopLevelItemPtr &x, Module *module) {
    ExprPtr cond;
    ExprListPtr message;
    Location location;
    if (!staticAssert(cond, message, location))
        return false;
    x = new StaticAssertTopLevel(module, cond, message);
    x->location = location;
    return true;
}

static bool staticAssertStatement(StatementPtr &x) {
    ExprPtr cond;
    ExprListPtr message;
    Location location;
    if (!staticAssert(cond, message, location))
        return false;
    x = new StaticAssertStatement(cond, message);
    x->location = location;
    return true;
}

static bool statement(StatementPtr &x, bool) {
    unsigned p = save();
    if (block(x))
        return true;
    if (restore(p), assignment(x))
        return true;
    if (restore(p), initAssignment(x))
        return true;
    if (restore(p), updateAssignment(x))
        return true;
    if (restore(p), prefixUpdate(x))
        return true;
    if (restore(p), ifStatement(x))
        return true;
    if (restore(p), switchStatement(x))
        return true;
    if (restore(p), returnStatement(x))
        return true;
    if (restore(p), evalStatement(x))
        return true;
    if (restore(p), exprStatement(x))
        return true;
    if (restore(p), whileStatement(x))
        return true;
    if (restore(p), breakStatement(x))
        return true;
    if (restore(p), continueStatement(x))
        return true;
    if (restore(p), forStatement(x))
        return true;
    if (restore(p), staticFor(x))
        return true;
    if (restore(p), tryStatement(x))
        return true;
    if (restore(p), throwStatement(x))
        return true;
    if (restore(p), finallyStatement(x))
        return true;
    if (restore(p), onerrorStatement(x))
        return true;
    if (restore(p), staticAssertStatement(x))
        return true;
    if (restore(p), gotoStatement(x))
        return true;

    return false;
}

static bool statementExprStatement(StatementPtr &stmt) {
    unsigned p = save();
    if (localBinding(stmt))
        return true;
    if (restore(p), assignment(stmt))
        return true;
    if (restore(p), initAssignment(stmt))
        return true;
    if (restore(p), updateAssignment(stmt))
        return true;
    return false;
}

//
// staticParams
//

static bool staticVarParam(IdentifierPtr &varParam) {
    if (!ellipsis())
        return false;
    if (!identifier(varParam))
        return false;
    return true;
}

static bool optStaticVarParam(IdentifierPtr &varParam) {
    unsigned p = save();
    if (!staticVarParam(varParam)) {
        restore(p);
        varParam = nullptr;
    }
    return true;
}

static bool trailingVarParam(IdentifierPtr &varParam) {
    if (!symbol(","))
        return false;
    if (!staticVarParam(varParam))
        return false;
    return true;
}

static bool optTrailingVarParam(IdentifierPtr &varParam) {
    unsigned p = save();
    if (!trailingVarParam(varParam)) {
        restore(p);
        varParam = nullptr;
    }
    return true;
}

static bool paramsAndVarParam(vector<IdentifierPtr> &params,
                              IdentifierPtr &varParam) {
    if (!identifierListNoTail(params))
        return false;
    if (!optTrailingVarParam(varParam))
        return false;
    unsigned p = save();
    if (!symbol(","))
        restore(p);
    return true;
}

static bool staticParamsInner(vector<IdentifierPtr> &params,
                              IdentifierPtr &varParam) {
    unsigned p = save();
    if (paramsAndVarParam(params, varParam))
        return true;
    restore(p);
    params.clear();
    varParam = nullptr;
    return optStaticVarParam(varParam);
}

static bool staticParams(vector<IdentifierPtr> &params,
                         IdentifierPtr &varParam) {
    if (!symbol("["))
        return false;
    if (!staticParamsInner(params, varParam))
        return false;
    if (!symbol("]"))
        return false;
    return true;
}

static bool optStaticParams(vector<IdentifierPtr> &params,
                            IdentifierPtr &varParam) {
    unsigned p = save();
    if (!staticParams(params, varParam)) {
        restore(p);
        params.clear();
        varParam = nullptr;
    }
    return true;
}

//
// code
//

static bool varArgTypeSpec(ExprPtr &vargType) {
    if (!symbol(":"))
        return false;
    Location start = currentLocation();
    if (!nameRef(vargType))
        return false;
    vargType->startLocation = start;
    vargType->endLocation = currentLocation();
    return true;
}

static bool optVarArgTypeSpec(ExprPtr &vargType) {
    unsigned p = save();
    if (!varArgTypeSpec(vargType)) {
        restore(p);
        vargType = nullptr;
    }
    return true;
}

static bool optArgTempness(ValueTempness &tempness) {
    unsigned p = save();
    if (keyword("rvalue")) {
        tempness = TEMPNESS_RVALUE;
        return true;
    }
    restore(p);
    if (keyword("ref")) {
        tempness = TEMPNESS_LVALUE;
        return true;
    }
    restore(p);
    if (keyword("forward")) {
        tempness = TEMPNESS_FORWARD;
        return true;
    }
    restore(p);
    tempness = TEMPNESS_DONTCARE;
    return true;
}

static bool valueFormalArg(FormalArgPtr &x, bool &hasVarArg,
                           bool &hasAsConversion) {
    Location location = currentLocation();
    ValueTempness tempness;
    if (!optArgTempness(tempness))
        return false;
    IdentifierPtr y;
    ExprPtr z;
    bool varArg = false;
    unsigned p = save();
    if (ellipsis()) {
        if (hasVarArg)
            return false;
        else
            hasVarArg = true;
        varArg = true;
    } else
        restore(p);
    if (!identifier(y))
        return false;
    if (varArg) {
        if (!optVarArgTypeSpec(z))
            return false;
    } else {
        if (!optTypeSpec(z))
            return false;
    }
    p = save();
    if (keyword("as")) {
        if (varArg)
            return false;
        ExprPtr w;
        if (!pattern(w))
            return false;
        hasAsConversion = true;
        x = new FormalArg(y, z, tempness, w);
    } else {
        restore(p);
        x = new FormalArg(y, z, tempness, varArg);
    }
    x->location = location;
    return true;
}

static FormalArgPtr makeStaticFormalArg(size_t index, const ExprPtr &expr,
                                        Location const &location) {
    // desugar static args
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream sout(buf);
    sout << "%arg" << index;
    IdentifierPtr argName = Identifier::get(sout.str());

    ExprPtr indexing = wrapIntoStatic(expr);
    indexing->startLocation = location;
    indexing->endLocation = currentLocation();

    FormalArgPtr arg = new FormalArg(argName, indexing.ptr());
    arg->location = location;
    return arg;
}

static bool staticFormalArg(size_t index, FormalArgPtr &x) {
    Location location = currentLocation();
    ExprPtr expr;
    if (!symbol("#"))
        return false;
    if (!expression(expr))
        return false;

    if (expr->exprKind == UNPACK) {
        error(expr, "#static variadic arguments are not yet supported");
    }

    x = makeStaticFormalArg(index, expr, location);
    return true;
}

static bool stringFormalArg(size_t index, FormalArgPtr &x) {
    Location location = currentLocation();
    ExprPtr expr;
    if (!stringLiteral(expr))
        return false;
    x = makeStaticFormalArg(index, expr, location);
    return true;
}

static bool formalArg(size_t index, FormalArgPtr &x, bool &hasVarArg,
                      bool &hasAsConversion) {
    unsigned p = save();
    if (valueFormalArg(x, hasVarArg, hasAsConversion))
        return true;
    if (restore(p), staticFormalArg(index, x))
        return true;
    if (restore(p), stringFormalArg(index, x))
        return true;
    return false;
}

static bool formalArgs(vector<FormalArgPtr> &x, bool &hasVarArg,
                       bool &hasAsConversion) {
    FormalArgPtr y;
    if (!formalArg(x.size(), y, hasVarArg, hasAsConversion))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") ||
            !formalArg(x.size(), y, hasVarArg, hasAsConversion)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool argumentsBody(vector<FormalArgPtr> &args, bool &hasVarArg,
                          bool &hasAsConversion) {
    unsigned p = save();
    if (!formalArgs(args, hasVarArg, hasAsConversion)) {
        restore(p);
        args.clear();
    }
    return true;
}

static bool arguments(vector<FormalArgPtr> &args, bool &hasVarArg,
                      bool &hasAsConversion) {
    if (!symbol("("))
        return false;
    if (!argumentsBody(args, hasVarArg, hasAsConversion))
        return false;
    if (!symbol(")"))
        return false;
    return true;
}

static bool bindingArg(FormalArgPtr &x, bool &hasVarArg) {
    Location location = currentLocation();
    IdentifierPtr y;
    ExprPtr z;
    bool varArg = false;
    unsigned p = save();
    if (ellipsis()) {
        if (hasVarArg)
            return false;
        else
            hasVarArg = true;
        varArg = true;
    } else
        restore(p);
    if (!identifier(y))
        return false;
    if (varArg) {
        if (!optVarArgTypeSpec(z))
            return false;
    } else {
        if (!optTypeSpec(z))
            return false;
    }
    x = new FormalArg(y, z);
    x->location = location;
    x->varArg = varArg;
    return true;
}

static bool bindingsBody(vector<FormalArgPtr> &x, bool &hasVarArg) {
    FormalArgPtr y;
    if (!bindingArg(y, hasVarArg))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !bindingArg(y, hasVarArg)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool predicate(ExprPtr &x) {
    if (!keyword("when"))
        return false;
    return expression(x);
}

static bool optPredicate(ExprPtr &x) {
    unsigned p = save();
    if (!predicate(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool patternVar(PatternVar &x) {
    unsigned p = save();
    x.isMulti = true;
    if (!ellipsis()) {
        restore(p);
        x.isMulti = false;
    }
    if (!identifier(x.name))
        return false;
    return true;
}

static bool patternVarList(vector<PatternVar> &x) {
    PatternVar y;
    if (!patternVar(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !patternVar(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optPatternVarList(vector<PatternVar> &x) {
    unsigned p = save();
    if (!patternVarList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool patternVarsWithCond(vector<PatternVar> &x, ExprPtr &y) {
    if (!symbol("["))
        return false;
    if (!optPatternVarList(x))
        return false;
    if (!optPredicate(y) || !symbol("]")) {
        x.clear();
        y = nullptr;
        return false;
    }
    return true;
}

static bool optPatternVarsWithCond(vector<PatternVar> &x, ExprPtr &y) {
    unsigned p = save();
    if (!patternVarsWithCond(x, y)) {
        restore(p);
        x.clear();
        y = nullptr;
    }
    return true;
}

static void skipOptPatternVar() {
    unsigned p = save();
    if (!symbol("[")) {
        restore(p);
        return;
    }
    // `[[` belongs to a different grammar rule.
    unsigned q = save();
    if (symbol("[")) {
        restore(p);
        return;
    }
    restore(q);
    int bracket = 1;
    while (bracket) {
        unsigned i = save();
        if (symbol("[")) {
            ++bracket;
            continue;
        }
        restore(i);
        if (symbol("]"))
            --bracket;
    }
}

static bool exprBody(StatementPtr &x) {
    if (!opsymbol("="))
        return false;
    Location location = currentLocation();
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs))
        return false;
    if (!symbol(";"))
        return false;
    x = new Return(rkind, exprs, true);
    x->location = location;
    return true;
}

static bool body(StatementPtr &x) {
    unsigned p = save();
    if (exprBody(x))
        return true;
    if (restore(p), block(x))
        return true;
    return false;
}

static bool optBody(StatementPtr &x) {
    unsigned p = save();
    if (body(x))
        return true;
    restore(p);
    x = nullptr;
    if (symbol(";"))
        return true;
    return false;
}

//
// topLevelVisibility, importVisibility
//

bool optVisibility(Visibility defaultVisibility, Visibility &x) {
    unsigned p = save();
    if (keyword("public")) {
        x = PUBLIC;
        return true;
    }
    restore(p);
    if (keyword("private")) {
        x = PRIVATE;
        return true;
    }
    restore(p);
    x = defaultVisibility;
    return true;
}

bool topLevelVisibility(Visibility &x) { return optVisibility(PUBLIC, x); }

bool importVisibility(Visibility &x) {
    return optVisibility(VISIBILITY_UNDEFINED, x);
}

//
// records
//

static bool recordField(RecordFieldPtr &x, bool &hasVarField) {
    Location location = currentLocation();
    IdentifierPtr y;
    ExprPtr z;
    bool varField = false;
    unsigned p = save();
    if (ellipsis()) {
        if (hasVarField)
            return false;
        else
            hasVarField = true;
        varField = true;
    } else
        restore(p);
    if (!identifier(y))
        return false;
    if (varField) {
        if (!varArgTypeSpec(z))
            return false;
    } else {
        if (!exprTypeSpec(z))
            return false;
    }
    x = new RecordField(y, z);
    x->location = location;
    x->varField = varField;
    return true;
}

static bool recordFields(vector<RecordFieldPtr> &x, bool &hasVarField) {
    RecordFieldPtr y;
    if (!recordField(y, hasVarField))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!recordField(y, hasVarField)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optRecordFields(vector<RecordFieldPtr> &x, bool &hasVarField) {
    unsigned p = save();
    if (!recordFields(x, hasVarField)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool recordBodyFields(RecordBodyPtr &x) {
    Location location = currentLocation();
    if (!symbol("("))
        return false;
    vector<RecordFieldPtr> y;
    bool hasVarField = false;
    if (!optRecordFields(y, hasVarField))
        return false;
    if (!symbol(")"))
        return false;
    if (!symbol(";"))
        return false;
    x = new RecordBody(y, hasVarField);
    x->location = location;
    return true;
}

static bool recordBodyComputed(RecordBodyPtr &x) {
    Location location = currentLocation();
    if (!opsymbol("="))
        return false;
    ExprListPtr y;
    if (!optExpressionList(y))
        return false;
    if (!symbol(";"))
        return false;
    x = new RecordBody(y);
    x->location = location;
    return true;
}

static bool recordBody(RecordBodyPtr &x) {
    unsigned p = save();
    if (recordBodyFields(x))
        return true;
    if (restore(p), recordBodyComputed(x))
        return true;
    return false;
}

static bool record(TopLevelItemPtr &x, Module *module, unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("record"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);

    IdentifierPtr name;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    RecordBodyPtr body;

    if (!identifier(name))
        return false;
    if (!optStaticParams(params, varParam))
        return false;
    if (!recordBody(body))
        return false;
    x = new RecordDecl(module, name, vis, patternVars, predicate, params,
                       varParam, body);
    x->location = location;
    return true;
}

//
// variant, instance
//

static bool instances(ExprListPtr &x) {
    return symbol("(") && optExpressionList(x) && symbol(")");
}

static bool optInstances(ExprListPtr &x, bool &open) {
    unsigned p = save();
    if (symbol("(")) {
        open = false;
        return optExpressionList(x) && symbol(")");
    } else {
        open = true;
        restore(p);
        x = new ExprList();
        return true;
    }
}

static bool variant(TopLevelItemPtr &x, Module *module, unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("variant"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam))
        return false;
    ExprListPtr defaultInstances;
    bool open;
    if (!optInstances(defaultInstances, open))
        return false;
    if (!symbol(";"))
        return false;
    x = new VariantDecl(module, name, vis, patternVars, predicate, params,
                        varParam, open, defaultInstances);
    x->location = location;
    return true;
}

static bool instance(TopLevelItemPtr &x, Module *module, unsigned s) {
    if (!keyword("instance"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);
    ExprPtr target;
    if (!pattern(target))
        return false;
    ExprListPtr members;
    if (!instances(members))
        return false;
    if (!symbol(";"))
        return false;
    x = new InstanceDecl(module, patternVars, predicate, target, members);
    x->location = location;
    return true;
}

static bool newtype(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("newtype"))
        return false;
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    if (!opsymbol("="))
        return false;
    ExprPtr expr;
    if (!expression(expr))
        return false;
    if (!symbol(";"))
        return false;
    x = new NewTypeDecl(module, name, vis, expr);
    x->location = location;
    return true;
}

//
// returnSpec
//

static bool namedReturnName(IdentifierPtr &x) {
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    if (!symbol(":"))
        return false;
    x = y;
    return true;
}

static bool returnTypeExpression(ExprPtr &x) { return orExpr(x); }

static bool returnType(ReturnSpecPtr &x) {
    Location location = currentLocation();
    ExprPtr z;
    if (!returnTypeExpression(z))
        return false;
    x = new ReturnSpec(z, nullptr);
    x->location = location;
    return true;
}

static bool returnTypeList(vector<ReturnSpecPtr> &x) {
    ReturnSpecPtr y;
    if (!returnType(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !returnType(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optReturnTypeList(vector<ReturnSpecPtr> &x) {
    unsigned p = save();
    if (!returnTypeList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool namedReturn(ReturnSpecPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!namedReturnName(y))
        return false;
    ExprPtr z;
    if (!returnTypeExpression(z))
        return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool namedReturnList(vector<ReturnSpecPtr> &x) {
    ReturnSpecPtr y;
    if (!namedReturn(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !namedReturn(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optNamedReturnList(vector<ReturnSpecPtr> &x) {
    unsigned p = save();
    if (!namedReturnList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool varReturnType(ReturnSpecPtr &x) {
    Location location = currentLocation();
    if (!ellipsis())
        return false;
    ExprPtr z;
    if (!returnTypeExpression(z))
        return false;
    x = new ReturnSpec(z, nullptr);
    x->location = location;
    return true;
}

static bool optVarReturnType(ReturnSpecPtr &x) {
    unsigned p = save();
    if (!varReturnType(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool varNamedReturn(ReturnSpecPtr &x) {
    Location location = currentLocation();
    if (!ellipsis())
        return false;
    IdentifierPtr y;
    if (!namedReturnName(y))
        return false;
    ExprPtr z;
    if (!returnTypeExpression(z))
        return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool optVarNamedReturn(ReturnSpecPtr &x) {
    unsigned p = save();
    if (!varNamedReturn(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool allReturnSpecsWithFlag(vector<ReturnSpecPtr> &returnSpecs,
                                   ReturnSpecPtr &varReturnSpec,
                                   bool &exprRetSpecs) {
    returnSpecs.clear();
    varReturnSpec = nullptr;
    unsigned p = save();
    if (symbol(":")) {
        if (!optReturnTypeList(returnSpecs))
            return false;
        if (!optVarReturnType(varReturnSpec))
            return false;
        if (!returnSpecs.empty() || varReturnSpec != nullptr)
            exprRetSpecs = true;
        return true;
    } else {
        restore(p);
        if (opsymbol("-->")) {
            if (!optNamedReturnList(returnSpecs))
                return false;
            if (!optVarNamedReturn(varReturnSpec))
                return false;
            return true;
        } else {
            restore(p);
            return false;
        }
    }
}

static bool allReturnSpecs(vector<ReturnSpecPtr> &returnSpecs,
                           ReturnSpecPtr &varReturnSpec) {
    bool exprRetSpecs = false;
    return allReturnSpecsWithFlag(returnSpecs, varReturnSpec, exprRetSpecs);
}

//
// define, overload
//

static bool isOverload(bool &isDefault) {
    int p = static_cast<int>(save());
    if (keyword("overload"))
        isDefault = false;
    else if (restore(p), keyword("default"))
        isDefault = true;
    else {
        return false;
    }
    return true;
}

// `[[name, ...]]` attribute list. Unknown names warn.
static bool optAttributeList(bool &isDiagnosticTransparent) {
    isDiagnosticTransparent = false;
    unsigned p = save();
    if (!symbol("[")) {
        restore(p);
        return true;
    }
    if (!symbol("[")) {
        restore(p);
        return true;
    }
    while (true) {
        Location attrLoc = currentLocation();
        IdentifierPtr name;
        if (!identifier(name))
            return false;
        if (name->str == "transparent") {
            isDiagnosticTransparent = true;
        } else {
            pushLocation(attrLoc);
            warning("unknown attribute '" + name->str + "'");
            popLocation();
        }
        unsigned q = save();
        if (symbol(","))
            continue;
        restore(q);
        break;
    }
    if (!symbol("]"))
        return false;
    if (!symbol("]"))
        return false;
    return true;
}

static bool optInline(InlineAttribute &isInline) {
    unsigned p = save();
    if (keyword("inline"))
        isInline = INLINE;
    else if (restore(p), keyword("forceinline"))
        isInline = FORCE_INLINE;
    else if (restore(p), keyword("noinline"))
        isInline = NEVER_INLINE;
    else {
        restore(p);
        isInline = IGNORE;
    }
    return true;
}

static bool optCallByName(bool &callByName) {
    unsigned p = save();
    if (!keyword("alias")) {
        restore(p);
        callByName = false;
        return true;
    }
    callByName = true;
    return true;
}

static bool optPrivateOverload(bool &privateOverload) {
    unsigned p = save();
    if (!keyword("private")) {
        restore(p);
        privateOverload = false;
        return true;
    }
    if (!keyword("overload")) {
        return false;
    }
    privateOverload = true;
    return true;
}

static bool llvmCode(LLVMCodePtr &b) {
    Token *t;
    if (!next(t) || (t->tokenKind != T_LLVM))
        return false;
    b = new LLVMCode(t->str);
    b->location = t->location;
    return true;
}

static bool llvmProcedure(vector<TopLevelItemPtr> &x, Module *module) {
    Location location = currentLocation();
    CodePtr y = new Code();
    if (!optPatternVarsWithCond(y->patternVars, y->predicate))
        return false;
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    InlineAttribute isInline;
    if (!optInline(isInline))
        return false;
    IdentifierPtr z;
    Location targetStartLocation = currentLocation();
    if (!identifier(z))
        return false;
    Location targetEndLocation = currentLocation();
    bool hasVarArg = false;
    bool hasAsConversion = false;
    if (!arguments(y->formalArgs, hasVarArg, hasAsConversion))
        return false;
    y->hasVarArg = hasVarArg;
    y->returnSpecsDeclared = allReturnSpecs(y->returnSpecs, y->varReturnSpec);
    if (!llvmCode(y->llvmBody))
        return false;
    y->location = location;

    ProcedurePtr u = new Procedure(module, z, vis, true);
    u->location = location;
    x.emplace_back(u.ptr());

    ExprPtr target = new NameRef(z);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr v =
        new Overload(module, target, y, false, isInline, hasAsConversion);
    v->location = location;
    x.emplace_back(v.ptr());

    u->singleOverload = v;

    return true;
}

static bool procedureWithInterface(vector<TopLevelItemPtr> &x, Module *module,
                                   unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("define"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    CodePtr interfaceCode = new Code();
    if (!optPatternVarsWithCond(interfaceCode->patternVars,
                                interfaceCode->predicate))
        return false;
    restore(e);
    IdentifierPtr name;
    Location targetStartLocation = currentLocation();
    if (!identifier(name))
        return false;
    Location targetEndLocation = currentLocation();
    bool hasVarArg = false;
    bool hasAsConversion = false;
    if (!arguments(interfaceCode->formalArgs, hasVarArg, hasAsConversion))
        return false;
    interfaceCode->hasVarArg = hasVarArg;
    interfaceCode->returnSpecsDeclared = allReturnSpecs(
        interfaceCode->returnSpecs, interfaceCode->varReturnSpec);

    bool privateOverload;
    if (!optPrivateOverload(privateOverload))
        return false;

    if (!symbol(";"))
        return false;
    interfaceCode->location = location;

    ExprPtr target = new NameRef(name);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr interface =
        new Overload(module, target, interfaceCode, false, IGNORE);
    interface->location = location;

    ProcedurePtr proc =
        new Procedure(module, name, vis, privateOverload, interface);
    proc->location = location;
    x.emplace_back(proc.ptr());

    return true;
}

static bool procedureWithBody(vector<TopLevelItemPtr> &x, Module *module,
                              unsigned s) {
    bool isDiagnosticTransparent = false;
    if (!optAttributeList(isDiagnosticTransparent))
        return false;
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    InlineAttribute isInline;
    if (!optInline(isInline))
        return false;
    bool callByName;
    if (!optCallByName(callByName))
        return false;
    IdentifierPtr name;
    Location targetStartLocation = currentLocation();
    if (!identifier(name))
        return false;
    Location targetEndLocation = currentLocation();
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    CodePtr code = new Code();
    if (!optPatternVarsWithCond(code->patternVars, code->predicate))
        return false;
    restore(e);
    bool hasVarArg = false;
    bool hasAsConversion = false;
    if (!arguments(code->formalArgs, hasVarArg, hasAsConversion))
        return false;
    code->hasVarArg = hasVarArg;
    bool exprRetSpecs = false;
    code->returnSpecsDeclared = allReturnSpecsWithFlag(
        code->returnSpecs, code->varReturnSpec, exprRetSpecs);
    if (!body(code->body))
        return false;
    code->location = location;
    if (exprRetSpecs && code->body->stmtKind == RETURN) {
        auto return_ = dynamic_cast<Return *>(code->body.ptr());
        if (return_->isExprReturn)
            return_->isReturnSpecs = true;
    }

    ProcedurePtr proc = new Procedure(module, name, vis, true);
    proc->location = location;
    x.emplace_back(proc.ptr());

    ExprPtr target = new NameRef(name);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr oload = new Overload(module, target, code, callByName, isInline,
                                     hasAsConversion);
    oload->location = location;
    oload->isDiagnosticTransparent = isDiagnosticTransparent;
    x.emplace_back(oload.ptr());

    proc->singleOverload = oload;

    return true;
}

static bool procedure(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("define"))
        return false;
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    bool privateOverload;
    if (!optPrivateOverload(privateOverload))
        return false;
    if (!symbol(";"))
        return false;
    x = new Procedure(module, name, vis, privateOverload);
    x->location = location;
    return true;
}

static bool overload(TopLevelItemPtr &x, Module *module, unsigned s) {
    bool isDiagnosticTransparent = false;
    if (!optAttributeList(isDiagnosticTransparent))
        return false;
    InlineAttribute isInline;
    if (!optInline(isInline))
        return false;
    bool callByName;
    if (!optCallByName(callByName))
        return false;
    bool isDefault;
    if (!isOverload(isDefault))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    CodePtr code = new Code();
    if (!optPatternVarsWithCond(code->patternVars, code->predicate))
        return false;
    restore(e);
    ExprPtr target;
    Location targetStartLocation = currentLocation();
    if (!pattern(target))
        return false;
    Location targetEndLocation = currentLocation();
    bool hasVarArg = false;
    bool hasAsConversion = false;
    if (!arguments(code->formalArgs, hasVarArg, hasAsConversion))
        return false;
    code->hasVarArg = hasVarArg;
    bool exprRetSpecs = false;
    code->returnSpecsDeclared = allReturnSpecsWithFlag(
        code->returnSpecs, code->varReturnSpec, exprRetSpecs);
    unsigned p = save();
    if (!optBody(code->body)) {
        restore(p);
        if (callByName)
            return false;
        if (!llvmCode(code->llvmBody))
            return false;
    }
    if (exprRetSpecs && code->body->stmtKind == RETURN) {
        auto return_ = dynamic_cast<Return *>(code->body.ptr());
        if (return_->isExprReturn)
            return_->isReturnSpecs = true;
    }
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    code->location = location;
    OverloadPtr oload = new Overload(module, target, code, callByName, isInline,
                                     hasAsConversion);
    oload->location = location;
    oload->isDefault = isDefault;
    oload->isDiagnosticTransparent = isDiagnosticTransparent;
    x = oload.ptr();
    return true;
}

//
// enumerations
//

static bool enumMember(EnumMemberPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    x = new EnumMember(y);
    x->location = location;
    return true;
}

static bool enumMemberList(vector<EnumMemberPtr> &x) {
    EnumMemberPtr y;
    if (!enumMember(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!enumMember(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool enumeration(TopLevelItemPtr &x, Module *module, unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("enum"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    EnumDeclPtr z = new EnumDecl(module, y, vis, patternVars, predicate);
    if (!symbol("("))
        return false;
    if (!enumMemberList(z->members))
        return false;
    if (!symbol(")"))
        return false;
    if (!symbol(";"))
        return false;
    x = z.ptr();
    x->location = location;
    return true;
}

//
// global variable
//

static bool globalVariable(TopLevelItemPtr &x, Module *module, unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("var"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam))
        return false;
    if (!opsymbol("="))
        return false;
    ExprPtr expr;
    if (!expression(expr))
        return false;
    if (!symbol(";"))
        return false;
    x = new GlobalVariable(module, name, vis, patternVars, predicate, params,
                           varParam, expr);
    x->location = location;
    return true;
}

//
// external procedure, external variable
//

static bool externalAttributes(ExprListPtr &x) {
    if (!symbol("("))
        return false;
    if (!expressionList(x))
        return false;
    if (!symbol(")"))
        return false;
    return true;
}

static bool optExternalAttributes(ExprListPtr &x) {
    unsigned p = save();
    if (!externalAttributes(x)) {
        restore(p);
        x = new ExprList();
    }
    return true;
}

static bool externalArg(ExternalArgPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y))
        return false;
    ExprPtr z;
    if (!exprTypeSpec(z))
        return false;
    x = new ExternalArg(y, z);
    x->location = location;
    return true;
}

static bool externalArgs(vector<ExternalArgPtr> &x) {
    ExternalArgPtr y;
    if (!externalArg(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !externalArg(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool externalVarArgs(bool &hasVarArgs) {
    if (!ellipsis())
        return false;
    hasVarArgs = true;
    return true;
}

static bool optExternalVarArgs(bool &hasVarArgs) {
    unsigned p = save();
    if (!externalVarArgs(hasVarArgs)) {
        restore(p);
        hasVarArgs = false;
    }
    return true;
}

static bool trailingExternalVarArgs(bool &hasVarArgs) {
    if (!symbol(","))
        return false;
    if (!externalVarArgs(hasVarArgs))
        return false;
    return true;
}

static bool optTrailingExternalVarArgs(bool &hasVarArgs) {
    unsigned p = save();
    if (!trailingExternalVarArgs(hasVarArgs)) {
        restore(p);
        hasVarArgs = false;
    }
    return true;
}

static bool externalArgsWithVArgs(vector<ExternalArgPtr> &x, bool &hasVarArgs) {
    if (!externalArgs(x))
        return false;
    if (!optTrailingExternalVarArgs(hasVarArgs))
        return false;
    return true;
}

static bool externalArgsBody(vector<ExternalArgPtr> &x, bool &hasVarArgs) {
    unsigned p = save();
    if (externalArgsWithVArgs(x, hasVarArgs))
        return true;
    restore(p);
    x.clear();
    hasVarArgs = false;
    if (!optExternalVarArgs(hasVarArgs))
        return false;
    return true;
}

static bool externalBody(StatementPtr &x) {
    unsigned p = save();
    if (exprBody(x))
        return true;
    if (restore(p), block(x))
        return true;
    if (restore(p), symbol(";")) {
        x = nullptr;
        return true;
    }
    return false;
}

static bool optExternalReturn(ExprPtr &x) {
    unsigned p = save();
    if (!symbol(":")) {
        restore(p);
        return true;
    }
    return optExpression(x);
}

static bool external(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("external"))
        return false;
    ExternalProcedurePtr y = new ExternalProcedure(module, vis);
    if (!optExternalAttributes(y->attributes))
        return false;
    if (!identifier(y->name))
        return false;
    if (!symbol("("))
        return false;
    bool hasVarArg = false;
    if (!externalArgsBody(y->args, hasVarArg))
        return false;
    y->hasVarArgs = hasVarArg;
    if (!symbol(")"))
        return false;
    if (!optExternalReturn(y->returnType))
        return false;
    if (!externalBody(y->body))
        return false;
    x = y.ptr();
    x->location = location;
    return true;
}

static bool externalVariable(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("external"))
        return false;
    ExternalVariablePtr y = new ExternalVariable(module, vis);
    if (!optExternalAttributes(y->attributes))
        return false;
    if (!identifier(y->name))
        return false;
    if (!exprTypeSpec(y->type))
        return false;
    if (!symbol(";"))
        return false;
    x = y.ptr();
    x->location = location;
    return true;
}

//
// global alias
//

static bool globalAlias(TopLevelItemPtr &x, Module *module, unsigned s) {
    Visibility vis;
    if (!topLevelVisibility(vis))
        return false;
    if (!keyword("alias"))
        return false;
    unsigned e = save();
    restore(s);
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate))
        return false;
    restore(e);
    IdentifierPtr name;
    if (!identifier(name))
        return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam))
        return false;
    if (!opsymbol("="))
        return false;
    ExprPtr expr;
    if (!expression(expr))
        return false;
    if (!symbol(";"))
        return false;
    x = new GlobalAlias(module, name, vis, patternVars, predicate, params,
                        varParam, expr);
    x->location = location;
    return true;
}

//
// imports
//

static bool importAlias(IdentifierPtr &x) {
    if (!keyword("as"))
        return false;
    if (!identifier(x))
        return false;
    return true;
}

static bool optImportAlias(IdentifierPtr &x) {
    unsigned p = save();
    if (!importAlias(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool importModule(ImportPtr &x) {
    Location location = currentLocation();
    DottedNamePtr y;
    if (!dottedName(y))
        return false;
    IdentifierPtr z;
    if (!optImportAlias(z))
        return false;
    x = new ImportModule(y, z);
    x->location = location;
    return true;
}

static bool importStar(ImportPtr &x) {
    Location location = currentLocation();
    DottedNamePtr y;
    if (!dottedName(y))
        return false;
    if (!symbol("."))
        return false;
    if (!opsymbol("*"))
        return false;
    x = new ImportStar(y);
    x->location = location;
    return true;
}

static bool importedMember(ImportedMember &x) {
    if (!topLevelVisibility(x.visibility))
        return false;
    if (!identifier(x.name))
        return false;
    if (!optImportAlias(x.alias))
        return false;
    return true;
}

static bool importedMemberList(vector<ImportedMember> &x) {
    ImportedMember y;
    if (!importedMember(y))
        return false;
    x.clear();
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!importedMember(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool importMembers(ImportPtr &x) {
    Location location = currentLocation();
    DottedNamePtr y;
    if (!dottedName(y))
        return false;
    if (!symbol("."))
        return false;
    if (!symbol("("))
        return false;
    ImportMembersPtr z = new ImportMembers(y);
    if (!importedMemberList(z->members))
        return false;
    if (!symbol(")"))
        return false;
    x = z.ptr();
    x->location = location;
    return true;
}

static bool import2(ImportPtr &x, Visibility visTop) {
    Visibility vis;
    if (!importVisibility(vis))
        return false;
    unsigned p = save();
    if (importStar(x))
        goto importsuccess;
    if (restore(p), importMembers(x))
        goto importsuccess;
    if (restore(p), importModule(x))
        goto importsuccess;
    return false;
importsuccess:
    if (vis == VISIBILITY_UNDEFINED) {
        if (visTop == VISIBILITY_UNDEFINED)
            x->visibility = PRIVATE;
        else
            x->visibility = visTop;
    } else
        x->visibility = vis;
    return true;
}

static bool importList(vector<ImportPtr> &x, Visibility vis) {
    ImportPtr y;
    if (!import2(y, vis))
        return false;
    while (true) {
        x.push_back(y);
        unsigned p = save();
        if (!symbol(",") || !import2(y, vis)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool import(vector<ImportPtr> &x, bool &sawKeyword) {
    sawKeyword = false;
    Visibility vis;
    if (!importVisibility(vis))
        return false;
    if (!keyword("import"))
        return false;
    sawKeyword = true;
    if (!importList(x, vis))
        return false;
    if (!symbol(";"))
        return false;
    return true;
}

static bool imports(vector<ImportPtr> &x) {
    x.clear();
    while (true) {
        beginItem();
        unsigned p = save();
        bool sawKeyword = false;
        if (import(x, sawKeyword))
            continue;
        restore(p);
        // a run that got past the keyword is a broken import, not the
        // start of the declarations
        if (!sawKeyword)
            break;
        recordParseError();
        synchronizeTopLevel();
        if (parseErrors.size() >= maxParseErrors &&
            !shouldPrintFullMatchErrors) {
            parseErrorOverflow = true;
            break;
        }
    }
    return true;
}

static bool evalTopLevel(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    if (!keyword("eval"))
        return false;
    ExprListPtr args;
    if (!expressionList(args))
        return false;
    if (!symbol(";"))
        return false;
    x = new EvalTopLevel(module, args);
    x->location = location;
    return true;
}

//
// documentation
//

static bool
documentationAnnotation(std::map<DocumentationAnnotation, string> &an) {
    Location location = currentLocation();
    Token *t;
    if (!next(t) || t->tokenKind != T_DOC_PROPERTY)
        return false;
    auto key = llvm::StringRef(t->str);

    DocumentationAnnotation ano;
    if (key == "section") {
        ano = SectionAnnotation;
    } else if (key == "module") {
        ano = ModuleAnnotation;
    } else if (key == "overload" || key == "default") {
        ano = OverloadAnnotation;
    } else if (key == "record") {
        ano = RecordAnnotation;
    } else {
        ano = InvalidAnnotation;
        pushLocation(location);
        fmtError("invalid annotation '%s'\n", key.str().c_str());
    }

    if (!next(t) || t->tokenKind != T_DOC_TEXT)
        return false;
    auto value = llvm::StringRef(t->str);

    an.insert(std::pair<DocumentationAnnotation, string>(ano, value.str()));
    return true;
}

static bool documentationText(std::string &text) {
    Token *t;
    if (!next(t) || t->tokenKind != T_DOC_TEXT)
        return false;
    if (parserOptionKeepDocumentation)
        text.append(t->str.str());
    text.append("\n");
    return true;
}

static bool documentation(TopLevelItemPtr &x, Module *module) {
    Location location = currentLocation();
    std::map<DocumentationAnnotation, string> annotation;
    std::string text;

    bool success = false;
    bool hasAttachmentAnnotation = false;

    for (;;) {
        unsigned p = save();
        if (documentationAnnotation(annotation)) {
            if (hasAttachmentAnnotation) {
                restore(p);
                break;
            }
            hasAttachmentAnnotation = true;
            success = true;
            continue;
        }
        restore(p);
        if (documentationText(text)) {
            success = true;
            continue;
        }
        restore(p);
        break;
    }

    if (success) {
        x = new Documentation(module, annotation, text);
        return true;
    }
    return false;
}

//
// module
//

static bool topLevelItem(vector<TopLevelItemPtr> &x, Module *module) {
    unsigned p = save();

    TopLevelItemPtr y;
    skipOptPatternVar();
    unsigned q = save();
    if (restore(q), overload(y, module, p))
        goto success;
    if (restore(q), procedureWithInterface(x, module, p))
        goto success2;
    if (restore(q), procedureWithBody(x, module, p))
        goto success2;
    if (restore(q), record(y, module, p))
        goto success;
    if (restore(q), variant(y, module, p))
        goto success;
    if (restore(q), instance(y, module, p))
        goto success;
    if (restore(q), enumeration(y, module, p))
        goto success;
    if (restore(q), globalVariable(y, module, p))
        goto success;
    if (restore(q), globalAlias(y, module, p))
        goto success;
    if (restore(p), procedure(y, module))
        goto success;
    if (restore(p), documentation(y, module))
        goto success;
    if (restore(p), llvmProcedure(x, module))
        goto success2;
    if (restore(p), external(y, module))
        goto success;
    if (restore(p), externalVariable(y, module))
        goto success;
    if (restore(p), newtype(y, module))
        goto success;
    if (restore(p), evalTopLevel(y, module))
        goto success;
    if (restore(p), staticAssertTopLevel(y, module))
        goto success;
    return false;
success:
    assert(y.ptr());
    x.push_back(y);
success2:
    return true;
}

static bool topLevelItems(vector<TopLevelItemPtr> &x, Module *module) {
    x.clear();

    while (true) {
        beginItem();
        unsigned p = save();
        if (topLevelItem(x, module))
            continue;
        restore(p);
        if (position >= tokens->size())
            break;
        // if the item parsed partway, the farthest failure is more precise
        // than blaming the leading token as a bad declaration
        if (failPosition > p)
            recordParseError();
        else
            recordTopLevelError();
        synchronizeTopLevel();
        if (parseErrors.size() >= maxParseErrors &&
            !shouldPrintFullMatchErrors) {
            parseErrorOverflow = true;
            break;
        }
    }
    return true;
}

static bool optModuleDeclarationAttributes(ExprListPtr &x) {
    unsigned p = save();
    if (!symbol("(")) {
        restore(p);
        x = nullptr;
        return true;
    }
    if (!optExpressionList(x))
        return false;
    if (!symbol(")"))
        return false;
    return true;
}

static bool optModuleDeclaration(ModuleDeclarationPtr &x) {
    Location location = currentLocation();

    unsigned p = save();
    if (!keyword("in")) {
        restore(p);
        x = nullptr;
        return true;
    }
    DottedNamePtr name;
    ExprListPtr attributes;
    if (!dottedName(name))
        return false;
    if (!optModuleDeclarationAttributes(attributes))
        return false;
    if (!symbol(";"))
        return false;

    x = new ModuleDeclaration(name, attributes);
    x->location = location;
    return true;
}

static bool optTopLevelLLVM(LLVMCodePtr &x) {
    unsigned p = save();
    if (!llvmCode(x)) {
        restore(p);
        x = nullptr;
    }
    return true;
}

static bool module(llvm::StringRef moduleName, ModulePtr &x) {
    Location location = currentLocation();
    ModulePtr y = new Module(moduleName);
    if (!imports(y->imports))
        return false;
    if (!optModuleDeclaration(y->declaration))
        return false;
    if (!optTopLevelLLVM(y->topLevelLLVM))
        return false;
    if (!topLevelItems(y->topLevelItems, y.ptr()))
        return false;
    x = y.ptr();
    x->location = location;
    return true;
}

//
// REPL
//

static bool replItems(ReplItem &x, bool = false) {
    inRepl = false;
    unsigned p = save();
    if (expression(x.expr) && position == tokens->size()) {
        x.isExprSet = true;
        return true;
    }
    restore(p);

    inRepl = true;
    x.toplevels.clear();
    x.imports.clear();
    x.stmts.clear();
    StatementPtr stmtItem;

    while (true) {
        if (position == tokens->size()) {
            break;
        }

        unsigned i = save();

        if (!topLevelItem(x.toplevels, nullptr)) {
            restore(i);
        } else {
            continue;
        }

        bool sawImportKeyword = false;
        if (!import(x.imports, sawImportKeyword)) {
            restore(i);
        } else {
            continue;
        }

        if (!blockItem(stmtItem)) {
            restore(i);
            break;
        } else {
            x.stmts.push_back(stmtItem);
        }
    }

    x.isExprSet = false;

    return true;
}

//
// parse
//

template <typename Parser, typename ParserParam, typename Node>
void applyParser(const SourcePtr &source, unsigned offset, size_t length,
                 Parser parser, ParserParam parserParam, Node &node) {
    vector<Token> t;
    tokenize(source, offset, length, t);

    tokens = &t;
    parseSource = source;
    position = maxPosition = 0;
    failPosition = 0;
    failExpected.clear();
    parseErrors.clear();
    parseErrorOverflow = false;

    bool ok = parser(node, parserParam);
    if ((!ok || position < t.size()) && parseErrors.empty())
        parseErrors.push_back(buildParseError());

    vector<Diagnostic> errs;
    errs.swap(parseErrors);
    bool overflow = parseErrorOverflow;

    tokens = nullptr;
    parseSource = nullptr;
    position = maxPosition = 0;
    failPosition = 0;
    failExpected.clear();
    parseErrorOverflow = false;

    if (!errs.empty()) {
        // drop exact duplicates from overlapping recovery paths
        vector<Diagnostic> uniq;
        for (Diagnostic const &d : errs) {
            bool dup = false;
            for (Diagnostic const &u : uniq)
                if (u.headline == d.headline &&
                    u.primary.startOffset == d.primary.startOffset)
                    dup = true;
            if (!dup)
                uniq.push_back(d);
        }
        for (Diagnostic const &d : uniq)
            displayDiagnostic(d);
        if (overflow)
            displayDiagnostic(Diagnostic(
                Severity::Note, "too many errors; stopping here", Span()));
        throw CompilerError();
    }
}

struct ModuleParser {
    llvm::StringRef moduleName;
    bool operator()(ModulePtr &m, Module *) const {
        return module(moduleName, m);
    }
};

ModulePtr parse(llvm::StringRef moduleName, const SourcePtr &source,
                ParserFlags flags) {
    if (flags)
        parserOptionKeepDocumentation = true;
    ModulePtr m;
    ModuleParser p = {moduleName};
    applyParser(source, 0, source->size(), p, m.ptr(), m);
    m->source = source;
    return m;
}

//
// parseExpr
//

ExprPtr parseExpr(const SourcePtr &source, unsigned offset, size_t length) {
    ExprPtr expr;
    applyParser(source, offset, length, expression, false, expr);
    return expr;
}

//
// parseExprList
//

ExprListPtr parseExprList(const SourcePtr &source, unsigned offset,
                          size_t length) {
    ExprListPtr exprList;
    applyParser(source, offset, length, expressionList, false, exprList);
    return exprList;
}

//
// parseStatements
//

void parseStatements(const SourcePtr &source, unsigned offset, size_t length,
                     vector<StatementPtr> &stmts) {
    applyParser(source, offset, length, blockItems, false, stmts);
}

//
// parseTopLevelItems
//

void parseTopLevelItems(const SourcePtr &source, unsigned offset, size_t length,
                        vector<TopLevelItemPtr> &topLevels, Module *module) {
    applyParser(source, offset, length, topLevelItems, module, topLevels);
}

//
// parseInteractive
//

ReplItem parseInteractive(const SourcePtr &source, unsigned offset,
                          size_t length) {
    ReplItem x;
    applyParser(source, offset, length, replItems, false, x);
    return x;
}
} // namespace ceramic
