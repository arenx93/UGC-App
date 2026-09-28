#pragma once
// "Historias": a brief turned into a pack of scene prompts. Port of Stories.swift.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "framecraft/models.h"

namespace fc {

/// One reference position in a story. Its tag (@Image1, @Audio2…) is its order among the
/// slots of the same kind and stays fixed for every scene.
struct StoryReferenceSlot {
    std::string id;
    ReferenceKind kind = ReferenceKind::image;
    std::optional<std::string> referenceID;  // nil for the "last frame" slot
    bool isLastFrame = false;
    std::string note;
};

struct StoryScene {
    std::string id;
    int number = 1;
    std::string title;
    std::string summary;
    int duration = 20;
    std::string prompt;
    std::optional<std::string> notes;
    std::vector<std::string> jobIDs;  // newest last
};

struct StoryMessage {
    std::string id;
    bool fromUser = false;
    std::string text;
    std::optional<int> sceneNumber;
    int64_t created = 0;
};

struct Story {
    std::string id;
    std::string title = "Nueva historia";
    std::string brief;
    std::string skillID = "builtin-ugc-celular";
    std::string dialogueLanguage = "Español";
    int sceneDuration = 20;
    std::optional<int> sceneCount;  // nullopt = the assistant decides
    std::string aspect = "9:16";
    std::string resolution = "720p";
    bool generateAudio = true;
    std::vector<StoryReferenceSlot> slots;
    std::string summary;
    std::string continuity;
    std::string referenceOrder;
    std::vector<StoryScene> scenes;
    std::vector<StoryMessage> messages;
    std::optional<std::string> finalCutJobID;
    int64_t created = 0;
    int64_t updated = 0;

    int totalDuration() const;
    /// "@Image2" for a slot, following the order of slots of the same kind.
    std::string tag(const StoryReferenceSlot& slot) const;
    std::vector<StoryReferenceSlot> slotsOf(ReferenceKind kind) const;
    bool usesLastFrame() const;
};

struct StoryDraft {
    struct Scene {
        std::string title;
        std::string summary;
        int duration = 20;
        std::string prompt;
        std::optional<std::string> notes;
    };
    std::string title;
    std::string summary;
    std::string continuity;
    std::string referenceOrder;
    std::vector<Scene> scenes;
};

namespace stories {

std::vector<std::string> referenceLines(const Story& story, const std::map<std::string, std::string>& names);
std::vector<std::string> displayLines(const Story& story, const std::map<std::string, std::string>& names);
std::string storyInstructions();
std::string sceneInstructions();
std::string storyBrief(const Story& story, const std::vector<std::string>& referenceLines, const std::optional<json>& skill,
                       bool previous, const std::optional<std::string>& feedback);
std::string sceneBrief(const Story& story, const StoryScene& scene, const std::vector<std::string>& referenceLines,
                       const std::optional<json>& skill, const std::string& feedback);
StoryDraft parseStory(const std::string& raw, int defaultDuration);
StoryDraft::Scene parseSingleScene(const std::string& raw, int defaultDuration);
/// The whole pack as text, in the spirit of the Walter pack.
std::string exportText(const Story& story, const std::vector<std::string>& referenceLines);

}  // namespace stories

void to_json(json& j, const StoryReferenceSlot& value);
void from_json(const json& j, StoryReferenceSlot& value);
void to_json(json& j, const StoryScene& value);
void from_json(const json& j, StoryScene& value);
void to_json(json& j, const StoryMessage& value);
void from_json(const json& j, StoryMessage& value);
void to_json(json& j, const Story& value);
void from_json(const json& j, Story& value);

}  // namespace fc
