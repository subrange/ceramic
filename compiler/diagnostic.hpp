#pragma once

#include "ceramic.hpp"

namespace ceramic {

enum class Severity { Error, Warning, Note, Help };

struct SpanLabel {
    Span span;
    string message;

    SpanLabel() = default;
    SpanLabel(Span span, string message)
        : span(span), message(std::move(message)) {}
};

struct Diagnostic {
    Severity severity = Severity::Error;
    string headline;
    Span primary;
    string primaryLabel; // inline label after the caret line; empty = none
    vector<SpanLabel> labels;
    vector<Diagnostic> notes;
    string detail; // free-form block printed after the snippet, verbatim
    string suggestion;

    Diagnostic() = default;
    Diagnostic(Severity severity, string headline, Span primary)
        : severity(severity), headline(std::move(headline)), primary(primary) {}
};

class Renderer {
  public:
    Renderer();

    void render(Diagnostic const &diag, llvm::raw_ostream &out);

  private:
    bool useColor;

    void renderOne(Diagnostic const &diag, llvm::raw_ostream &out);
    void renderHeadline(Severity severity, llvm::StringRef msg,
                        llvm::raw_ostream &out);
    void renderLocationLine(Span primary, llvm::raw_ostream &out);
    void renderSnippet(Span primary, llvm::StringRef inlineLabel,
                       Severity severity, llvm::raw_ostream &out);
    string bold(llvm::StringRef s) const;
    string colorSeverity(Severity severity, llvm::StringRef s) const;
    string colorCaret(Severity severity, llvm::StringRef s) const;
};

unsigned visualColumn(SourcePtr const &source, unsigned offset);
void lineBoundsAt(SourcePtr const &source, unsigned offset, unsigned &lineStart,
                  unsigned &lineEnd, unsigned &lineNumber);

void displayDiagnostic(Diagnostic const &diag);

llvm::StringRef severityWord(Severity severity);

} // namespace ceramic
