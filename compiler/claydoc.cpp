#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>

#include "claydoc.hpp"
#include "parser.hpp"

#include <sstream>
#include <string>

using namespace std;
using namespace clay;

static void usage(const char *argv0) {
    llvm::errs() << "usage: " << argv0 << " <sourceDir> <htmlOutputDir>\n";
}

DocModule *docParseModule(const string &fileName, DocState *state, const std::string& fqn) {
    SourcePtr src = new Source(fileName);
    ModulePtr m = parse(fileName, src, ParserKeepDocumentation);

    auto *docMod = new DocModule;
    docMod->fileName = fileName;
    docMod->fqn = fqn;

    auto *section = new DocSection;
    docMod->sections.push_back(section);

    DocumentationPtr lastAttachment;

    for (const auto& item : m->topLevelItems) {
        if (!item)
            continue;
        std::string name = identifierString(item->name);

        switch (item->objKind) {
            case DOCUMENTATION: {
                DocumentationPtr doc = dynamic_cast<Documentation *>(item.ptr());
                if (doc->annotation.count(ModuleAnnotation)) {
                    docMod->name = doc->annotation.find(ModuleAnnotation)->second;
                    docMod->description = doc->text;
                } else if (doc->annotation.count(SectionAnnotation)) {
                    section = new DocSection;
                    section->name = doc->annotation.find(SectionAnnotation)->second;
                    section->description = doc->text;
                    docMod->sections.push_back(section);
                } else {
                    lastAttachment = doc;
                }
                break;
            }
            case OVERLOAD:
                if (!!dynamic_cast<Overload *>(item.ptr())->target)
                    name = dynamic_cast<Overload *>(item.ptr())->target->asString();
            case RECORD_DECL:
            case PROCEDURE: {
                auto *obj = new DocObject;
                obj->item = item;
                obj->name = name;
                if (!!lastAttachment) {
                    obj->description = lastAttachment->text;
                    lastAttachment = nullptr;
                }
                section->objects.push_back(obj);

                if (item->objKind != OVERLOAD)
                    state->references.insert(std::pair<std::string, DocModule *>(name, docMod));

                break;
            }
            default: {
            } // make compiler happy
        }
    }
    state->modules.insert(std::pair<std::string, DocModule *>(fqn, docMod));

    return docMod;
}

bool endsWith(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

int main(int argc, char **argv) {
    string inputDir;
    string outputDir;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-help") == 0
            || strcmp(argv[i], "--help") == 0
            || strcmp(argv[i], "/?") == 0) {
            usage(argv[0]);
            return 2;
        } else if (strstr(argv[i], "-") != argv[i]) {
            if (!inputDir.empty()) {
                if (!outputDir.empty()) {
                    usage(argv[0]);
                    return 1;
                }
                outputDir = argv[i];
                continue;
            }
            inputDir = argv[i];
        }
    }

    if (inputDir.empty() || outputDir.empty()) {
        usage(argv[0]);
        return 1;
    }

    bool whatever = false;
    std::error_code ec;

    ec = llvm::sys::fs::create_directories(outputDir, whatever);
    if (ec) {
        llvm::errs() << "cannot create output directory " << outputDir << "\n";
        return 4;
    }

    std::error_code ec2;
    auto *state = new DocState;
    state->name = llvm::sys::path::filename(inputDir).str();

    for (llvm::sys::fs::recursive_directory_iterator it(inputDir, ec2), ite; it != ite; it.increment(ec)) {
        llvm::sys::fs::file_status status;
        if (!it->status() && is_regular_file(status) && endsWith(it->path(), ".clay")) {
            std::string fqn;

            llvm::sys::path::const_iterator word = llvm::sys::path::begin(it->path());
            llvm::sys::path::const_iterator path_end = llvm::sys::path::end(it->path());

            if (word != path_end)
                ++word;

            for (; word != path_end; ++word) {
                llvm::sys::path::const_iterator next_word = word;
                ++next_word;

                if (next_word == path_end)
                    break;

                fqn.append(*word);
                fqn.append(".");
            }

            llvm::StringRef stemR = llvm::sys::path::stem(it->path());
            fqn.append(string(stemR.begin(), stemR.end()));

            std::string fileName = it->path();
            llvm::errs() << "parsing " << fileName << "\n";

            docParseModule(fileName, state, fqn);
        }
    }

    emitHtmlIndex(outputDir, state);
}

std::string identifierString(const IdentifierPtr& id) {
    if (!id)
        return {"<anonymous>"};
    return {id->str.str().begin(), id->str.str().end()};
}
