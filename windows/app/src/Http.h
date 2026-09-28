#pragma once
// Blocking HTTPS client on WinHTTP (call it from a background thread).

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace fcapp {

struct HttpResponse {
    int status = 0;
    std::string contentType;
    std::string body;
};

struct HttpRequest {
    std::wstring method = L"GET";
    std::string url;
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    std::string body;
    int timeoutSeconds = 60;
};

/// Network failure (no HTTP answer). `timedOut` tells a timeout from other errors.
class HttpError : public std::runtime_error {
public:
    HttpError(std::string const& message, bool timedOut) : std::runtime_error(message), timedOut_(timedOut) {}
    bool timedOut() const { return timedOut_; }

private:
    bool timedOut_;
};

struct HttpStream {
    /// Called once with the status and Content-Type, before any data.
    std::function<void(int status, std::string const& contentType)> onHeaders;
    /// Called for every chunk; return false to stop reading.
    std::function<bool(std::string_view chunk)> onData;
};

HttpResponse httpSend(HttpRequest const& request);
/// Streams the body to `stream.onData` (the returned body stays empty).
HttpResponse httpSendStreaming(HttpRequest const& request, HttpStream const& stream);
/// Downloads to a file; returns status and content type. Stops at `maxBytes`.
HttpResponse httpDownload(std::string const& url, std::filesystem::path const& destination, int timeoutSeconds,
                          int64_t maxBytes);

}  // namespace fcapp
