#include "framecraft/models.h"

namespace fc {

const char* toString(MediaKind kind) { return kind == MediaKind::video ? "video" : "image"; }
MediaKind mediaKindFrom(const std::string& text) { return text == "video" ? MediaKind::video : MediaKind::image; }

const char* toString(JobStatus status) {
    switch (status) {
    case JobStatus::submitting: return "submitting";
    case JobStatus::queued: return "queued";
    case JobStatus::generating: return "generating";
    case JobStatus::saving: return "saving";
    case JobStatus::success: return "success";
    case JobStatus::fail: return "fail";
    case JobStatus::unknown: return "unknown";
    }
    return "unknown";
}

JobStatus jobStatusFrom(const std::string& text) {
    if (text == "submitting") return JobStatus::submitting;
    if (text == "queued") return JobStatus::queued;
    if (text == "generating") return JobStatus::generating;
    if (text == "saving") return JobStatus::saving;
    if (text == "success") return JobStatus::success;
    if (text == "fail") return JobStatus::fail;
    return JobStatus::unknown;
}

bool isActive(JobStatus status) {
    return status == JobStatus::submitting || status == JobStatus::queued || status == JobStatus::generating ||
           status == JobStatus::saving;
}

const char* toString(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::image: return "image";
    case ReferenceKind::video: return "video";
    case ReferenceKind::audio: return "audio";
    }
    return "image";
}

ReferenceKind referenceKindFrom(const std::string& text) {
    if (text == "video") return ReferenceKind::video;
    if (text == "audio") return ReferenceKind::audio;
    return ReferenceKind::image;
}

const char* tagPrefix(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::image: return "@Image";
    case ReferenceKind::video: return "@Video";
    case ReferenceKind::audio: return "@Audio";
    }
    return "@Image";
}

int64_t maxBytes(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::image: return 30LL * 1024 * 1024;
    case ReferenceKind::video: return 200LL * 1024 * 1024;
    case ReferenceKind::audio: return 15LL * 1024 * 1024;
    }
    return 0;
}

const char* formatsDescription(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::image: return "PNG, JPG o WebP";
    case ReferenceKind::video: return "MP4, MOV o MKV";
    case ReferenceKind::audio: return "MP3, WAV, AAC, M4A u OGG";
    }
    return "";
}

const char* pluralNoun(ReferenceKind kind) {
    switch (kind) {
    case ReferenceKind::image: return "imágenes";
    case ReferenceKind::video: return "videos";
    case ReferenceKind::audio: return "audios";
    }
    return "";
}

const char* toString(SkillMedia media) {
    switch (media) {
    case SkillMedia::image: return "image";
    case SkillMedia::video: return "video";
    case SkillMedia::any: return "any";
    }
    return "any";
}

SkillMedia skillMediaFrom(const std::string& text) {
    if (text == "image") return SkillMedia::image;
    if (text == "video") return SkillMedia::video;
    return SkillMedia::any;
}

bool supports(SkillMedia media, MediaKind kind) {
    return media == SkillMedia::any || (media == SkillMedia::image) == (kind == MediaKind::image);
}

namespace {
template <typename T>
void putOptional(json& j, const char* key, const std::optional<T>& value) {
    if (value) j[key] = *value;
}

template <typename T>
std::optional<T> getOptional(const json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    try {
        return it->get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> getStrings(const json& j, const char* key) {
    std::vector<std::string> out;
    auto it = j.find(key);
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& item : *it) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

std::string getString(const json& j, const char* key, const std::string& fallback = "") {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
}

int64_t getInt(const json& j, const char* key, int64_t fallback = 0) {
    auto it = j.find(key);
    return it != j.end() && it->is_number() ? it->get<int64_t>() : fallback;
}
}  // namespace

void to_json(json& j, const JobSettings& v) {
    j = json{{"resolution", v.resolution},
             {"aspect", v.aspect},
             {"imageReferences", v.imageReferences},
             {"videoReferences", v.videoReferences},
             {"audioReferences", v.audioReferences}};
    putOptional(j, "camera", v.camera);
    putOptional(j, "film", v.film);
    putOptional(j, "duration", v.duration);
    putOptional(j, "generateAudio", v.generateAudio);
}

void from_json(const json& j, JobSettings& v) {
    v.resolution = getString(j, "resolution");
    v.aspect = getString(j, "aspect");
    v.camera = getOptional<std::string>(j, "camera");
    v.film = getOptional<std::string>(j, "film");
    v.duration = getOptional<int>(j, "duration");
    v.generateAudio = getOptional<bool>(j, "generateAudio");
    v.imageReferences = getStrings(j, "imageReferences");
    v.videoReferences = getStrings(j, "videoReferences");
    v.audioReferences = getStrings(j, "audioReferences");
}

void to_json(json& j, const Job& v) {
    j = json{{"id", v.id},         {"batchID", v.batchID},   {"kind", toString(v.kind)},
             {"model", v.model},   {"prompt", v.prompt},     {"finalPrompt", v.finalPrompt},
             {"settings", v.settings}, {"status", toString(v.status)}, {"progress", v.progress},
             {"outputs", v.outputs}, {"created", v.created}, {"updated", v.updated},
             {"favorite", v.favorite}};
    putOptional(j, "taskId", v.taskId);
    putOptional(j, "error", v.error);
}

void from_json(const json& j, Job& v) {
    v.id = getString(j, "id");
    v.batchID = getString(j, "batchID");
    v.kind = mediaKindFrom(getString(j, "kind"));
    v.model = getString(j, "model");
    v.prompt = getString(j, "prompt");
    v.finalPrompt = getString(j, "finalPrompt");
    if (j.contains("settings")) v.settings = j.at("settings").get<JobSettings>();
    v.status = jobStatusFrom(getString(j, "status"));
    v.progress = static_cast<int>(getInt(j, "progress"));
    v.taskId = getOptional<std::string>(j, "taskId");
    v.outputs = getStrings(j, "outputs");
    v.error = getOptional<std::string>(j, "error");
    v.created = getInt(j, "created");
    v.updated = getInt(j, "updated", v.created);
    v.favorite = getOptional<bool>(j, "favorite").value_or(false);
}

void to_json(json& j, const ReferenceFile& v) {
    j = json{{"id", v.id},     {"name", v.name},   {"kind", toString(v.kind)}, {"mime", v.mime},
             {"fileName", v.fileName}, {"bytes", v.bytes}, {"created", v.created}};
    putOptional(j, "durationMs", v.durationMs);
}

void from_json(const json& j, ReferenceFile& v) {
    v.id = getString(j, "id");
    v.name = getString(j, "name");
    v.kind = referenceKindFrom(getString(j, "kind"));
    v.mime = getString(j, "mime");
    v.fileName = getString(j, "fileName");
    v.durationMs = getOptional<int>(j, "durationMs");
    v.bytes = getInt(j, "bytes");
    v.created = getInt(j, "created");
}

void to_json(json& j, const Skill& v) {
    j = json{{"id", v.id},           {"name", v.name},           {"summary", v.summary}, {"content", v.content},
             {"media", toString(v.media)}, {"isBuiltin", v.isBuiltin}, {"created", v.created}};
}

void from_json(const json& j, Skill& v) {
    v.id = getString(j, "id");
    v.name = getString(j, "name");
    v.summary = getString(j, "summary");
    v.content = getString(j, "content");
    v.media = skillMediaFrom(getString(j, "media", "any"));
    v.isBuiltin = getOptional<bool>(j, "isBuiltin").value_or(false);
    v.created = getInt(j, "created");
}

}  // namespace fc
