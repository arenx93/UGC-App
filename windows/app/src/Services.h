#pragma once
// Network and system services: KIE, OpenAI, Windows Credential Manager, Codex CLI.
// Every blocking call must run on a background thread.

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "framecraft/prompts.h"

namespace fcapp {

// MARK: KIE

class KieService {
public:
    explicit KieService(std::string key);

    std::optional<double> credits() const;
    std::string createTask(std::string const& model, fc::json const& input) const;
    fc::KieTaskRecord recordInfo(std::string const& taskId) const;
    /// Uploads a local reference file and returns KIE's temporary download URL.
    std::string upload(std::filesystem::path const& file, std::string const& mime, std::string const& displayName,
                       std::string const& fileName) const;
    /// Streams a prompt-assistant request; `onText` receives the text accumulated so far.
    std::string streamText(std::string const& path, fc::json const& body, fc::PromptModel const& model,
                           std::function<void(std::string const&)> const& onText) const;
    /// Tries each documented route for the model until one answers.
    std::string runModel(fc::PromptModel const& model, std::string const& instructions, std::string const& brief,
                         std::vector<std::string> const& images, int maxTokens,
                         std::function<void(std::string const&)> const& onText) const;

private:
    fc::json call(std::string const& path, fc::json const* body, int timeoutSeconds = 55) const;
    std::string key_;
};

// MARK: OpenAI

fc::json openAIResponses(std::string const& key, fc::json const& body);

// MARK: Credentials (Windows Credential Manager)

namespace credentials {
inline constexpr wchar_t kKie[] = L"Framecraft/kie";
inline constexpr wchar_t kOpenAI[] = L"Framecraft/openai";
std::optional<std::string> read(wchar_t const* target);
bool write(wchar_t const* target, std::string const& secret);
void remove(wchar_t const* target);
}  // namespace credentials

// MARK: Codex CLI

namespace codex {

enum class State { unknown, checking, notInstalled, loggedOut, loggingIn, loggedIn };

struct Status {
    State state = State::unknown;
    std::string detail;
};

inline const std::string kNpmCommand = "npm install -g @openai/codex";
inline const std::string kWingetCommand = "winget install OpenAI.Codex";

/// codex.exe bundled next to Framecraft.exe, or found in PATH.
std::optional<std::filesystem::path> binary();
Status status();
/// Opens the browser to sign in with ChatGPT; returns when the login finishes.
void login();
void logout();
/// Runs one non-interactive request (prompt through stdin) and returns the final answer.
std::string generate(std::string const& prompt, std::vector<std::filesystem::path> const& images);

}  // namespace codex

}  // namespace fcapp
