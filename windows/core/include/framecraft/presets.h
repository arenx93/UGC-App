#pragma once
// Port of lib/presets.ts (web app) and Presets.swift (macOS). Keep them in sync.

#include <optional>
#include <string>
#include <vector>

namespace fc {

struct ImageModel {
    std::string id;
    std::string name;
    std::string note;
    std::string glyph;  // Segoe Fluent Icons code point
};

/// A camera or framing preset. `id` is the stable key; `text` is the English instruction added to the prompt.
struct StylePreset {
    std::string id;
    std::string title;
    std::string glyph;
    std::string summary;
    std::string text;
};

namespace presets {

inline const std::string kNone = "N/A";
inline const std::string kVideoModelID = "bytedance/seedance-2-5";
inline const std::string kVideoModelName = "Seedance 2.5";
/// Model id for the final cut Framecraft assembles from a story's scenes (made locally, no credits).
inline const std::string kStoryCutModelID = "framecraft-story";
constexpr int kMaxVideoPromptLength = 30000;
constexpr int kMinVideoDuration = 4;
constexpr int kMaxVideoDuration = 30;

const std::vector<ImageModel>& imageModels();
const std::vector<StylePreset>& cameras();
const std::vector<StylePreset>& films();
const std::vector<std::string>& imageResolutions();
const std::vector<std::string>& videoResolutions();
const std::vector<std::string>& videoAspects();
const std::vector<int>& videoDurationShortcuts();

std::string cameraText(const std::string& id);
std::string filmText(const std::string& id);
bool isValidCamera(const std::string& id);
bool isValidFilm(const std::string& id);
const ImageModel* imageModel(const std::string& id);
std::string modelName(const std::string& id);
int clampDuration(int seconds);

/// Longest prompt the user may type for an image model.
int maxPromptLength(const std::string& imageModel);
/// Longest final prompt (with presets) accepted by KIE for an image model.
int maxComposedLength(const std::string& imageModel);

/// Aspect ratios KIE accepts for a model and resolution.
std::vector<std::string> ratios(const std::string& model, const std::string& resolution);

/// Final prompt sent to an image model: the user's prompt plus the selected camera/film presets.
/// JSON visual profiles get the presets merged in.
std::string composePrompt(const std::string& prompt, const std::string& camera, const std::string& film,
                          const std::optional<std::string>& aspect = std::nullopt);

}  // namespace presets

/// The dialogue-density rule from the Seedance kit: about 2.47 spoken words per second.
namespace dialogue {
constexpr double kWordsPerSecond = 2.47;
int targetWords(int seconds);
double secondsForWords(int words);
}  // namespace dialogue

}  // namespace fc
