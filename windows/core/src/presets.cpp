#include "framecraft/presets.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "framecraft/models.h"
#include "framecraft/util.h"

namespace fc {
namespace presets {

const std::vector<ImageModel>& imageModels() {
    static const std::vector<ImageModel> models = {
        {"gpt-image-2", "GPT Image 2", "Versátil. Ideal para empezar.", "\xEE\x9E\xB4"},
        {"gpt-image-2-5-flare", "GPT Image 2.5 · Flare", "Más creativo y expresivo.", "\xEE\xA7\xA9"},
        {"gpt-image-2-5-sunburst", "GPT Image 2.5 · Sunburst", "Máximo detalle y nitidez.", "\xEE\x9C\x86"},
        {"nano-banana-pro", "Nano Banana Pro", "El que mejor respeta tus fotos de referencia.", "\xEE\xA4\x9E"},
    };
    return models;
}

const std::vector<StylePreset>& cameras() {
    static const std::vector<StylePreset> list = {
        {kNone, "Sin preset", "\xEE\x9C\x91", "Solo tu prompt.", ""},
        {"Android Camera", "Celular Android", "\xEE\xA2\xAA", "Foto casera: algo de ruido, foco suave, exposición imperfecta.",
         "Low-quality Android phone snapshot: slightly soft focus, subtle motion blur, visible digital noise, compressed detail and imperfect exposure. Natural, unpolished everyday photography."},
        {"iPhone Camera", "iPhone", "\xEE\xA2\xAA", "Foto espontánea de iPhone con piel real y luz disponible.",
         "Candid iPhone snapshot with natural smartphone processing, realistic skin texture, available light and spontaneous everyday framing. Avoid a polished studio look."},
        {"TV / News Camera", "Noticiero", "\xEE\x9E\xAA", "Cuadro de cámara de noticias local, documental.",
         "Local television news camera footage captured as a still: documentary framing, on-location broadcast lighting, slightly compressed video detail and realistic local-news color. No station logo, lower third or text unless requested."},
    };
    return list;
}

const std::vector<StylePreset>& films() {
    static const std::vector<StylePreset> list = {
        {kNone, "Sin encuadre", "\xEE\x9C\x91", "El encuadre lo decide el prompt.", ""},
        {"POV", "POV", "\xEE\xA0\x8E", "En primera persona: se ve una sola mano.",
         "First-person POV through a handheld phone at 1× magnification. Exactly one hand is visible; the other hand is holding the phone outside the frame. Slight handheld motion blur, eye-level perspective and natural framing. No extra hands or fingers."},
        {"Selfie", "Selfie", "\xEE\x9D\xB2", "Cámara frontal a un brazo de distancia.",
         "Front-facing phone selfie, candid expression, imperfect slightly off-center framing, natural arm-length perspective and spontaneous composition. Unretouched everyday appearance."},
    };
    return list;
}

const std::vector<std::string>& imageResolutions() {
    static const std::vector<std::string> list = {"1K", "2K", "4K"};
    return list;
}

const std::vector<std::string>& videoResolutions() {
    static const std::vector<std::string> list = {"480p", "720p", "1080p"};
    return list;
}

const std::vector<std::string>& videoAspects() {
    static const std::vector<std::string> list = {"9:16", "16:9", "1:1", "4:3", "3:4", "21:9", "adaptive"};
    return list;
}

const std::vector<int>& videoDurationShortcuts() {
    static const std::vector<int> list = {5, 10, 15, 20, 25, 30};
    return list;
}

std::string cameraText(const std::string& id) {
    for (const auto& preset : cameras())
        if (preset.id == id) return preset.text;
    return "";
}

std::string filmText(const std::string& id) {
    for (const auto& preset : films())
        if (preset.id == id) return preset.text;
    return "";
}

bool isValidCamera(const std::string& id) {
    return std::any_of(cameras().begin(), cameras().end(), [&](const StylePreset& p) { return p.id == id; });
}

bool isValidFilm(const std::string& id) {
    return std::any_of(films().begin(), films().end(), [&](const StylePreset& p) { return p.id == id; });
}

const ImageModel* imageModel(const std::string& id) {
    for (const auto& model : imageModels())
        if (model.id == id) return &model;
    return nullptr;
}

std::string modelName(const std::string& id) {
    if (id == kStoryCutModelID) return "Historia montada";
    if (id == kVideoModelID) return kVideoModelName;
    if (const auto* model = imageModel(id)) return model->name;
    return id;
}

int clampDuration(int seconds) { return std::clamp(seconds, kMinVideoDuration, kMaxVideoDuration); }

int maxPromptLength(const std::string& model) { return model == "nano-banana-pro" ? 9000 : 18000; }
int maxComposedLength(const std::string& model) { return model == "nano-banana-pro" ? 10000 : 20000; }

std::vector<std::string> ratios(const std::string& model, const std::string& resolution) {
    if (startsWith(model, "gpt-image-2-5")) {
        std::vector<std::string> list = {"auto", "1:1", "3:2", "2:3", "4:3", "3:4", "16:9", "9:16", "21:9"};
        if (resolution == "1K") list.insert(list.end(), {"27:16", "16:27", "9:8", "8:9"});
        return list;
    }
    std::vector<std::string> all = model == "nano-banana-pro"
        ? std::vector<std::string>{"1:1", "2:3", "3:2", "3:4", "4:3", "4:5", "5:4", "9:16", "16:9", "21:9"}
        : std::vector<std::string>{"auto", "1:1", "3:2", "2:3", "4:3", "3:4", "5:4", "4:5", "16:9", "9:16", "2:1", "1:2", "3:1", "1:3", "21:9", "9:21"};
    std::set<std::string> blocked;
    if (resolution == "2K") blocked = {"5:4", "4:5", "3:1", "1:3", "9:21"};
    else if (resolution == "4K") blocked = {"3:1", "1:3", "9:21"};
    std::vector<std::string> out;
    for (const auto& r : all) {
        if (model == "gpt-image-2" && ((resolution != "1K" && r == "auto") || (resolution == "4K" && r == "1:1"))) continue;
        if (model != "nano-banana-pro" && blocked.count(r)) continue;
        out.push_back(r);
    }
    return out;
}

std::string composePrompt(const std::string& prompt, const std::string& camera, const std::string& film,
                          const std::optional<std::string>& aspect) {
    std::string text = trim(prompt);
    std::string cameraInstruction = cameraText(camera);
    std::string filmInstruction = filmText(film);
    if (!startsWith(text, "{")) {
        std::vector<std::string> parts;
        for (const auto& part : {text, cameraInstruction, filmInstruction})
            if (!part.empty()) parts.push_back(part);
        return join(parts, "\n\n");
    }
    json profile = json::parse(text, nullptr, false);
    if (profile.is_discarded() || !profile.is_object()) return text;

    auto object = [](json& parent, const char* key) -> json& {
        if (!parent.contains(key) || !parent[key].is_object()) parent[key] = json::object();
        return parent[key];
    };

    if (aspect && !aspect->empty() && *aspect != "auto") object(profile, "composition")["aspect_ratio"] = *aspect;
    if (cameraInstruction.empty() && filmInstruction.empty()) return profile.dump();

    json& specs = object(profile, "technical_specs");
    if (!cameraInstruction.empty()) {
        specs["camera_style"] = cameraInstruction;
        if (camera == "Android Camera") {
            specs["sharpness"] = "Slightly soft focus and subtle motion blur";
            specs["grain"] = "Visible digital noise";
            specs["texture"] = "Compressed detail, imperfect exposure";
        } else if (camera == "iPhone Camera") {
            specs["sharpness"] = "Natural smartphone detail, realistic skin texture";
            specs["grain"] = "Subtle natural phone noise";
            specs["texture"] = "Unretouched candid detail";
        } else if (camera == "TV / News Camera") {
            specs["sharpness"] = "Slightly compressed broadcast video detail";
            specs["texture"] = "Local-news documentary footage";
        }
    }
    if (!filmInstruction.empty()) {
        object(profile, "composition")["framing"] = filmInstruction;
        json& specsAgain = object(profile, "technical_specs");
        specsAgain["perspective"] = film == "POV" ? "First-person eye-level handheld phone, 1x magnification"
                                                  : "Front-facing phone selfie at arm’s length, imperfect off-center framing";
        if (film == "POV") {
            specsAgain["motion_blur"] = "Slight handheld motion blur";
            object(profile, "subject_analysis")["hands_and_gestures"] = json{
                {"left_hand", "Outside the frame, holding the phone"},
                {"right_hand", "The only visible hand; natural gesture consistent with the scene"},
                {"finger_positions", "Natural anatomy, no extra fingers"},
                {"interaction", "Exactly one visible hand; phone-holding hand stays out of frame"},
                {"visible_hand_count", 1},
            };
        }
    }
    json overrides = {{"priority",
                       "These selected camera and framing constraints override conflicting descriptions elsewhere in this profile, including generation_parameters.prompts."}};
    if (!cameraInstruction.empty()) overrides["camera"] = cameraInstruction;
    if (!filmInstruction.empty()) overrides["film"] = filmInstruction;
    profile["framecraft_overrides"] = overrides;
    return profile.dump();
}

}  // namespace presets

namespace dialogue {
int targetWords(int seconds) { return static_cast<int>(std::lround(seconds * kWordsPerSecond)); }
double secondsForWords(int words) { return words / kWordsPerSecond; }
}  // namespace dialogue

}  // namespace fc
