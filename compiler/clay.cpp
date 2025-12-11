#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/Signals.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Error.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/Bitcode/BitcodeWriterPass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/IRPrinter/IRPrintingPasses.h>

#include <system_error>
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "clay.hpp"
#include "hirestimer.hpp"
#include "error.hpp"
#include "codegen.hpp"
#include "loader.hpp"
#include "invoketables.hpp"
#include "parachute.hpp"

using std::string;
using std::vector;

// for _exit
#ifdef _WIN32
# include <process.h>
#else
# include <unistd.h>
#endif

namespace clay {
#ifdef WIN32
#define PATH_SEPARATORS "/\\"
#else
#define PATH_SEPARATORS "/"
#endif

#ifdef WIN32
#define ENV_SEPARATOR ';'
#else
#define ENV_SEPARATOR ':'
#endif

    static bool runModule(llvm::Module *module,
                      const std::vector<std::string> &argv,
                      char const *const*envp,
                      llvm::ArrayRef<std::string> libSearchPaths,
                      llvm::ArrayRef<std::string> libs) {
        auto JIT_expected = llvm::orc::LLJITBuilder().create();
        if (!JIT_expected) {
            llvm::errs() << "error creating JIT: " << llvm::toString(JIT_expected.takeError()) << "\n";
            return false;
        }
        auto JIT = std::move(JIT_expected.get());
        llvm::orc::LLJIT &jit = *JIT;

        llvm::orc::JITDylib& mainDylib = jit.getMainJITDylib();

        auto Generator_expected = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit.getDataLayout().getGlobalPrefix()
        );

        if (!Generator_expected) {
            llvm::errs() << "error creating generator: " << llvm::toString(Generator_expected.takeError()) << "\n";
            return false;
        }

        std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> Generator =
            std::move(*Generator_expected);

        mainDylib.addGenerator(std::move(Generator));

        module->setDataLayout(jit.getDataLayout());

        auto TSM = llvm::orc::ThreadSafeModule(std::unique_ptr<llvm::Module>(module),
                                               std::make_unique<llvm::LLVMContext>());

        if (llvm::Error AddIRErr = jit.addIRModule(std::move(TSM))) {
            llvm::errs() << "error adding module to JIT: " << llvm::toString(std::move(AddIRErr)) << "\n";
            return false;
        }

        auto mainAddr_expected = jit.lookup("main");
        if (!mainAddr_expected) {
            llvm::errs() << "error resolving main: " << llvm::toString(mainAddr_expected.takeError()) << "\n";
            return false;
        }

        using MainPtr = int (*)(int, char *[], char *[]);

        llvm::orc::ExecutorAddr mainAddr = mainAddr_expected.get();
        auto mainFunc = reinterpret_cast<MainPtr>(mainAddr.getValue());

        std::vector<const char*> c_argv;
        for (const auto& arg : argv) {
            c_argv.push_back(arg.c_str());
        }
        c_argv.push_back(nullptr);

        mainFunc(static_cast<int>(c_argv.size() - 1), const_cast<char **>(c_argv.data()), const_cast<char **>(envp));

        return true;
    }

    static void optimizeLLVM(llvm::Module *module, unsigned optLevel, bool internalize) {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;
        llvm::PassBuilder PB;

        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        llvm::OptimizationLevel O = llvm::OptimizationLevel::O0;
        if (optLevel == 1) O = llvm::OptimizationLevel::O1;
        else if (optLevel == 2) O = llvm::OptimizationLevel::O2;
        else if (optLevel >= 3) O = llvm::OptimizationLevel::O3;

        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(O);

        if (optLevel > 2 && internalize) {
            MPM.addPass(llvm::InternalizePass([=](const llvm::GlobalValue &GV) {
                return GV.getName() == "main";
            }));
        }

        MPM.addPass(llvm::VerifierPass());

        MPM.run(*module, MAM);
    }

    static void generateLLVM(llvm::Module *module, bool emitAsm, llvm::raw_ostream *out) {
        llvm::ModuleAnalysisManager MAM;
        llvm::ModulePassManager passes;

        if (emitAsm)
            passes.addPass(llvm::PrintModulePass(*out));
        else
            passes.addPass(llvm::BitcodeWriterPass(*out));

        passes.run(*module, MAM);
    }

    static void generateAssembly(llvm::Module *module,
                                 llvm::TargetMachine *targetMachine,
                                 llvm::raw_pwrite_stream *out,
                                 bool emitObject) {
        llvm::legacy::PassManager passes;

        passes.add(llvm::createVerifierPass());

        llvm::CodeGenFileType fileType = emitObject
                                            ? llvm::CodeGenFileType::ObjectFile
                                            : llvm::CodeGenFileType::AssemblyFile;

        if (targetMachine->addPassesToEmitFile(passes, *out, nullptr, fileType)) {
            llvm::errs() << "error: adding codegen passes failed\n";
            return;
        }

        passes.run(*module);
    }

    [[maybe_unused]] static std::string joinCmdArgs(llvm::ArrayRef<llvm::StringRef> args)
    {
        std::string s;
        llvm::raw_string_ostream ss(s);
        for (const llvm::StringRef &arg : args) {
            if (&arg != args.begin()) {
                ss << " ";
            }
            ss << arg;
        }
        return s;
    }

    static bool generateBinary(llvm::Module *module,
                               llvm::TargetMachine *targetMachine,
                               llvm::Twine const &outputFilePath,
                               llvm::StringRef const &clangPath,
                               bool /*exceptions*/,
                               bool sharedLib,
                               bool debug,
                               llvm::ArrayRef<string> arguments,
                               bool verbose) {
        int fd;
        PathString tempObj;
        if (std::error_code ec = llvm::sys::fs::createUniqueFile("clayobj-%%%%%%%%.obj", fd, tempObj)) {
            llvm::errs() << "error creating temporary object file: " << ec.message() << '\n';
            return false;
        }
        llvm::FileRemover removeTempObj(tempObj);

        {
            llvm::raw_fd_ostream objOut(fd, /*shouldClose=*/ true);

            generateAssembly(module, targetMachine, &objOut, true);
        }

        string outputFilePathStr = outputFilePath.str();

        std::vector<llvm::StringRef> clangArgs;
        clangArgs.emplace_back(clangPath.data());

        switch (llvmDataLayout->getPointerSizeInBits()) {
            case 32:
                clangArgs.emplace_back("-m32");
                break;
            case 64:
                clangArgs.emplace_back("-m64");
                break;
            default:
                assert(false);
        }

        const llvm::Triple& triple(llvmModule->getTargetTriple());
        if (sharedLib) {
            clangArgs.emplace_back("-shared");

            if (triple.isOSWindows()) {
                string linkerFlags;
                PathString defPath;
                outputFilePath.toVector(defPath);
                llvm::sys::path::replace_extension(defPath, "def");

                linkerFlags = "-Wl,--output-def," + string(defPath.begin(), defPath.end());

                clangArgs.emplace_back(linkerFlags);
            }
        }
        if (debug) {
            if (triple.getOS() == llvm::Triple::Win32)
                clangArgs.emplace_back("-Wl,/debug");
        }
        clangArgs.emplace_back("-o");
        clangArgs.emplace_back(outputFilePathStr);
        clangArgs.push_back(tempObj);
        for (const auto & argument : arguments)
            clangArgs.emplace_back(argument);

        if (verbose) {
            llvm::errs() << "executing clang to generate binary:\n";
            llvm::errs() << "    " << joinCmdArgs(clangArgs) << "\n";
        }

        int result = llvm::sys::ExecuteAndWait(clangPath, clangArgs);

        if (debug && triple.isOSDarwin()) {
            llvm::ErrorOr<std::string> dsymutilPathOrErr = llvm::sys::findProgramByName("dsymutil");
            if (std::error_code ec = dsymutilPathOrErr.getError())
                llvm::errs() << "error creating dsymutil: " << ec.message() << '\n';

            std::string dsymutilPath = dsymutilPathOrErr ? *dsymutilPathOrErr : "";

            if (!dsymutilPath.empty()) {
                string outputDSYMPath = outputFilePathStr;
                outputDSYMPath.append(".dSYM");

                std::vector<llvm::StringRef> dsymutilArgs;
                dsymutilArgs.emplace_back(dsymutilPath);
                dsymutilArgs.emplace_back("-o");
                dsymutilArgs.emplace_back(outputDSYMPath);
                dsymutilArgs.emplace_back(outputFilePathStr);

                if (verbose) {
                    llvm::errs() << "executing dsymutil:";
                    llvm::errs() << "    " << joinCmdArgs(dsymutilArgs) << "\n";
                }

                int dsymResult = llvm::sys::ExecuteAndWait(dsymutilPath, dsymutilArgs);

                if (dsymResult != 0)
                    llvm::errs() << "warning: dsymutil exited with error code " << dsymResult << "\n";
            } else
                llvm::errs() <<
                        "warning: unable to find dsymutil on the path; debug info for executable will not be generated\n";
        }

        return (result == 0);
    }

    static void usage(const char *argv0) {
        llvm::errs() << "usage: " << argv0 << " <options> <clay file>\n";
        llvm::errs() << "       " << argv0 << " <options> -e <clay code>\n";
        llvm::errs() << "options:\n";
        llvm::errs() << "  -o <file>             specify output file\n";
        llvm::errs() << "  -target <target>      set target platform for code generation\n";
        llvm::errs() << "  -mcpu <CPU>           set target CPU for code generation\n";
        llvm::errs() << "  -mattr <features>     set target features for code generation\n"
                << "                        use +feature to enable a feature\n"
                << "                        or -feature to disable it\n"
                << "                        for example, -mattr +feature1,-feature2\n";
        llvm::errs() << "  -soft-float           generate software floating point library calls\n";
        llvm::errs() << "  -shared               create a dynamically linkable library\n";
        llvm::errs() << "  -emit-llvm            emit llvm code\n";
        llvm::errs() << "  -S                    emit assembler code\n";
        llvm::errs() << "  -c                    emit object code\n";
        llvm::errs() << "  -DFLAG[=value]        set flag value\n"
                << "                        (queryable with Flag?() and Flag())\n";
        llvm::errs() << "  -O0 -O1 -O2 -O3       set optimization level\n";
        llvm::errs() << "                        (default -O2, or -O0 with -g)\n";
        llvm::errs() << "  -g                    keep debug symbol information\n";
        llvm::errs() << "  -exceptions           enable exception handling\n";
        llvm::errs() << "  -no-exceptions        disable exception handling\n";
        llvm::errs() << "  -inline               inline procedures marked 'forceinline'\n";
        llvm::errs() << "                        and enable 'inline' hints (default)\n";
        llvm::errs() << "  -no-inline            ignore 'inline' and 'forceinline' keyword\n";
        llvm::errs() << "  -import-externals     include externals from imported modules\n"
                << "                        in compilation unit\n"
                << "                        (default when building standalone or -shared)\n";
        llvm::errs() << "  -no-import-externals  don't include externals from imported modules\n"
                << "                        in compilation unit\n"
                << "                        (default when building -c or -S)\n";
        llvm::errs() << "  -pic                  generate position independent code\n";
        llvm::errs() << "  -run                  execute the program without writing to disk\n";
        llvm::errs() << "  -timing               show timing information\n";
        llvm::errs() << "  -verbose              be verbose\n";
        llvm::errs() << "  -full-match-errors    show universal patterns in match failure errors\n";
        llvm::errs() << "  -log-match <module.symbol>\n"
                << "                        log overload matching behavior for calls to <symbol>\n"
                << "                        in module <module>\n";
#ifdef __APPLE__
        llvm::errs() << "  -arch <arch>          build for Darwin architecture <arch>\n";
        llvm::errs() << "  -F<dir>               add <dir> to framework search path\n";
        llvm::errs() << "  -framework <name>     link with framework <name>\n";
#endif
        llvm::errs() << "  -L<dir>               add <dir> to library search path\n";
        llvm::errs() << "  -Wl,<opts>            pass flags to linker\n";
        llvm::errs() << "  -l<lib>               link with library <lib>\n";
        llvm::errs() << "  -I<path>              add <path> to clay module search path\n";
        llvm::errs() << "  -deps                 keep track of the dependencies of the currently\n";
        llvm::errs() << "                        compiling file and write them to the file\n";
        llvm::errs() << "                        specified by -o-deps\n";
        llvm::errs() << "  -no-deps              don't generate dependencies file\n";
        llvm::errs() << "  -o-deps <file>        write the dependencies to this file\n";
        llvm::errs() << "                        (defaults to <compilation output file>.d)\n";
        llvm::errs() << "  -e <source>           compile and run <source> (implies -run)\n";
        llvm::errs() << "  -M<module>            \"import <module>.*;\" for -e\n";
        llvm::errs() << "  -version              display version info\n";

        llvm::errs() << "  -final-overloads      enable final overloads (temporary option)\n";
    }

    static string sharedExtensionForTarget(llvm::Triple const &triple) {
        if (triple.isOSWindows()) {
            return ".dll";
        }
        if (triple.isOSDarwin()) {
            return ".dylib";
        }
        return ".so";
    }

    static string objExtensionForTarget(llvm::Triple const &triple) {
        if (triple.isOSWindows()) {
            return ".obj";
        }
        return ".o";
    }

    static string exeExtensionForTarget(llvm::Triple const &triple) {
        if (triple.isOSWindows()) {
            return ".exe";
        }
        return "";
    }

    static void printVersion() {
        llvm::errs() << "clay compiler version " CLAY_COMPILER_VERSION
                ", language version " CLAY_LANGUAGE_VERSION " ("
#ifdef GIT_ID
         << "git id " << GIT_ID << ", "
#endif
#ifdef HG_ID
         << "hg id " << HG_ID << ", "
#endif
#ifdef SVN_REVISION
         << "llvm r" << SVN_REVISION << ", "
#endif
                << __DATE__ << ")\n";
    }

    int main2(int argc, char **argv, char const *const*envp) {
        if (argc == 1) {
            usage(argv[0]);
            return 2;
        }

        bool emitLLVM = false;
        bool emitAsm = false;
        bool emitObject = false;
        bool sharedLib = false;
        bool genPIC = false;
        bool inlineEnabled = true;
        bool exceptions = true;
        bool run = false;
        bool repl = false;
        bool verbose = false;
        bool crossCompiling = false;
        bool showTiming = false;
        bool codegenExternals = false;
        bool codegenExternalsSet = false;

        bool generateDeps = false;

        unsigned optLevel = 2;
        bool optLevelSet = false;

        bool finalOverloadsEnabled = false;
        bool softFloat = false;

#ifdef __APPLE__
        genPIC = true;

        string arch;
#endif

        string clayFile;
        string outputFile;
        string targetTriple = llvm::sys::getDefaultTargetTriple();

        string targetCPU;
        string targetFeatures;

        string clayScriptImports;
        string clayScript;

        vector<string> libSearchPathArgs;
        vector<string> libSearchPath;
        string linkerFlags;
        vector<string> librariesArgs;
        vector<string> libraries;
        vector<PathString> searchPath;

        string dependenciesOutputFile;
#ifdef __APPLE__
        vector<string> frameworkSearchPath;
        vector<string> frameworks;
#endif

        bool debug = false;

        for (int i = 1; i < argc; ++i) {
            if ((strcmp(argv[i], "-shared") == 0)) {
                sharedLib = true;
            } else if (strcmp(argv[i], "-emit-llvm") == 0) {
                emitLLVM = true;
            } else if (strcmp(argv[i], "-S") == 0) {
                emitAsm = true;
            } else if (strcmp(argv[i], "-c") == 0) {
                emitObject = true;
            } else if (strcmp(argv[i], "-g") == 0) {
                debug = true;
                if (!optLevelSet)
                    optLevel = 0;
            } else if (strcmp(argv[i], "-O0") == 0) {
                optLevel = 0;
                optLevelSet = true;
            } else if (strcmp(argv[i], "-O1") == 0) {
                optLevel = 1;
                optLevelSet = true;
            } else if (strcmp(argv[i], "-O2") == 0) {
                optLevel = 2;
                optLevelSet = true;
            } else if (strcmp(argv[i], "-O3") == 0) {
                optLevel = 3;
                optLevelSet = true;
            } else if (strcmp(argv[i], "-inline") == 0) {
                inlineEnabled = true;
            } else if (strcmp(argv[i], "-no-inline") == 0) {
                inlineEnabled = false;
            } else if (strcmp(argv[i], "-exceptions") == 0) {
                exceptions = true;
            } else if (strcmp(argv[i], "-no-exceptions") == 0) {
                exceptions = false;
            } else if (strcmp(argv[i], "-pic") == 0) {
                genPIC = true;
            } else if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "-v") == 0) {
                verbose = true;
            } else if (strcmp(argv[i], "-run") == 0) {
                run = true;
            } else if (strcmp(argv[i], "-repl") == 0) {
                repl = true;
            } else if (strcmp(argv[i], "-timing") == 0) {
                showTiming = true;
            } else if (strcmp(argv[i], "-full-match-errors") == 0) {
                shouldPrintFullMatchErrors = true;
            } else if (strcmp(argv[i], "-log-match") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: symbol name missing after -log-match\n";
                    return 1;
                }
                ++i;
                char const *dot = strrchr(argv[i], '.');
                if (dot == nullptr) {
                    logMatchSymbols.insert(make_pair(string("*"), argv[i]));
                } else {
                    logMatchSymbols.insert(make_pair(string(static_cast<char const *>(argv[i]), dot), string(dot + 1)));
                }
            } else if (strcmp(argv[i], "-e") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: source string missing after -e\n";
                    return 1;
                }
                ++i;
                run = true;
                clayScript += argv[i];
                clayScript += "\n";
            } else if (strncmp(argv[i], "-M", 2) == 0) {
                string modulespec = argv[i] + 2;
                if (modulespec.empty()) {
                    llvm::errs() << "error: module missing after -M\n";
                    return 1;
                }
                clayScriptImports += "import " + modulespec + ".*; ";
            } else if (strcmp(argv[i], "-o") == 0) {
                ++i;
                if (i == argc) {
                    llvm::errs() << "error: filename missing after -o\n";
                    return 1;
                }
                if (!outputFile.empty()) {
                    llvm::errs() << "error: output file already specified: "
                            << outputFile
                            << ", specified again as " << argv[i] << '\n';
                    return 1;
                }
                outputFile = argv[i];
            }
#ifdef __APPLE__
            else if (strstr(argv[i], "-F") == argv[i]) {
                string frameworkDir = argv[i] + strlen("-F");
                if (frameworkDir.empty()) {
                    if (i + 1 == argc) {
                        llvm::errs() << "error: directory missing after -F\n";
                        return 1;
                    }
                    ++i;
                    frameworkDir = argv[i];
                    if (frameworkDir.empty() || (frameworkDir[0] == '-')) {
                        llvm::errs() << "error: directory missing after -F\n";
                        return 1;
                    }
                }
                frameworkSearchPath.push_back("-F" + frameworkDir);
            } else if (strcmp(argv[i], "-framework") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: framework name missing after -framework\n";
                    return 1;
                }
                ++i;
                string framework = argv[i];
                if (framework.empty() || (framework[0] == '-')) {
                    llvm::errs() << "error: framework name missing after -framework\n";
                    return 1;
                }
                frameworks.emplace_back("-framework");
                frameworks.push_back(framework);
            } else if (strcmp(argv[i], "-arch") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: architecture name missing after -arch\n";
                    return 1;
                }
                ++i;
                if (!arch.empty()) {
                    llvm::errs() << "error: multiple -arch flags currently unsupported\n";
                    return 1;
                }
                arch = argv[i];
                if (arch.empty() || (arch[0] == '-')) {
                    llvm::errs() << "error: architecture name missing after -arch\n";
                    return 1;
                }

                if (arch == "i386") {
                    targetTriple = "i386-apple-darwin10";
                } else if (arch == "x86_64") {
                    targetTriple = "x86_64-apple-darwin10";
                } else if (arch == "ppc") {
                    targetTriple = "powerpc-apple-darwin10";
                } else if (arch == "ppc64") {
                    targetTriple = "powerpc64-apple-darwin10";
                } else if (arch == "armv6") {
                    targetTriple = "armv6-apple-darwin4.1-iphoneos";
                } else if (arch == "armv7") {
                    targetTriple = "thumbv7-apple-darwin4.1-iphoneos";
                } else {
                    llvm::errs() << "error: unrecognized -arch value " << arch << "\n";
                    return 1;
                }
            }
#endif
            else if (strcmp(argv[i], "-target") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: target name missing after -target\n";
                    return 1;
                }
                ++i;
                targetTriple = argv[i];
                if (targetTriple.empty() || (targetTriple[0] == '-')) {
                    llvm::errs() << "error: target name missing after -target\n";
                    return 1;
                }
                crossCompiling = targetTriple != llvm::sys::getDefaultTargetTriple();
            } else if (strcmp(argv[i], "-mcpu") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: CPU name missing after -mcpu\n";
                    return 1;
                }
                ++i;
                targetCPU = argv[i];
                if (targetCPU.empty() || (targetCPU[0] == '-')) {
                    llvm::errs() << "error: CPU name missing after -mcpu\n";
                    return 1;
                }
            } else if (strcmp(argv[i], "-mattr") == 0) {
                if (i + 1 == argc) {
                    llvm::errs() << "error: features missing after -mattr\n";
                    return 1;
                }
                ++i;
                targetFeatures = argv[i];
                if (targetFeatures.empty() || (targetFeatures[0] == '-')) {
                    llvm::errs() << "error: features missing after -mattr\n";
                    return 1;
                }
            } else if (strcmp(argv[i], "-soft-float") == 0) {
                softFloat = true;
            } else if (strstr(argv[i], "-Wl") == argv[i]) {
                linkerFlags += argv[i] + strlen("-Wl");
            } else if (strstr(argv[i], "-L") == argv[i]) {
                string libDir = argv[i] + strlen("-L");
                if (libDir.empty()) {
                    if (i + 1 == argc) {
                        llvm::errs() << "error: directory missing after -L\n";
                        return 1;
                    }
                    ++i;
                    libDir = argv[i];
                    if (libDir.empty() || (libDir[0] == '-')) {
                        llvm::errs() << "error: directory missing after -L\n";
                        return 1;
                    }
                }
                libSearchPath.push_back(libDir);
                libSearchPathArgs.push_back("-L" + libDir);
            } else if (strstr(argv[i], "-l") == argv[i]) {
                string lib = argv[i] + strlen("-l");
                if (lib.empty()) {
                    if (i + 1 == argc) {
                        llvm::errs() << "error: library missing after -l\n";
                        return 1;
                    }
                    ++i;
                    lib = argv[i];
                    if (lib.empty() || (lib[0] == '-')) {
                        llvm::errs() << "error: library missing after -l\n";
                        return 1;
                    }
                }
                libraries.push_back(lib);
                librariesArgs.push_back("-l" + lib);
            } else if (strstr(argv[i], "-D") == argv[i]) {
                char *namep = argv[i] + strlen("-D");
                if (namep[0] == '\0') {
                    if (i + 1 == argc) {
                        llvm::errs() << "error: definition missing after -D\n";
                        return 1;
                    }
                    ++i;
                    namep = argv[i];
                    if (namep[0] == '\0' || namep[0] == '-') {
                        llvm::errs() << "error: definition missing after -D\n";
                        return 1;
                    }
                }
                char *equalSignp = strchr(namep, '=');
                string name = equalSignp == nullptr
                                  ? string(namep)
                                  : string(namep, equalSignp);
                string value = equalSignp == nullptr
                                   ? string()
                                   : string(equalSignp + 1);

                globalFlags[name] = value;
            } else if (strstr(argv[i], "-I") == argv[i]) {
                string path = argv[i] + strlen("-I");
                if (path.empty()) {
                    if (i + 1 == argc) {
                        llvm::errs() << "error: path missing after -I\n";
                        return 1;
                    }
                    ++i;
                    path = argv[i];
                    if (path.empty() || (path[0] == '-')) {
                        llvm::errs() << "error: path missing after -I\n";
                        return 1;
                    }
                }
                searchPath.emplace_back(path);
            } else if (strstr(argv[i], "-version") == argv[i]
                       || strcmp(argv[i], "--version") == 0) {
                printVersion();
                return 0;
            } else if (strcmp(argv[i], "-import-externals") == 0) {
                codegenExternals = true;
                codegenExternalsSet = true;
            } else if (strcmp(argv[i], "-no-import-externals") == 0) {
                codegenExternals = false;
                codegenExternalsSet = true;
            } else if (strcmp(argv[i], "-deps") == 0) {
                generateDeps = true;
            } else if (strcmp(argv[i], "-no-deps") == 0) {
                generateDeps = false;
            } else if (strcmp(argv[i], "-o-deps") == 0) {
                ++i;
                if (i == argc) {
                    llvm::errs() << "error: filename missing after -o-deps\n";
                    return 1;
                }
                if (!dependenciesOutputFile.empty()) {
                    llvm::errs() << "error: dependencies output file already specified: "
                            << dependenciesOutputFile
                            << ", specified again as " << argv[i] << '\n';
                    return 1;
                }
                dependenciesOutputFile = argv[i];
            } else if (strcmp(argv[i], "--") == 0) {
                ++i;
                if (clayFile.empty()) {
                    if (i != argc - 1) {
                        llvm::errs() << "error: clay file already specified: " << argv[i]
                                << ", unrecognized parameter: " << argv[i + 1] << '\n';
                        return 1;
                    }
                    clayFile = argv[i];
                } else {
                    if (i != argc) {
                        llvm::errs() << "error: clay file already specified: " << clayFile
                                << ", unrecognized parameter: " << argv[i] << '\n';
                        return 1;
                    }
                }
            } else if (strcmp(argv[i], "-help") == 0
                       || strcmp(argv[i], "--help") == 0
                       || strcmp(argv[i], "/?") == 0) {
                usage(argv[0]);
                return 2;
            } else if (strstr(argv[i], "-") != argv[i]) {
                if (!clayFile.empty()) {
                    llvm::errs() << "error: clay file already specified: " << clayFile
                            << ", unrecognized parameter: " << argv[i] << '\n';
                    return 1;
                }
                clayFile = argv[i];
            } else if (strcmp(argv[i], "-final-overloads") == 0) {
                finalOverloadsEnabled = true;
            } else {
                llvm::errs() << "error: unrecognized option " << argv[i] << '\n';
                return 1;
            }
        }

        if (verbose) {
            printVersion();
        }

        if (repl && clayScript.empty() && clayFile.empty()) {
            clayScript = "/*empty module if file not specified*/";
        } else {
            if (clayScript.empty() && clayFile.empty()) {
                llvm::errs() << "error: clay file not specified\n";
                return 1;
            }
            if (!clayScript.empty() && !clayFile.empty()) {
                llvm::errs() << "error: -e cannot be specified with input file\n";
                return 1;
            }
        }

        if (!clayScriptImports.empty() && clayScript.empty()) {
            llvm::errs() << "error: -M specified without -e\n";
        }

        if (emitAsm && emitObject) {
            llvm::errs() << "error: -S or -c cannot be used together\n";
            return 1;
        }

        if (crossCompiling && run) {
            llvm::errs() << "error: cannot use -run when cross compiling\n";
            return 1;
        }

        if (crossCompiling && !(emitLLVM || emitAsm || emitObject)
#ifdef __APPLE__
            && arch.empty()
#endif
        ) {
            llvm::errs() << "error: must use -emit-llvm, -S, or -c when cross compiling\n";
            return 1;
        }

        if (!codegenExternalsSet)
            codegenExternals = !(emitLLVM || emitAsm || emitObject);

        if ((emitLLVM || emitAsm || emitObject) && run)
            run = false;

        setInlineEnabled(inlineEnabled);
        setExceptionsEnabled(exceptions);

        setFinalOverloadsEnabled(finalOverloadsEnabled);

        llvm::Triple llvmTriple(targetTriple);
        targetTriple = llvmTriple.str();

        std::string moduleName = clayScript.empty() ? clayFile : "-e";

        llvm::TargetMachine *targetMachine = initLLVM(targetTriple,
                                                      targetCPU, targetFeatures, softFloat, moduleName, "",
                                                      (sharedLib || genPIC), debug, optLevel);
        if (targetMachine == nullptr) {
            llvm::errs() << "error: unable to initialize LLVM for target " << targetTriple << "\n";
            return 1;
        }

        initTypes();
        initExternalTarget(targetTriple);

        // Try environment variables first
        if (char *libclayPath = getenv("CLAY_PATH")) {
            // Parse the environment variable
            // Format expected is standard PATH form, i.e.
            // CLAY_PATH=path1:path2:path3  (on Unix)
            // CLAY_PATH=path1;path2;path3  (on Windows)
            char *begin = libclayPath;
            char *end;
            do {
                end = begin;
                while (*end && (*end != ENV_SEPARATOR))
                    ++end;
                searchPath.emplace_back(llvm::StringRef(begin, static_cast<size_t>(end - begin)));
                begin = end + 1;
            } while (*end);
        }
        // Add the relative path from the executable
        PathString clayExe(llvm::sys::fs::getMainExecutable(argv[0], reinterpret_cast<void *>(&usage)));
        llvm::StringRef clayDir = llvm::sys::path::parent_path(clayExe);

        PathString libDirDevelopment(clayDir);
        PathString libDirProduction1(clayDir);
        PathString libDirProduction2(clayDir);

        llvm::sys::path::append(libDirDevelopment, "../../lib-clay");
        llvm::sys::path::append(libDirProduction1, "../lib/lib-clay");
        llvm::sys::path::append(libDirProduction2, "lib-clay");

        searchPath.push_back(libDirDevelopment);
        searchPath.push_back(libDirProduction1);
        searchPath.push_back(libDirProduction2);
        searchPath.emplace_back(".");

        if (verbose) {
            llvm::errs() << "using search path:\n";

            for (const auto & it : searchPath) {
                llvm::errs() << "    " << it << "\n";
            }
        }

        setSearchPath(searchPath);

        if (outputFile.empty()) {
            llvm::StringRef clayFileBasename = llvm::sys::path::stem(clayFile);
            outputFile = string(clayFileBasename.begin(), clayFileBasename.end());

            if (emitLLVM && emitAsm)
                outputFile += ".ll";
            else if (emitAsm)
                outputFile += ".s";
            else if (emitObject || emitLLVM)
                outputFile += objExtensionForTarget(llvmTriple);
            else if (sharedLib)
                outputFile += sharedExtensionForTarget(llvmTriple);
            else
                outputFile += exeExtensionForTarget(llvmTriple);
        }
        if (!run) {
            bool isDir;
            if (!llvm::sys::fs::is_directory(outputFile, isDir) && isDir) {
                llvm::errs() << "error: output file '" << outputFile << "' is a directory\n";
                return 1;
            }
            llvm::sys::RemoveFileOnSignal(outputFile);
        }

        if (generateDeps) {
            if (run) {
                llvm::errs() << "error: '-deps' can not be used together with '-e' or '-run'\n";
                return 1;
            }
            if (dependenciesOutputFile.empty()) {
                dependenciesOutputFile = outputFile;
                dependenciesOutputFile += ".d";
            }
        }

        if (generateDeps) {
            bool isDir;
            if (!llvm::sys::fs::is_directory(dependenciesOutputFile, isDir) && isDir) {
                llvm::errs() << "error: dependencies output file '" << dependenciesOutputFile << "' is a directory\n";
                return 1;
            }
            llvm::sys::RemoveFileOnSignal(dependenciesOutputFile);
        }

        HiResTimer loadTimer, compileTimer, optTimer, outputTimer;

        //compiler

        loadTimer.start();
        try {
            initLoader();

            ModulePtr m;
            vector<string> sourceFiles;
            if (!clayScript.empty()) {
                string clayScriptSource;
                clayScriptSource = clayScriptImports + "main() {\n" + clayScript + "}";
                m = loadProgramSource("-e", clayScriptSource, verbose, repl);
            } else if (generateDeps)
                m = loadProgram(clayFile, &sourceFiles, verbose, repl);
            else
                m = loadProgram(clayFile, nullptr, verbose, repl);

            loadTimer.stop();
            compileTimer.start();
            codegenEntryPoints(m, codegenExternals);
            compileTimer.stop();

            if (generateDeps) {
                std::error_code ec;

                if (verbose) {
                    llvm::errs() << "generating dependencies into " << dependenciesOutputFile << "\n";
                }

                llvm::raw_fd_ostream dependenciesOut(dependenciesOutputFile,
                                                     ec,
                                                     llvm::sys::fs::OF_None);
                if (ec) {
                    llvm::errs() << "error creating dependencies file: " << ec.message() << '\n';
                    return 1;
                }
                dependenciesOut << outputFile << ": \\\n";
                for (size_t i = 0; i < sourceFiles.size(); ++i) {
                    dependenciesOut << "  " << sourceFiles[i];
                    if (i < sourceFiles.size() - 1)
                        dependenciesOut << " \\\n";
                }
            }

            bool internalize = true;
            if (debug || sharedLib || run || !codegenExternals)
                internalize = false;

            optTimer.start();

            if (!repl) {
                if (optLevel > 0)
                    optimizeLLVM(llvmModule, optLevel, internalize);
            }
            optTimer.stop();

            if (run) {
                vector<string> runArgs;
                runArgs.push_back(clayFile);
                runModule(llvmModule, runArgs, envp, libSearchPath, libraries);
            } else if (repl) {
                // TODO: future me task
                runInteractive(llvmModule, m);
            } else if (emitLLVM || emitAsm || emitObject) {
                std::error_code ec;

                llvm::raw_fd_ostream out(outputFile,
                                         ec,
                                         llvm::sys::fs::OF_None);
                if (ec) {
                    llvm::errs() << "error creating output file: " << ec.message() << '\n';
                    return 1;
                }
                outputTimer.start();
                if (emitLLVM)
                    generateLLVM(llvmModule, emitAsm, &out);
                else if (emitAsm || emitObject)
                    generateAssembly(llvmModule, targetMachine, &out, emitObject);
                outputTimer.stop();
            } else {
                bool result;
                llvm::ErrorOr<std::string> clangPathOrErr = llvm::sys::findProgramByName("clang");
                if (std::error_code ec = clangPathOrErr.getError()) {
                    llvm::errs() << "error: unable to find clang on the path: " << ec.message() << "\n";
                    return 1;
                }
                const std::string& clangPath = clangPathOrErr.get();

                vector<string> arguments;
#ifdef __APPLE__
                if (!arch.empty()) {
                    arguments.emplace_back("-arch");
                    arguments.push_back(arch);
                }
#endif
                if (!linkerFlags.empty())
                    arguments.push_back("-Wl" + linkerFlags);
#ifdef __APPLE__
                copy(
                    frameworkSearchPath.begin(),
                    frameworkSearchPath.end(),
                    back_inserter(arguments)
                );
                copy(frameworks.begin(), frameworks.end(), back_inserter(arguments));
#endif
                copy(
                    libSearchPathArgs.begin(),
                    libSearchPathArgs.end(),
                    back_inserter(arguments)
                );
                copy(librariesArgs.begin(), librariesArgs.end(), back_inserter(arguments));

                outputTimer.start();
                result = generateBinary(llvmModule, targetMachine, outputFile, clangPath,
                                        exceptions, sharedLib, debug, arguments, verbose);
                outputTimer.stop();
                if (!result)
                    return 1;
            }
        } catch (const CompilerError &) {
            return 1;
        }
        if (showTiming) {
            llvm::errs() << "load time = " << static_cast<size_t>(loadTimer.elapsedMillis()) << " ms\n";
            llvm::errs() << "compile time = " << static_cast<size_t>(compileTimer.elapsedMillis()) << " ms\n";
            llvm::errs() << "optimization time = " << static_cast<size_t>(optTimer.elapsedMillis()) << " ms\n";
            llvm::errs() << "codegen time = " << static_cast<size_t>(outputTimer.elapsedMillis()) << " ms\n";
            llvm::errs().flush();
        }

        _exit(0);
    }
}

int main(int argc, char **argv, char const *const*envp) {
    return clay::parachute(clay::main2, argc, argv, envp);
}
