#include "diagnostic.hpp"

#include "llvm/Support/raw_ostream.h"

#include <unistd.h>

namespace ceramic {

static constexpr unsigned TAB_WIDTH = 8;
static const char *SNIPPET_INDENT = "    ";
static const char *LOCATION_INDENT = "  ";

static const char *ANSI_RESET = "\033[0m";
static const char *ANSI_BOLD_RED = "\033[1;31m";
static const char *ANSI_BOLD_YELLOW = "\033[1;33m";
static const char *ANSI_BOLD_CYAN = "\033[1;36m";
static const char *ANSI_BOLD_GREEN = "\033[1;32m";

static bool isUtf8Continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

void lineBoundsAt(SourcePtr const &source, unsigned offset, unsigned &lineStart,
                  unsigned &lineEnd, unsigned &lineNumber) {
    lineStart = 0;
    lineNumber = 1;
    const char *data = source->data();
    const unsigned size = static_cast<unsigned>(source->size());
    const unsigned clamped = offset > size ? size : offset;
    for (unsigned i = 0; i < clamped; ++i) {
        if (data[i] == '\n') {
            lineStart = i + 1;
            ++lineNumber;
        }
    }
    lineEnd = lineStart;
    while (lineEnd < size && data[lineEnd] != '\n')
        ++lineEnd;
}

unsigned visualColumn(SourcePtr const &source, unsigned offset) {
    unsigned lineStart, lineEnd, lineNumber;
    lineBoundsAt(source, offset, lineStart, lineEnd, lineNumber);
    const char *data = source->data();
    unsigned col = 0;
    for (unsigned i = lineStart; i < offset; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '\t')
            col += TAB_WIDTH - (col % TAB_WIDTH);
        else if (!isUtf8Continuation(c))
            ++col;
    }
    return col;
}

llvm::StringRef severityWord(Severity severity) {
    switch (severity) {
    case Severity::Error:
        return "error";
    case Severity::Warning:
        return "warning";
    case Severity::Note:
        return "note";
    case Severity::Help:
        return "help";
    }
    return "error";
}

static const char *severityAnsi(Severity severity) {
    switch (severity) {
    case Severity::Error:
        return ANSI_BOLD_RED;
    case Severity::Warning:
        return ANSI_BOLD_YELLOW;
    case Severity::Note:
        return ANSI_BOLD_CYAN;
    case Severity::Help:
        return ANSI_BOLD_GREEN;
    }
    return ANSI_BOLD_RED;
}

Renderer::Renderer() : useColor(isatty(fileno(stderr)) != 0) {}

string Renderer::bold(llvm::StringRef s) const {
    if (!useColor)
        return s.str();
    string buf;
    buf.reserve(s.size() + 8);
    buf += "\033[1m";
    buf += s;
    buf += ANSI_RESET;
    return buf;
}

string Renderer::colorSeverity(Severity severity, llvm::StringRef s) const {
    if (!useColor)
        return s.str();
    string buf;
    buf.reserve(s.size() + 16);
    buf += severityAnsi(severity);
    buf += s;
    buf += ANSI_RESET;
    return buf;
}

string Renderer::colorCaret(Severity severity, llvm::StringRef s) const {
    return colorSeverity(severity, s);
}

void Renderer::renderHeadline(Severity severity, llvm::StringRef msg,
                              llvm::raw_ostream &out) {
    out << colorSeverity(severity, severityWord(severity)) << ": " << msg
        << "\n";
}

void Renderer::renderLocationLine(Span primary, llvm::raw_ostream &out) {
    if (!primary.ok())
        return;
    unsigned lineStart, lineEnd, lineNumber;
    lineBoundsAt(primary.source, primary.startOffset, lineStart, lineEnd,
                 lineNumber);
    unsigned column = visualColumn(primary.source, primary.startOffset) + 1;
    out << LOCATION_INDENT << "at " << primary.source->fileName << ":"
        << lineNumber << ":" << column << "\n";
}

void Renderer::renderSnippet(Span primary, llvm::StringRef inlineLabel,
                             Severity severity, llvm::raw_ostream &out) {
    if (!primary.ok())
        return;

    unsigned lineStart, lineEnd, lineNumber;
    lineBoundsAt(primary.source, primary.startOffset, lineStart, lineEnd,
                 lineNumber);
    const char *data = primary.source->data();

    out << SNIPPET_INDENT;
    for (unsigned i = lineStart; i < lineEnd; ++i)
        out << data[i];
    out << "\n";

    // build the caret line as plain text first, then color the marker run.
    out << SNIPPET_INDENT;
    unsigned spanEnd = primary.endOffset;
    if (spanEnd > lineEnd)
        spanEnd = lineEnd;
    if (spanEnd < primary.startOffset)
        spanEnd = primary.startOffset;

    unsigned col = 0;
    for (unsigned i = lineStart; i < primary.startOffset; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '\t') {
            unsigned advance = TAB_WIDTH - (col % TAB_WIDTH);
            for (unsigned k = 0; k < advance; ++k)
                out << ' ';
            col += advance;
        } else if (!isUtf8Continuation(c)) {
            out << ' ';
            ++col;
        }
    }

    string marker;
    if (primary.startOffset == spanEnd) {
        marker = "^";
    } else {
        bool first = true;
        for (unsigned i = primary.startOffset; i < spanEnd; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (isUtf8Continuation(c))
                continue;
            if (c == '\t') {
                unsigned advance = TAB_WIDTH - (col % TAB_WIDTH);
                for (unsigned k = 0; k < advance; ++k) {
                    marker += (first ? '^' : '~');
                    first = false;
                }
                col += advance;
            } else {
                marker += (first ? '^' : '~');
                first = false;
                ++col;
            }
        }
        if (marker.empty())
            marker = "^";
    }

    out << colorCaret(severity, marker);
    if (!inlineLabel.empty())
        out << " " << colorCaret(severity, inlineLabel);
    out << "\n";
}

void Renderer::renderOne(Diagnostic const &diag, llvm::raw_ostream &out) {
    renderHeadline(diag.severity, diag.headline, out);
    renderLocationLine(diag.primary, out);
    renderSnippet(diag.primary, diag.primaryLabel, diag.severity, out);

    for (auto const &lbl : diag.labels) {
        // secondary labels render like a sub-note: location + snippet, with
        // the label text inline on the caret line.
        renderLocationLine(lbl.span, out);
        renderSnippet(lbl.span, lbl.message, Severity::Note, out);
    }
}

void Renderer::render(Diagnostic const &diag, llvm::raw_ostream &out) {
    renderOne(diag, out);
    for (size_t i = 0; i < diag.notes.size(); ++i)
        renderOne(diag.notes[i], out);
    if (!diag.suggestion.empty()) {
        out << colorSeverity(Severity::Help, severityWord(Severity::Help))
            << ": " << diag.suggestion << "\n";
    }
    out.flush();
}

void displayDiagnostic(Diagnostic const &diag) {
    static Renderer renderer;
    renderer.render(diag, llvm::errs());
}

} // namespace ceramic
