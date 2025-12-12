#include <llvm/Support/Error.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "clay.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "error.hpp"
#include "loader.hpp"
#include "invoketables.hpp"
#include "env.hpp"

#include <csetjmp>
#include <csignal>
#include <cstdio>

namespace clay {
    typedef llvm::SmallString<16U> Str;

    const char *replAnonymousFunctionName = "__replAnonymousFunction__";

    static ModulePtr module;
    static std::unique_ptr<llvm::orc::LLJIT> jit;

    jmp_buf recovery;

    static bool printAST = false;

    static void eval(llvm::StringRef line);

    string newFunctionName() {
        static int funNum = 0;
        string buf;
        llvm::raw_string_ostream funName(buf);
        funName << replAnonymousFunctionName << funNum;
        ++funNum;
        return funName.str();
    }

    string stripSpaces(string const &s) {
        size_t i;
        for (i = 0; i < s.size(); ++i) {
            if (!isSpace(s[i]))
                break;
        }
        return s.substr(i, s.length());
    }

    static vector<Token> addTokens() {
        char buf[255];
        string line = fgets(buf, 255, stdin);
        line = stripSpaces(line);
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        tokenize(source, 0, line.length(), tokens);
        return tokens;
    }

    static void cmdGlobals(const vector<Token> &tokens) {
        ModulePtr m;
        if (tokens.size() == 1) {
            m = module;
        } else if (tokens.size() == 2) {
            m = globalModules[tokens[1].str];
        } else {
            llvm::errs() << ":globals [module name]\n";
            return;
        }

        string buf;
        llvm::raw_string_ostream code(buf);

        llvm::StringMap<ObjectPtr>::const_iterator g = m->globals.begin();
        for (; g != m->globals.end(); ++g) {
            if (g->second->objKind == GLOBAL_VARIABLE) {
                IdentifierPtr name = ((GlobalVariable *) g->second.ptr())->name;
                code << "println(\"" << name->str << " : \", "
                        << "Type(" << name->str << "), \" : \", "
                        << name->str << ");\n";
            }
        }
        code.flush();
        eval(buf);
    }

    static void cmdModules(const vector<Token> &tokens) {
        if (tokens.size() != 1) {
            llvm::errs() << "Warning: command parameters are ignored\n";
        }
        llvm::StringMap<ModulePtr>::const_iterator iter = globalModules.begin();
        for (; iter != globalModules.end(); ++iter) {
            llvm::errs() << iter->getKey() << "\n";
        }
    }

    static void cmdOverloads(const vector<Token> &tokens) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i].tokenKind == T_IDENTIFIER) {
                Str identStr = tokens[i].str;

                ObjectPtr obj = lookupPrivate(module, Identifier::get(identStr));
                if (obj == nullptr || obj->objKind != PROCEDURE) {
                    llvm::errs() << identStr << " is not a procedure name\n";
                    continue;
                }

                vector<InvokeSet *> sets = lookupInvokeSets(obj.ptr());
                for (size_t k = 0; k < sets.size(); ++k) {
                    llvm::errs() << "        ";
                    for (size_t l = 0; l < sets[k]->argsKey.size(); ++l) {
                        llvm::errs() << sets[k]->argsKey[l]->toString() << " : ";
                    }
                    llvm::errs() << "\n";
                }
            }
        }
    }

    static void cmdPrint(const vector<Token> &tokens) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i].tokenKind == T_IDENTIFIER) {
                Str identifier = tokens[i].str;
                llvm::StringMap<ImportSet>::const_iterator iter = module->allSymbols.find(identifier);
                if (iter == module->allSymbols.end()) {
                    llvm::errs() << "Can't find identifier " << identifier.c_str();
                } else {
                    for (size_t i = 0; i < iter->second.size(); ++i) {
                        llvm::errs() << iter->second[i]->toString() << "\n";
                    }
                }
            }
        }
    }

    static void replCommand(string const &line) {
        SourcePtr source = new Source(line, 0);
        vector<Token> tokens;
        //TODO: don't use compiler's tokenizer
        tokenize(source, 0, line.length(), tokens);
        Str cmd = tokens[0].str;
        if (cmd == "q") {
            exit(0);
        } else if (cmd == "globals") {
            cmdGlobals(tokens);
        } else if (cmd == "modules") {
            cmdModules(tokens);
        } else if (cmd == "overloads") {
            cmdOverloads(tokens);
        } else if (cmd == "print") {
            cmdPrint(tokens);
        } else if (cmd == "ast_on") {
            printAST = true;
        } else if (cmd == "ast_off") {
            printAST = false;
        } else if (cmd == "rebuild") {
            //TODO : this command should re-codegen everything
        } else {
            llvm::errs() << "Unknown interactive mode command: " << cmd.c_str() << "\n";
        }
    }

    static void loadImports(const llvm::ArrayRef<ImportPtr> imports) {
        for (const auto & import : imports) {
            module->imports.push_back(import);
        }
        for (const auto & import : imports) {
            loadDependent(module, nullptr, import, false);
        }
        for (const auto & import : imports) {
            initModule(import->module);
        }
    }

    static void jitTopLevel(llvm::ArrayRef<TopLevelItemPtr> toplevels) {
        if (toplevels.empty()) {
            return;
        }
        if (printAST) {
            for (size_t i = 0; i < toplevels.size(); ++i) {
                llvm::errs() << i << ": " << toplevels[i]->toString() << "\n";
            }
        }
        addGlobals(module, toplevels);
    }

    static void jitStatements(llvm::ArrayRef<StatementPtr> statements) {
        if (statements.empty()) {
            return;
        }

        if (printAST) {
            for (const auto & statement : statements) {
                llvm::errs() << statement->toString() << "\n";
            }
        }

        IdentifierPtr fun = Identifier::get(newFunctionName());

        BlockPtr funBody = new Block(statements);
        ExternalProcedurePtr entryProc =
                new ExternalProcedure(nullptr,
                                      fun,
                                      PRIVATE,
                                      vector<ExternalArgPtr>(),
                                      false,
                                      nullptr,
                                      funBody.ptr(),
                                      new ExprList());

        entryProc->env = module->env;

        codegenBeforeRepl(module);
        try {
            codegenExternalProcedure(entryProc, true);
        } catch (std::exception) {
            return;
        }

        llvm::Function *ctor;
        llvm::Function *dtor;
        codegenAfterRepl(ctor, dtor);

        // hacky voodoo raw memory address returned by LLJIT symbol lookup
        // https://llvm.org/docs/ORCv2.html
        using CtorPtr = void (*)();
        using PFN = void (*)();

        auto ctorAddExpected = jit->lookup(ctor->getName());
        if (!ctorAddExpected) {
            llvm::errs() << "error: cannot look up constructor: " << llvm::toString(ctorAddExpected.takeError()) << "\n";
            return;
        }
        CtorPtr ctorFunc = reinterpret_cast<CtorPtr>(ctorAddExpected->getValue());
        ctorFunc();

        auto dtorAddExpected = jit->lookup(dtor->getName());
        if (!dtorAddExpected) {
            llvm::errs() << "error: cannot look up destructor: " << llvm::toString(dtorAddExpected.takeError()) << "\n";
            return;
        }
        void *dtorLlvmFun = reinterpret_cast<void *>(dtorAddExpected->getValue());
        atexit((PFN) (uintptr_t) dtorLlvmFun);

        auto entryAddrExpected = jit->lookup(entryProc->llvmFunc->getName());
        if (!entryAddrExpected) {
            llvm::errs() << "error: cannot look up entry function: " << llvm::toString(entryAddrExpected.takeError()) << "\n";
            return;
        }
        PFN entryFunc = reinterpret_cast<PFN>(entryAddrExpected->getValue());
        entryFunc();
    }

    static void jitAndPrintExpr(ExprPtr expr) {
        //expr -> println(expr);
        NameRefPtr println = new NameRef(Identifier::get("println"));
        ExprPtr call = new Call(println.ptr(), new ExprList(expr));
        ExprStatementPtr callStmt = new ExprStatement(call);
        jitStatements(vector<StatementPtr>(1, callStmt.ptr()));
    }

    static void eval(llvm::StringRef line) {
        SourcePtr source = new Source(line, 0);
        try {
            ReplItem x = parseInteractive(source, 0, source->size());
            if (x.isExprSet) {
                jitAndPrintExpr(x.expr);
            } else {
                loadImports(x.imports);
                jitTopLevel(x.toplevels);
                jitStatements(x.stmts);
            }
        } catch (CompilerError) {
            return;
        }
    }

    [[noreturn]] static void interactiveLoop() {
        setjmp(recovery);
        while (true) {
            llvm::errs().flush();
            llvm::errs() << "clay>";
            char buf[255];
            string line = fgets(buf, 255, stdin);
            line = stripSpaces(line);
            if (line[0] == ':') {
                replCommand(line.substr(1, line.size() - 1));
            } else {
                eval(line);
            }
        }
    }

    static void exceptionHandler(int i) {
        llvm::errs() << "SIGABRT called\n";
        longjmp(recovery, 1);
    }

    void runInteractive(llvm::Module *llvmModule_, ModulePtr module_) {
        signal(SIGABRT, exceptionHandler);

        llvmModule = llvmModule_;
        module = module_;
        llvm::errs() << "Clay interpreter\n";
        llvm::errs() << ":q to exit\n";
        llvm::errs() << ":print {identifier} to print an identifier\n";
        llvm::errs() << ":modules to list global modules\n";
        llvm::errs() << ":globals to list globals\n";
        llvm::errs() << "In multi-line mode empty line to exit\n";

        auto expectedJIT = llvm::orc::LLJITBuilder().create();
        if (!expectedJIT) {
            llvm::errs() << "error: could not create JIT: " << llvm::toString(expectedJIT.takeError()) << "\n";
            return;
        }
        jit = std::move(*expectedJIT);

        llvm::orc::JITDylib& mainDylib = jit->getMainJITDylib();

        auto generatorExpected = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix()
        );

        if (!generatorExpected) {
            llvm::errs() << "error: setting up symbol" << llvm::toString(generatorExpected.takeError()) << "\n";
        } else {
            std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> generator =
                std::move(*generatorExpected);
            mainDylib.addGenerator(std::move(generator));
        }

        llvmModule->setDataLayout(jit->getDataLayout());

        auto tsm = llvm::orc::ThreadSafeModule(
            std::unique_ptr<llvm::Module>(llvmModule),
            std::make_unique<llvm::LLVMContext>());

        if (llvm::Error addIRErr = jit->addIRModule(std::move(tsm))) {
            llvm::errs() << "error: " << addIRErr << "\n";
            return;
        }

        setAddTokens(&addTokens);

        interactiveLoop();
    }
}
