#include "pch.h"

#include "Http.h"

#include <fstream>

#include "WinUtil.h"

namespace fcapp {

namespace {

struct Handle {
    HINTERNET value = nullptr;
    Handle() = default;
    explicit Handle(HINTERNET h) : value(h) {}
    Handle(Handle const&) = delete;
    Handle& operator=(Handle const&) = delete;
    ~Handle() {
        if (value) WinHttpCloseHandle(value);
    }
    explicit operator bool() const { return value != nullptr; }
};

HINTERNET session() {
    static HINTERNET shared = [] {
        HINTERNET handle = WinHttpOpen(L"Framecraft/1.0 (Windows)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0);
        if (!handle) {
            handle = WinHttpOpen(L"Framecraft/1.0 (Windows)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);
        }
        if (handle) {
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            WinHttpSetOption(handle, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
            DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(handle, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof decompression);
        }
        return handle;
    }();
    return shared;
}

[[noreturn]] void fail(char const* what) {
    DWORD code = GetLastError();
    bool timedOut = code == ERROR_WINHTTP_TIMEOUT;
    std::string message = timedOut ? "La conexión tardó demasiado." : std::string("No se pudo conectar (") + what + " " + std::to_string(code) + ").";
    throw HttpError(message, timedOut);
}

std::wstring queryHeader(HINTERNET request, DWORD info) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) return {};
    value.resize(size / sizeof(wchar_t));
    return value;
}

/// Opens, sends and receives the headers of a request. Calls `read` with the request handle.
HttpResponse perform(HttpRequest const& request, std::function<void(HINTERNET, HttpResponse&)> const& read) {
    if (!session()) fail("WinHttpOpen");
    std::wstring url = widen(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    wchar_t host[256]{}, path[4096]{}, extra[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) throw HttpError("Dirección inválida.", false);

    Handle connection(WinHttpConnect(session(), host, parts.nPort, 0));
    if (!connection) fail("WinHttpConnect");
    std::wstring target = std::wstring(path) + extra;
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle handle(WinHttpOpenRequest(connection.value, request.method.c_str(), target.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!handle) fail("WinHttpOpenRequest");
    int timeout = request.timeoutSeconds * 1000;
    WinHttpSetTimeouts(handle.value, 30000, 30000, timeout, timeout);

    std::wstring headers;
    for (auto const& [name, value] : request.headers) headers += name + L": " + value + L"\r\n";
    BOOL sent = WinHttpSendRequest(handle.value, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                   headers.empty() ? 0 : static_cast<DWORD>(-1L),
                                   request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
                                   static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0);
    if (!sent) fail("WinHttpSendRequest");
    if (!WinHttpReceiveResponse(handle.value, nullptr)) fail("WinHttpReceiveResponse");

    HttpResponse response;
    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(handle.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                        &size, WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status);
    response.contentType = narrow(queryHeader(handle.value, WINHTTP_QUERY_CONTENT_TYPE));
    read(handle.value, response);
    return response;
}

/// Reads the body in chunks; `sink` returns false to stop.
void readBody(HINTERNET handle, std::function<bool(std::string_view)> const& sink) {
    std::string buffer;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(handle, &available)) fail("WinHttpQueryDataAvailable");
        if (available == 0) break;
        buffer.resize(available);
        DWORD read = 0;
        if (!WinHttpReadData(handle, buffer.data(), available, &read)) fail("WinHttpReadData");
        if (read == 0) break;
        if (!sink(std::string_view(buffer.data(), read))) break;
    }
}

}  // namespace

HttpResponse httpSend(HttpRequest const& request) {
    return perform(request, [](HINTERNET handle, HttpResponse& response) {
        readBody(handle, [&](std::string_view chunk) {
            response.body.append(chunk);
            return response.body.size() < 64u * 1024 * 1024;
        });
    });
}

HttpResponse httpSendStreaming(HttpRequest const& request, HttpStream const& stream) {
    return perform(request, [&](HINTERNET handle, HttpResponse& response) {
        if (stream.onHeaders) stream.onHeaders(response.status, response.contentType);
        readBody(handle, [&](std::string_view chunk) { return stream.onData ? stream.onData(chunk) : true; });
    });
}

HttpResponse httpDownload(std::string const& url, std::filesystem::path const& destination, int timeoutSeconds, int64_t maxBytes) {
    HttpRequest request;
    request.url = url;
    request.timeoutSeconds = timeoutSeconds;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    std::ofstream file(destination, std::ios::binary | std::ios::trunc);
    if (!file) throw HttpError("No se pudo crear el archivo.", false);
    int64_t written = 0;
    bool tooLarge = false;
    HttpResponse response = perform(request, [&](HINTERNET handle, HttpResponse& answer) {
        if (answer.status < 200 || answer.status >= 300) return;
        readBody(handle, [&](std::string_view chunk) {
            written += static_cast<int64_t>(chunk.size());
            if (written > maxBytes) {
                tooLarge = true;
                return false;
            }
            file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            return static_cast<bool>(file);
        });
    });
    file.close();
    if (tooLarge) {
        std::filesystem::remove(destination, error);
        throw HttpError("El archivo supera el tamaño permitido.", false);
    }
    return response;
}

}  // namespace fcapp
