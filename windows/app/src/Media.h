#pragma once
// Media on Windows: durations, thumbnails, last frame of a video and joining clips (Windows.Media.Editing).

#include <filesystem>
#include <optional>
#include <vector>

namespace fcapp::media {

/// Duration in seconds of a video or audio file (blocking; call from a background thread).
std::optional<double> duration(std::filesystem::path const& file, bool video);

/// Saves the last frame of a video as PNG (blocking; background thread).
void saveLastFrame(std::filesystem::path const& video, std::filesystem::path const& png);

/// Joins clips one after another into a single MP4 (blocking; background thread).
void concatenate(std::vector<std::filesystem::path> const& clips, std::filesystem::path const& destination);

/// Thumbnail for an image or video file, decoded to about `pixels` wide (UI thread).
winrt::Windows::Foundation::IAsyncOperation<winrt::Microsoft::UI::Xaml::Media::ImageSource> thumbnail(
    std::filesystem::path file, bool video, int pixels);

}  // namespace fcapp::media
