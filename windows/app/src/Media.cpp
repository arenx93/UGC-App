#include "pch.h"

#include "Media.h"

#include "framecraft/util.h"

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Media::Editing;
using namespace winrt::Windows::Graphics::Imaging;
namespace wf = winrt::Windows::Foundation;
namespace imaging = winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace fcapp::media {

std::optional<double> duration(std::filesystem::path const& file, bool video) {
    try {
        StorageFile storage = StorageFile::GetFileFromPathAsync(file.wstring()).get();
        wf::TimeSpan span{};
        if (video) {
            span = storage.Properties().GetVideoPropertiesAsync().get().Duration();
        } else {
            span = storage.Properties().GetMusicPropertiesAsync().get().Duration();
        }
        double seconds = std::chrono::duration<double>(span).count();
        if (seconds > 0) return seconds;
        // Some containers report no duration in the file properties: ask the media pipeline.
        MediaClip clip = MediaClip::CreateFromFileAsync(storage).get();
        seconds = std::chrono::duration<double>(clip.OriginalDuration()).count();
        return seconds > 0 ? std::optional<double>(seconds) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

void saveLastFrame(std::filesystem::path const& video, std::filesystem::path const& png) {
    StorageFile file = StorageFile::GetFileFromPathAsync(video.wstring()).get();
    MediaClip clip = MediaClip::CreateFromFileAsync(file).get();
    MediaComposition composition;
    composition.Clips().Append(clip);
    auto end = clip.OriginalDuration() - std::chrono::milliseconds(50);
    if (end.count() < 0) end = wf::TimeSpan{0};
    auto frame = composition.GetThumbnailAsync(end, 0, 0, VideoFramePrecision::NearestFrame).get();
    BitmapDecoder decoder = BitmapDecoder::CreateAsync(frame).get();
    SoftwareBitmap bitmap = decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied).get();

    std::filesystem::create_directories(png.parent_path());
    StorageFolder folder = StorageFolder::GetFolderFromPathAsync(png.parent_path().wstring()).get();
    StorageFile output = folder.CreateFileAsync(png.filename().wstring(), CreationCollisionOption::ReplaceExisting).get();
    auto stream = output.OpenAsync(FileAccessMode::ReadWrite).get();
    BitmapEncoder encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), stream).get();
    encoder.SetSoftwareBitmap(bitmap);
    encoder.FlushAsync().get();
    stream.Close();
}

void concatenate(std::vector<std::filesystem::path> const& clips, std::filesystem::path const& destination) {
    MediaComposition composition;
    for (auto const& path : clips) {
        StorageFile file = StorageFile::GetFileFromPathAsync(path.wstring()).get();
        composition.Clips().Append(MediaClip::CreateFromFileAsync(file).get());
    }
    std::filesystem::create_directories(destination.parent_path());
    StorageFolder folder = StorageFolder::GetFolderFromPathAsync(destination.parent_path().wstring()).get();
    StorageFile output = folder.CreateFileAsync(destination.filename().wstring(), CreationCollisionOption::ReplaceExisting).get();
    auto reason = composition.RenderToFileAsync(output, MediaTrimmingPreference::Precise).get();
    if (reason != winrt::Windows::Media::Transcoding::TranscodeFailureReason::None) {
        throw fc::Error("Windows no pudo unir los videos (código " + std::to_string(static_cast<int>(reason)) + ").", true);
    }
}

wf::IAsyncOperation<winrt::Microsoft::UI::Xaml::Media::ImageSource> thumbnail(std::filesystem::path file, bool video, int pixels) {
    imaging::BitmapImage bitmap;
    bitmap.DecodePixelWidth(pixels);
    try {
        StorageFile storage = co_await StorageFile::GetFileFromPathAsync(file.wstring());
        if (video) {
            auto thumb = co_await storage.GetThumbnailAsync(FileProperties::ThumbnailMode::VideosView, static_cast<uint32_t>(pixels));
            if (!thumb) co_return nullptr;
            co_await bitmap.SetSourceAsync(thumb);
        } else {
            auto stream = co_await storage.OpenReadAsync();
            co_await bitmap.SetSourceAsync(stream);
        }
    } catch (...) {
        co_return nullptr;
    }
    co_return bitmap;
}

}  // namespace fcapp::media
