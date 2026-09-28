#include "pch.h"

#include "Services.h"

#include <thread>

#include "Http.h"
#include "WinUtil.h"
#include "framecraft/util.h"

namespace fcapp {

using fc::json;

// MARK: - KIE

KieService::KieService(std::string key) : key_(fc::trim(key)) {}

json KieService::call(std::string const& path, json const* body, int timeoutSeconds) const {
    HttpRequest request;
    request.method = body ? L"POST" : L"GET";
    request.url = fc::kie::kApiBase + path;
    request.timeoutSeconds = timeoutSeconds;
    request.headers.push_back({L"Authorization", L"Bearer " + widen(key_)});
    if (body) {
        request.headers.push_back({L"Content-Type", L"application/json"});
        request.body = body->dump();
    }
    HttpResponse response;
    try {
        response = httpSend(request);
    } catch (HttpError const& error) {
        throw fc::Error(error.timedOut() ? "KIE tardó demasiado en responder." : "No se pudo conectar con KIE. Revisá tu conexión.", false);
    }
    json answer = json::parse(response.body, nullptr, false);
    if (answer.is_discarded() || !answer.is_object()) {
        throw fc::Error("KIE devolvió una respuesta inválida (" + std::to_string(response.status) + ").",
                        response.status >= 400 && response.status < 500);
    }
    fc::kie::check(answer, response.status, key_);
    return answer;
}

std::optional<double> KieService::credits() const { return fc::kie::parseCredits(call("/api/v1/chat/credit", nullptr)); }

std::string KieService::createTask(std::string const& model, json const& input) const {
    json body = {{"model", model}, {"input", input}};
    return fc::kie::parseTaskId(call("/api/v1/jobs/createTask", &body));
}

fc::KieTaskRecord KieService::recordInfo(std::string const& taskId) const {
    std::string encoded;
    for (unsigned char c : taskId) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            char hex[4];
            snprintf(hex, sizeof hex, "%%%02X", c);
            encoded += hex;
        }
    }
    return fc::kie::parseRecord(call("/api/v1/jobs/recordInfo?taskId=" + encoded, nullptr), key_);
}

std::string KieService::upload(std::filesystem::path const& file, std::string const& mime, std::string const& displayName,
                               std::string const& fileName) const {
    std::string bytes = readFileBytes(file);
    if (bytes.empty()) throw fc::Error("No se pudo leer la referencia.", true);
    std::string boundary = "Framecraft-" + fc::newUUID();
    HttpRequest request;
    request.method = L"POST";
    request.url = fc::kie::kUploadURL;
    request.timeoutSeconds = 180;
    request.headers.push_back({L"Authorization", L"Bearer " + widen(key_)});
    request.headers.push_back({L"Content-Type", L"multipart/form-data; boundary=" + widen(boundary)});
    request.body = fc::kie::multipartUploadBody(boundary, bytes, mime, displayName, fileName);
    HttpResponse response;
    try {
        response = httpSend(request);
    } catch (HttpError const&) {
        throw fc::Error("No se pudo subir la referencia a KIE. Revisá tu conexión e intentá de nuevo.", true);
    }
    json answer = json::parse(response.body, nullptr, false);
    if (answer.is_discarded()) throw fc::Error("KIE devolvió una respuesta inválida al subir la referencia.", true);
    return fc::kie::parseUpload(answer, response.status);
}

std::string KieService::streamText(std::string const& path, json const& body, fc::PromptModel const& model,
                                   std::function<void(std::string const&)> const& onText) const {
    HttpRequest request;
    request.method = L"POST";
    request.url = fc::kie::kApiBase + path;
    request.timeoutSeconds = 300;
    request.headers.push_back({L"Authorization", L"Bearer " + widen(key_)});
    request.headers.push_back({L"Content-Type", L"application/json"});
    request.headers.push_back({L"Accept", L"text/event-stream, application/json"});
    request.body = body.dump();

    fc::StreamAccumulator accumulator(key_);
    bool streaming = false;
    int status = 0;
    std::string pending;  // partial line (stream) or whole body (JSON)
    HttpStream stream;
    stream.onHeaders = [&](int code, std::string const& contentType) {
        status = code;
        streaming = fc::contains(contentType, "event-stream") && code >= 200 && code < 300;
    };
    stream.onData = [&](std::string_view chunk) {
        pending.append(chunk);
        if (!streaming) return pending.size() < 8u * 1024 * 1024;
        size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (accumulator.feedLine(line)) onText(accumulator.text());
            if (accumulator.done()) return false;
        }
        return true;
    };
    try {
        httpSendStreaming(request, stream);
    } catch (HttpError const& error) {
        if (streaming && !accumulator.text().empty()) return accumulator.result();
        throw fc::Error(error.timedOut() ? "KIE no respondió a tiempo. Probá de nuevo o elegí un modelo más rápido."
                                         : "No se pudo conectar con KIE. Revisá tu conexión.",
                        true);
    }
    if (streaming) {
        if (!pending.empty()) accumulator.feedLine(pending);
        return accumulator.result();
    }
    json answer = json::parse(pending, nullptr, false);
    if (answer.is_discarded() || !answer.is_object()) {
        throw fc::Error("KIE devolvió una respuesta inválida (" + std::to_string(status) + "). " + fc::prefixCharacters(pending, 200), true);
    }
    fc::kie::check(answer, status, key_);
    std::string text = fc::prompts::readAnswer(model, answer);
    onText(text);
    return text;
}

std::string KieService::runModel(fc::PromptModel const& model, std::string const& instructions, std::string const& brief,
                                 std::vector<std::string> const& images, int maxTokens,
                                 std::function<void(std::string const&)> const& onText) const {
    std::optional<fc::Error> last;
    for (auto const& request : fc::prompts::kieRequests(model, instructions, brief, images, maxTokens)) {
        try {
            return streamText(request.path, request.body, model, onText);
        } catch (fc::Error const& error) {
            last = error;
        }
    }
    throw last.value_or(fc::Error("KIE no respondió.", true));
}

// MARK: - OpenAI

json openAIResponses(std::string const& key, json const& body) {
    HttpRequest request;
    request.method = L"POST";
    request.url = "https://api.openai.com/v1/responses";
    request.timeoutSeconds = 90;
    request.headers.push_back({L"Authorization", L"Bearer " + widen(fc::trim(key))});
    request.headers.push_back({L"Content-Type", L"application/json"});
    request.body = body.dump();
    HttpResponse response;
    try {
        response = httpSend(request);
    } catch (HttpError const&) {
        throw fc::Error("No se pudo conectar con OpenAI a tiempo. Tu idea sigue ahí: probá de nuevo.", true);
    }
    if (response.status < 200 || response.status >= 300) throw fc::Error(fc::kie::openAIStatusMessage(response.status), true);
    json answer = json::parse(response.body, nullptr, false);
    if (answer.is_discarded()) throw fc::Error("OpenAI devolvió una respuesta inválida.", true);
    return answer;
}

// MARK: - Credentials

namespace credentials {

std::optional<std::string> read(wchar_t const* target) {
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target, CRED_TYPE_GENERIC, 0, &credential)) return std::nullopt;
    std::string secret(reinterpret_cast<char const*>(credential->CredentialBlob), credential->CredentialBlobSize);
    CredFree(credential);
    if (secret.empty()) return std::nullopt;
    return secret;
}

bool write(wchar_t const* target, std::string const& secret) {
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(target);
    credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<wchar_t*>(L"Framecraft");
    return CredWriteW(&credential, 0) != FALSE;
}

void remove(wchar_t const* target) { CredDeleteW(target, CRED_TYPE_GENERIC, 0); }

}  // namespace credentials

// MARK: - Codex CLI

namespace codex {

namespace {

struct ProcessResult {
    DWORD exitCode = 1;
    std::string out;
    std::string err;
    bool timedOut = false;
};

/// Quotes one argument for CommandLineToArgvW / the MSVC runtime.
std::wstring quote(std::wstring const& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
    std::wstring quoted = L"\"";
    for (size_t i = 0;; ++i) {
        size_t backslashes = 0;
        while (i < argument.size() && argument[i] == L'\\') {
            ++i;
            ++backslashes;
        }
        if (i == argument.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        if (argument[i] == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
        } else {
            quoted.append(backslashes, L'\\');
        }
        quoted += argument[i];
    }
    quoted += L'"';
    return quoted;
}

std::string drain(HANDLE pipe) {
    std::string data;
    char buffer[8192];
    DWORD read = 0;
    while (ReadFile(pipe, buffer, sizeof buffer, &read, nullptr) && read > 0) data.append(buffer, read);
    return data;
}

ProcessResult run(std::vector<std::wstring> const& arguments, std::string const& input, int timeoutSeconds) {
    auto exe = binary();
    if (!exe) throw fc::Error("No se encontró Codex.", true);
    std::wstring commandLine;
    std::wstring extension = exe->extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    bool script = extension == L".cmd" || extension == L".bat";
    commandLine = quote(exe->wstring());
    for (auto const& argument : arguments) commandLine += L" " + quote(argument);
    if (script) commandLine = L"cmd.exe /d /s /c \"" + commandLine + L"\"";

    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE outRead, outWrite, errRead, errWrite, inRead, inWrite;
    CreatePipe(&outRead, &outWrite, &inherit, 0);
    CreatePipe(&errRead, &errWrite, &inherit, 0);
    CreatePipe(&inRead, &inWrite, &inherit, 0);
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = inRead;
    startup.hStdOutput = outWrite;
    startup.hStdError = errWrite;
    PROCESS_INFORMATION process{};

    // Environment: plain output, no colors.
    SetEnvironmentVariableW(L"NO_COLOR", L"1");
    std::wstring workingDirectory = std::filesystem::temp_directory_path().wstring();
    BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                  workingDirectory.c_str(), &startup, &process);
    CloseHandle(outWrite);
    CloseHandle(errWrite);
    CloseHandle(inRead);
    if (!started) {
        CloseHandle(outRead);
        CloseHandle(errRead);
        CloseHandle(inWrite);
        throw fc::Error("No se pudo iniciar Codex.", true);
    }
    ProcessResult result;
    std::thread writer([&] {
        DWORD written = 0;
        size_t offset = 0;
        while (offset < input.size() &&
               WriteFile(inWrite, input.data() + offset, static_cast<DWORD>(std::min<size_t>(input.size() - offset, 1 << 16)), &written, nullptr)) {
            offset += written;
        }
        CloseHandle(inWrite);
    });
    std::thread outReader([&] { result.out = drain(outRead); });
    std::thread errReader([&] { result.err = drain(errRead); });
    if (WaitForSingleObject(process.hProcess, static_cast<DWORD>(timeoutSeconds) * 1000) == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
    }
    GetExitCodeProcess(process.hProcess, &result.exitCode);
    writer.join();
    outReader.join();
    errReader.join();
    CloseHandle(outRead);
    CloseHandle(errRead);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

std::string lastLines(std::string const& text) {
    auto lines = fc::splitLines(fc::trim(text));
    std::vector<std::string> tail(lines.size() > 3 ? lines.end() - 3 : lines.begin(), lines.end());
    return fc::trim(fc::join(tail, " "));
}

}  // namespace

std::optional<std::filesystem::path> binary() {
    std::error_code error;
    auto bundled = executableDirectory() / L"codex.exe";
    if (std::filesystem::exists(bundled, error)) return bundled;
    for (wchar_t const* name : {L"codex.exe", L"codex.cmd"}) {
        wchar_t found[MAX_PATH];
        if (SearchPathW(nullptr, name, nullptr, MAX_PATH, found, nullptr)) return std::filesystem::path(found);
    }
    return std::nullopt;
}

Status status() {
    if (!binary()) return {State::notInstalled, ""};
    ProcessResult version;
    try {
        version = run({L"--version"}, "", 20);
    } catch (fc::Error const&) {
        return {State::notInstalled, ""};
    }
    if (version.exitCode != 0) return {State::notInstalled, ""};
    ProcessResult login = run({L"login", L"status"}, "", 20);
    std::string text = fc::trim(login.out + "\n" + login.err);
    if (login.exitCode == 0 && !fc::contains(fc::lower(text), "not logged in")) {
        auto lines = fc::splitLines(text);
        return {State::loggedIn, lines.empty() || lines[0].empty() ? "Sesión iniciada" : lines[0]};
    }
    return {State::loggedOut, ""};
}

void login() {
    ProcessResult result = run({L"login"}, "", 600);
    if (result.exitCode != 0 || result.timedOut) {
        throw fc::Error("No se completó el inicio de sesión de Codex. " + lastLines(result.err), true);
    }
}

void logout() {
    try {
        run({L"logout"}, "", 30);
    } catch (...) {
    }
}

std::string generate(std::string const& prompt, std::vector<std::filesystem::path> const& images) {
    auto folder = std::filesystem::temp_directory_path() / widen("framecraft-codex-" + fc::newUUID());
    std::filesystem::create_directories(folder);
    auto answerFile = folder / L"answer.txt";
    std::vector<std::wstring> arguments = {L"exec", L"--skip-git-repo-check", L"--sandbox", L"read-only", L"--cd", folder.wstring(),
                                           L"--output-last-message", answerFile.wstring(), L"-"};
    for (auto const& image : images) {
        arguments.push_back(L"--image");
        arguments.push_back(image.wstring());
    }
    ProcessResult result = run(arguments, prompt, 600);
    std::string text = fc::trim(readFileBytes(answerFile));
    std::error_code error;
    std::filesystem::remove_all(folder, error);
    if (!text.empty()) return text;
    std::string log = fc::lower(result.err + "\n" + result.out);
    if (fc::contains(log, "login") || fc::contains(log, "unauthorized") || fc::contains(log, "401"))
        throw fc::Error("Codex no tiene una sesión activa. Iniciá sesión con tu cuenta de ChatGPT.", true);
    if (fc::contains(log, "usage limit") || fc::contains(log, "rate limit"))
        throw fc::Error("Llegaste al límite de uso de tu plan de ChatGPT en Codex. Probá más tarde o usá KIE.", true);
    if (result.timedOut) throw fc::Error("Codex tardó demasiado. Probá de nuevo.", true);
    throw fc::Error("Codex no devolvió un prompt. " + lastLines(result.err), true);
}

}  // namespace codex

}  // namespace fcapp
