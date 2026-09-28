#pragma once
// Built-in and user skills for the prompt assistant. Port of SkillLibrary.swift.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "framecraft/models.h"
#include "framecraft/prompts.h"

namespace fc {

/// Files shipped with the app (Skills/ folder next to the executable).
class SkillResources {
public:
    explicit SkillResources(std::filesystem::path root);
    /// Looks next to the executable and in the repository (development builds, tests).
    static std::optional<SkillResources> locate(const std::filesystem::path& executableDir);

    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path path(const std::string& relative) const;
    std::string text(const std::string& relative) const;
    /// The JSON visual-profile instructions (stored as a JSON string).
    const std::string& jsonProfile() const;
    const std::vector<PromptExample>& examples() const;
    std::filesystem::path guidePDF() const { return path("guia/documento-base-ugc-seedance.pdf"); }

private:
    std::filesystem::path root_;
    mutable std::optional<std::string> jsonProfile_;
    mutable std::optional<std::vector<PromptExample>> examples_;
};

struct ParsedSkill {
    std::string name;
    std::string summary;
    std::string body;
};

namespace skills {

inline const std::string kJsonProfileID = "builtin-json-image";
inline const std::string kUgcID = "builtin-ugc-celular";
inline const std::string kArthasID = "builtin-arthas-cachito";
inline const std::string kGeneralID = "";
inline const std::string kExampleAttribution =
    "Examples adapted from YouMind OpenLab, Awesome GPT Image 2, CC BY 4.0. Original authors and sources accompany selected examples.";

std::vector<Skill> builtins(const SkillResources* resources);
/// The full worked example (8 scenes) — extra context for the UGC skill.
std::string walterExample(const SkillResources* resources);
/// Parses a SKILL.md-style file: optional YAML front matter (name, description) and the markdown body.
ParsedSkill parse(const std::string& markdown, const std::string& fallbackName);
/// Guesses whether an imported skill is meant for images or videos.
SkillMedia guessMedia(const std::string& name, const std::string& summary, const std::string& body);
/// The three examples whose title/prompt best match the words of the idea (port of lib/builtin-skill.ts).
std::vector<PromptExample> relevantExamples(const std::string& idea, const std::vector<PromptExample>& examples);
/// {"profile", "examples", ...} context of the JSON-profile skill.
json jsonProfileContext(const std::string& idea, const SkillResources& resources, std::vector<PromptExample>* chosen = nullptr);

}  // namespace skills
}  // namespace fc
