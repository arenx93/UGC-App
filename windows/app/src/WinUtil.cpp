#include "pch.h"

#include "WinUtil.h"

#include <fstream>
#include <sstream>

#include "framecraft/util.h"

namespace fcapp {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), length);
    return out;
}

std::string narrow(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
    std::string out(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), out.data(), length, nullptr, nullptr);
    return out;
}

std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw))) result = raw;
    CoTaskMemFree(raw);
    return result;
}

std::filesystem::path executableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
}

winrt::Windows::Foundation::Uri fileUri(std::filesystem::path const& path) {
    std::wstring text = path.wstring();
    std::replace(text.begin(), text.end(), L'\\', L'/');
    return winrt::Windows::Foundation::Uri(L"file:///" + text);
}

void openURL(std::string const& url) {
    ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openPath(std::filesystem::path const& path) {
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void revealInExplorer(std::vector<std::filesystem::path> const& files) {
    if (files.empty()) return;
    PIDLIST_ABSOLUTE folder = ILCreateFromPathW(files.front().parent_path().c_str());
    if (!folder) return;
    std::vector<PIDLIST_ABSOLUTE> items;
    for (auto const& file : files) {
        if (auto item = ILCreateFromPathW(file.c_str())) items.push_back(item);
    }
    std::vector<PCUITEMID_CHILD> children;
    for (auto item : items) children.push_back(ILFindLastID(item));
    SHOpenFolderAndSelectItems(folder, static_cast<UINT>(children.size()), children.data(), 0);
    for (auto item : items) ILFree(item);
    ILFree(folder);
}

void copyToClipboard(std::string const& text) {
    winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
    package.SetText(hs(text));
    winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
    winrt::Windows::ApplicationModel::DataTransfer::Clipboard::Flush();
}

std::string readFileBytes(std::filesystem::path const& path, size_t limit) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    if (limit == SIZE_MAX) {
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }
    std::string bytes(limit, '\0');
    stream.read(bytes.data(), static_cast<std::streamsize>(limit));
    bytes.resize(static_cast<size_t>(stream.gcount()));
    return bytes;
}

bool writeFileBytes(std::filesystem::path const& path, std::string const& bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

std::string relativeTime(int64_t ms) {
    int64_t seconds = (fc::nowMs() - ms) / 1000;
    if (seconds < 45) return "recién";
    int64_t minutes = seconds / 60;
    if (minutes < 1) return "hace 1 minuto";
    if (minutes < 60) return "hace " + std::to_string(minutes) + (minutes == 1 ? " minuto" : " minutos");
    int64_t hours = minutes / 60;
    if (hours < 24) return "hace " + std::to_string(hours) + (hours == 1 ? " hora" : " horas");
    int64_t days = hours / 24;
    if (days < 30) return days == 1 ? "ayer" : "hace " + std::to_string(days) + " días";
    int64_t months = days / 30;
    return months < 12 ? "hace " + std::to_string(months) + (months == 1 ? " mes" : " meses") : "hace más de un año";
}

std::string formatDateTime(int64_t ms) {
    time_t seconds = static_cast<time_t>(ms / 1000);
    tm local{};
    localtime_s(&local, &seconds);
    char buffer[64];
    strftime(buffer, sizeof buffer, "%d/%m/%Y %H:%M", &local);
    return buffer;
}

std::string formatNumber(double value, int fractionDigits) {
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%.*f", fractionDigits, value);
    std::string text = buffer;
    // Thousands separator (1.250) as used in Spanish.
    size_t dot = text.find('.');
    std::string integer = dot == std::string::npos ? text : text.substr(0, dot);
    std::string fraction = dot == std::string::npos ? "" : "," + text.substr(dot + 1);
    bool negative = !integer.empty() && integer[0] == '-';
    if (negative) integer.erase(0, 1);
    std::string grouped;
    for (size_t i = 0; i < integer.size(); ++i) {
        if (i && (integer.size() - i) % 3 == 0) grouped += '.';
        grouped += integer[i];
    }
    return (negative ? "-" : "") + grouped + fraction;
}

std::string fileTimestamp() {
    time_t now = time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    char buffer[32];
    strftime(buffer, sizeof buffer, "%Y%m%d-%H%M%S", &local);
    return buffer;
}

}  // namespace fcapp
