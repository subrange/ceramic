" Vim syntax file
" Language: ceramic
" Maintainer: Joe Groff <joe@duriansoftware.com>
" Last Change: 2011 Nov 21

" Quit when a custom syntax file was already loaded
if exists("b:current_syntax")
    finish
endif

" Include ! and ? in keyword characters
setlocal iskeyword=33,48-57,63,65-90,95,97-122

syn keyword ceramicKeyword public private import record variant instance define overload external alias inline enum var if else goto return while switch break continue for try catch throw onerror finally

syn keyword ceramicLabelKeyword case default
syn keyword ceramicOperatorKeyword and or not static forward ref as in rvalue

syn keyword ceramicType Bool Int8 Int16 Int32 Int64 Int128 UInt8 UInt16 UInt32 UInt64 UInt128 Float32 Float64 Float80 Float128 Pointer CodePointer RefCodePointer CCodePointer StdCallCodePointer FastCallCodePointer RawPointer OpaquePointer Array Tuple Void Byte UByte Char Short UShort Int UInt Long ULong Float Double RawPointer SizeT PtrInt UPtrInt StringConstant Vec Union Static

syn keyword ceramicBoolean true false

syn keyword ceramicDebug observeTo observe observeCallTo observeCall

syn region ceramicString start=+"+ skip=+\\\\\|\\"+ end=+"+
syn region ceramicIdentifier start=+#"+ skip=+\\\\\|\\"+ end=+"+
syn region ceramicTripleString start=+"""+ skip=+\\\\\|\\"+ end=+""""\@!+
syn region ceramicTripleIdentifier start=+#"""+ skip=+\\\\\|\\"+ end=+""""\@!+

syn region ceramicComment start="/\*" end="\*/"
syn region ceramicComment start="//" end="$"

syn match ceramicDecimal /\.\@<![+\-]\?\<\([0-9][0-9_]*\)\([.][0-9_]*\)\?\([eE][+\-]\?[0-9][0-9_]*\)\?\(ss\|uss\|s\|us\|i\|u\|l\|ll\|ul\|ull\|f\|ff\|fl\|fll\|fj\|j\|ffj\|lj\|flj\|fllj\)\?\w\@!/
syn match ceramicHex /\.\@<![+\-]\?\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\(\([.][0-9A-Fa-f_]*\)\?[pP][+\-]\?[0-9][0-9_]*\)\?\(ss\|uss\|s\|us\|i\|u\|l\|ll\|ul\|ull\|f\|ff\|fl\|fll\|fj\|j\|ffj\|lj\|flj\|fllj\)\?\>/
syn match ceramicSimpleIdentifier /#[A-Za-z_?][A-Za-z0-9_?]*\>/
syn match ceramicChar /'\([^'\\]\|\\\(["'trnf0$\\]\|x[0-9a-fA-F]\{2}\)\)'/
syn match ceramicGotoLabel /^\s*[A-Za-z_?][A-Za-z0-9_?]*\(:\s*$\)\@=/

syn match ceramicMultiValue /\.\.\.\?/
syn match ceramicLambda /=>\|->/

hi def link ceramicKeyword     Statement
hi def link ceramicType        Type
hi def link ceramicBoolean     Boolean
hi def link ceramicComment     Comment
hi def link ceramicTripleString String
hi def link ceramicString      String
hi def link ceramicChar        Character
hi def link ceramicDecimal     Number
hi def link ceramicHex         Number
hi def link ceramicSimpleIdentifier Constant
hi def link ceramicIdentifier Constant
hi def link ceramicTripleIdentifier Constant
hi def link ceramicOperatorKeyword Operator
hi def link ceramicLabelKeyword Label
hi def link ceramicGotoLabel    Label
hi def link ceramicMultiValue   Special
hi def link ceramicLambda       Special
hi def link ceramicDebug        Todo

let b:current_syntax = "ceramic"
