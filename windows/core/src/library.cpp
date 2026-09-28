#include "framecraft/library.h"

#include <fstream>
#include <sstream>

#include "framecraft/util.h"

namespace fc {

namespace fs = std::filesystem;

void to_json(json& j, const Preferences& v) {
    j = {{"mode", toString(v.mode)},
         {"imageModel", v.imageModel},
         {"imageResolution", v.imageResolution},
         {"imageAspect", v.imageAspect},
         {"quantity", v.quantity},
         {"camera", v.camera},
         {"film", v.film},
         {"videoResolution", v.videoResolution},
         {"videoAspect", v.videoAspect},
         {"videoDuration", v.videoDuration},
         {"videoAudio", v.videoAudio},
         {"provider", toString(v.provider)},
         {"promptModel", v.promptModel},
         {"dialogueLanguage", v.dialogueLanguage},
         {"notifyWhenDone", v.notifyWhenDone},
         {"onboardingDone", v.onboardingDone},
         {"theme", v.theme},
         {"templatesExpanded", v.templatesExpanded}};
}

void from_json(const json& j, Preferences& v) {
    Preferences d;
    auto s = [&](const char* key, const std::string& fallback) {
        auto it = j.find(key);
        return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
    };
    auto i = [&](const char* key, int fallback) {
        auto it = j.find(key);
        return it != j.end() && it->is_number() ? it->get<int>() : fallback;
    };
    auto b = [&](const char* key, bool fallback) {
        auto it = j.find(key);
        return it != j.end() && it->is_boolean() ? it->get<bool>() : fallback;
    };
    v.mode = mediaKindFrom(s("mode", toString(d.mode)));
    v.imageModel = s("imageModel", d.imageModel);
    v.imageResolution = s("imageResolution", d.imageResolution);
    v.imageAspect = s("imageAspect", d.imageAspect);
    v.quantity = i("quantity", d.quantity);
    v.camera = s("camera", d.camera);
    v.film = s("film", d.film);
    v.videoResolution = s("videoResolution", d.videoResolution);
    v.videoAspect = s("videoAspect", d.videoAspect);
    v.videoDuration = i("videoDuration", d.videoDuration);
    v.videoAudio = b("videoAudio", d.videoAudio);
    v.provider = promptProviderFrom(s("provider", "kie"));
    v.promptModel = s("promptModel", d.promptModel);
    v.dialogueLanguage = s("dialogueLanguage", d.dialogueLanguage);
    v.notifyWhenDone = b("notifyWhenDone", d.notifyWhenDone);
    v.onboardingDone = b("onboardingDone", d.onboardingDone);
    v.theme = s("theme", d.theme);
    v.templatesExpanded = b("templatesExpanded", d.templatesExpanded);
}

void to_json(json& j, const LibraryIndex& v) {
    j = {{"version", v.version}, {"jobs", v.jobs}, {"references", v.references}, {"skills", v.skills},
         {"stories", v.stories}, {"preferences", v.preferences}};
}

void from_json(const json& j, LibraryIndex& v) {
    auto list = [&](const char* key, auto& target) {
        auto it = j.find(key);
        if (it == j.end() || !it->is_array()) return;
        using Item = typename std::decay_t<decltype(target)>::value_type;
        for (const auto& item : *it) {
            try {
                target.push_back(item.get<Item>());
            } catch (...) {
                // Skip a damaged entry instead of losing the whole library.
            }
        }
    };
    v.version = j.value("version", 1);
    list("jobs", v.jobs);
    list("references", v.references);
    list("skills", v.skills);
    list("stories", v.stories);
    if (auto it = j.find("preferences"); it != j.end() && it->is_object()) v.preferences = it->get<Preferences>();
}

namespace library {

LibraryIndex load(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    stream.close();
    json value = json::parse(buffer.str(), nullptr, false);
    if (value.is_discarded() || !value.is_object()) {
        std::error_code error;
        fs::copy_file(file, fs::path(file).concat(".bak"), fs::copy_options::overwrite_existing, error);
        return {};
    }
    return value.get<LibraryIndex>();
}

bool save(const fs::path& file, const LibraryIndex& index) {
    std::error_code error;
    fs::create_directories(file.parent_path(), error);
    fs::path temporary = fs::path(file).concat(".tmp");
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << json(index).dump(1);
        if (!stream) return false;
    }
    fs::rename(temporary, file, error);
    return !error;
}

}  // namespace library
}  // namespace fc
