#pragma once
// The saved library (index JSON): generations, references, skills, stories and preferences.

#include <filesystem>
#include <string>
#include <vector>

#include "framecraft/models.h"
#include "framecraft/prompts.h"
#include "framecraft/stories.h"

namespace fc {

struct Preferences {
    MediaKind mode = MediaKind::video;
    std::string imageModel = "gpt-image-2";
    std::string imageResolution = "1K";
    std::string imageAspect = "9:16";
    int quantity = 1;
    std::string camera = "N/A";
    std::string film = "N/A";
    std::string videoResolution = "720p";
    std::string videoAspect = "9:16";
    int videoDuration = 10;
    bool videoAudio = true;
    PromptProvider provider = PromptProvider::kie;
    std::string promptModel = "gpt-5-6-terra";
    std::string dialogueLanguage = "Español";
    bool notifyWhenDone = true;
    bool onboardingDone = false;
    /// "system", "light" or "dark".
    std::string theme = "system";
    bool templatesExpanded = true;
};

struct LibraryIndex {
    int version = 1;
    std::vector<Job> jobs;
    std::vector<ReferenceFile> references;
    std::vector<Skill> skills;
    std::vector<Story> stories;
    Preferences preferences;
};

void to_json(json& j, const Preferences& value);
void from_json(const json& j, Preferences& value);
void to_json(json& j, const LibraryIndex& value);
void from_json(const json& j, LibraryIndex& value);

namespace library {
/// Reads the index; returns an empty library when the file is missing or unreadable
/// (a damaged file is kept aside as .bak so nothing is lost).
LibraryIndex load(const std::filesystem::path& file);
/// Writes atomically (temporary file + rename).
bool save(const std::filesystem::path& file, const LibraryIndex& index);
}  // namespace library

}  // namespace fc
