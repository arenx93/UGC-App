#pragma once
// Windows helpers: UTF-8/UTF-16, known folders, shell, clipboard and small conversions.

#include <filesystem>
#include <string>

namespace fcapp {

std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view utf16);
inline winrt::hstring hs(std::string_view utf8) { return winrt::hstring(widen(utf8)); }
inline std::string str(winrt::hstring const& text) { return narrow(text); }

std::filesystem::path knownFolder(REFKNOWNFOLDERID id);
std::filesystem::path executableDirectory();

/// file:/// URI for a local path (images and media in XAML).
winrt::Windows::Foundation::Uri fileUri(std::filesystem::path const& path);
void openURL(std::string const& url);
void openPath(std::filesystem::path const& path);
/// Opens Explorer with the files selected.
void revealInExplorer(std::vector<std::filesystem::path> const& files);
void copyToClipboard(std::string const& text);

std::string readFileBytes(std::filesystem::path const& path, size_t limit = SIZE_MAX);
bool writeFileBytes(std::filesystem::path const& path, std::string const& bytes);

/// "hace 3 minutos"
std::string relativeTime(int64_t millisecondsSinceEpoch);
std::string formatDateTime(int64_t millisecondsSinceEpoch);
std::string formatNumber(double value, int fractionDigits = 0);
std::string fileTimestamp();

}  // namespace fcapp
