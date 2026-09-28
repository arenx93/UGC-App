#pragma once
// Ready-made starting points for images, videos and stories. Port of Templates.swift.

#include <optional>
#include <string>
#include <vector>

#include "framecraft/models.h"

namespace fc {

struct CreativeTemplate {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string glyph;  // Segoe Fluent Icons
    MediaKind media;
    /// Idea text for the assistant. Words in [brackets] are placeholders for the user.
    std::string idea;
    std::optional<int> duration;
    std::string aspect = "9:16";

    std::vector<std::string> placeholders() const;
};

struct StoryTemplate {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string glyph;
    std::string brief;
    std::optional<int> sceneCount;
    int sceneDuration = 15;
};

namespace templates {
const std::vector<CreativeTemplate>& creative();
const std::vector<StoryTemplate>& stories();
/// "[tu producto]"-style placeholders inside a text.
std::vector<std::string> placeholders(const std::string& text);
}  // namespace templates

}  // namespace fc
