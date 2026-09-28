#pragma once
// Library data: generations (jobs), reference files and skills.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace fc {

using json = nlohmann::json;

enum class MediaKind { image, video };
const char* toString(MediaKind kind);
MediaKind mediaKindFrom(const std::string& text);

enum class JobStatus { submitting, queued, generating, saving, success, fail, unknown };
const char* toString(JobStatus status);
JobStatus jobStatusFrom(const std::string& text);
bool isActive(JobStatus status);
inline bool isFinished(JobStatus status) { return !isActive(status); }

/// Settings a generation was created with (used by "Reusar ajustes").
struct JobSettings {
    std::string resolution;
    std::string aspect;
    std::optional<std::string> camera;
    std::optional<std::string> film;
    std::optional<int> duration;
    std::optional<bool> generateAudio;
    std::vector<std::string> imageReferences;
    std::vector<std::string> videoReferences;
    std::vector<std::string> audioReferences;
};

/// One generation. Image batches create one job per image (shared batchID).
struct Job {
    std::string id;
    std::string batchID;
    MediaKind kind = MediaKind::image;
    std::string model;
    std::string prompt;
    std::string finalPrompt;
    JobSettings settings;
    JobStatus status = JobStatus::submitting;
    int progress = 0;
    std::optional<std::string> taskId;
    /// File names inside the library's generations folder.
    std::vector<std::string> outputs;
    std::optional<std::string> error;
    int64_t created = 0;  // ms since 1970
    int64_t updated = 0;
    bool favorite = false;
};

enum class ReferenceKind { image, video, audio };
const char* toString(ReferenceKind kind);
ReferenceKind referenceKindFrom(const std::string& text);
/// Tag used inside Seedance prompts: @Image1, @Video1, @Audio1…
const char* tagPrefix(ReferenceKind kind);
int64_t maxBytes(ReferenceKind kind);
const char* formatsDescription(ReferenceKind kind);
/// "imágenes", "videos", "audios".
const char* pluralNoun(ReferenceKind kind);
inline const ReferenceKind kAllReferenceKinds[] = {ReferenceKind::image, ReferenceKind::video, ReferenceKind::audio};

/// A reference file the user imported (stored in the library folder).
struct ReferenceFile {
    std::string id;
    std::string name;
    ReferenceKind kind = ReferenceKind::image;
    std::string mime;
    std::string fileName;
    std::optional<int> durationMs;
    int64_t bytes = 0;
    int64_t created = 0;

    double durationSeconds() const { return durationMs.value_or(0) / 1000.0; }
};

enum class SkillMedia { image, video, any };
const char* toString(SkillMedia media);
SkillMedia skillMediaFrom(const std::string& text);
bool supports(SkillMedia media, MediaKind kind);

/// A creative skill: text instructions that guide the prompt assistant.
struct Skill {
    std::string id;
    std::string name;
    std::string summary;
    std::string content;
    SkillMedia media = SkillMedia::any;
    bool isBuiltin = false;
    int64_t created = 0;
};

void to_json(json& j, const JobSettings& value);
void from_json(const json& j, JobSettings& value);
void to_json(json& j, const Job& value);
void from_json(const json& j, Job& value);
void to_json(json& j, const ReferenceFile& value);
void from_json(const json& j, ReferenceFile& value);
void to_json(json& j, const Skill& value);
void from_json(const json& j, Skill& value);

}  // namespace fc
