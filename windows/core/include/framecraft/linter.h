#pragma once
// Checks a video prompt against the Seedance kit checklist. Port of PromptLinter.swift.

#include <string>
#include <vector>

namespace fc {

struct LintItem {
    enum class State { ok, warning, problem, info };
    std::string id;
    State state;
    std::string title;
    std::string detail;
};

struct LintReport {
    std::vector<LintItem> items;
    int dialogueWords = 0;
    int targetWords = 0;
    int promptWords = 0;

    int count(LintItem::State state) const;
    int problems() const { return count(LintItem::State::problem); }
    int warnings() const { return count(LintItem::State::warning); }
    int passed() const { return count(LintItem::State::ok); }
    const LintItem* item(const std::string& id) const;
};

struct ReferenceCounts {
    int images = 0;
    int videos = 0;
    int audios = 0;
};

namespace linter {

const std::vector<std::string>& adWords();

/// `ugcStyle` applies the phone-UGC rules (timestamps, ad words, wide depth of field).
LintReport lintVideo(const std::string& prompt, int duration, ReferenceCounts references, bool ugcStyle);

/// Words spoken inside "quotes" / “quotes”.
int dialogueWords(const std::string& text);
/// Highest N used in tags like @Image3 (0 when absent), case-insensitive.
int highestTag(const std::string& prefix, const std::string& text);
bool hasTimestamps(const std::string& text);
/// True when `term` appears as a whole word/phrase not preceded by a negation in the same clause.
bool containsUnnegated(const std::string& lowered, const std::string& term);
/// "(direction): \"line\"" — acting direction right before a spoken line.
bool hasDirectionBeforeLine(const std::string& text);

}  // namespace linter
}  // namespace fc
