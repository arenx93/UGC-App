#pragma once
// Prompt assistant: requests for KIE / OpenAI / Codex, reading answers, KIE task records,
// generation inputs and reference-file checks. Port of PromptRequests.swift, KieClient.swift, Media.swift.

#include <optional>
#include <string>
#include <vector>

#include "framecraft/models.h"
#include "framecraft/util.h"

namespace fc {

/// How a KIE chat model is called (routes and formats differ by model).
struct PromptAPI {
    enum class Kind {
        chat,       // OpenAI-style Chat Completions at /{slug}/v1/chat/completions; slugs tried in order.
        claude,     // Claude Messages at /claude/v1/messages with "model" in the body.
        responses,  // OpenAI Responses at /codex/v1/responses with "model" in the body (GPT 5.6).
    };
    Kind kind = Kind::chat;
    std::vector<std::string> slugs;  // chat
    std::string model;               // claude / responses
    bool operator==(const PromptAPI&) const = default;
};

struct PromptModel {
    std::string id;
    std::string name;
    std::string note;
    PromptAPI api;
};

enum class PromptProvider { kie, codex, openai };
const char* toString(PromptProvider provider);
PromptProvider promptProviderFrom(const std::string& text);
std::string providerTitle(PromptProvider provider);
std::string providerSummary(PromptProvider provider);

/// Everything the assistant knows about the request.
struct PromptBrief {
    std::string idea;
    MediaKind media = MediaKind::image;
    std::string targetModel;
    std::string aspect;
    std::string resolution;
    std::string camera = "N/A";
    std::string film = "N/A";
    std::optional<int> duration;
    std::optional<bool> generateAudio;
    std::optional<std::string> dialogueLanguage;
    std::vector<std::string> referenceTags;  // "@Image1 = producto.png"
    std::optional<std::string> previousPrompt;
    std::optional<std::string> feedback;
};

struct PromptResult {
    std::string prompt;
    std::optional<std::string> notes;
    bool operator==(const PromptResult&) const = default;
};

struct PromptExample {
    std::string title;
    std::string prompt;
    std::string author;
    std::string source;
};

struct KieRequest {
    std::string path;
    json body;
};

namespace prompts {

const std::vector<PromptModel>& promptModels();
inline const std::string kDefaultPromptModel = "gpt-5-6-terra";
const PromptModel& promptModel(const std::string& id);  // falls back to the default
inline const std::string kOpenAIModel = "gpt-4.1-mini";
constexpr int64_t kMaxAnalysisBytes = 8LL * 1024 * 1024;

const std::vector<std::string>& profileSections();

/// Requests to try in order for a KIE prompt model.
std::vector<KieRequest> kieRequests(const PromptModel& model, const std::string& instructions, const std::string& brief,
                                    const std::vector<std::string>& images, int maxTokens = 8000);
json responsesBody(const std::string& model, const std::string& instructions, const std::string& brief,
                   const std::vector<std::string>& images);
json claudeBody(const std::string& model, const std::string& instructions, const std::string& brief,
                const std::vector<std::string>& images, int maxTokens = 8000);
json kieChatBody(const std::string& model, const std::string& instructions, const std::string& brief,
                 const std::vector<std::string>& images);
json openAIBody(const std::string& instructions, const std::string& brief, const std::vector<std::string>& images,
                MediaKind media, bool structured, std::optional<int> maxOutputTokens = std::nullopt);

/// JSON text of the creative brief sent as the user message. `skill` is {"name","content"} or the JSON-profile context.
std::string briefJSON(const PromptBrief& brief, const std::optional<json>& skill);
std::string instructions(MediaKind media, bool structured);
/// Single prompt text for the Codex CLI (instructions + brief).
std::string codexCLIPrompt(const std::string& instructions, const std::string& brief);

// Reading non-streamed answers. Throw fc::Error when there is no text.
std::string readChat(const json& answer);
std::string readCodex(const json& answer);
std::string readClaude(const json& answer);
std::string readOpenAI(const json& answer);
std::string readAnswer(const PromptModel& model, const json& answer);

/// Validates and unpacks the model's answer.
PromptResult finalize(const std::string& raw, MediaKind media, bool structured);
/// The actionable text prompt inside a JSON visual profile, or nullopt when `text` is not a profile.
std::optional<std::string> mainPromptFromProfile(const std::string& text);
std::string stripFences(const std::string& text);
/// Parses the whole text as a JSON object, or the outermost {...} inside it.
std::optional<json> jsonObjectIn(const std::string& text);

}  // namespace prompts

/// Accumulates a server-sent-events answer (Chat Completions, Responses or Claude events).
class StreamAccumulator {
public:
    explicit StreamAccumulator(std::string secretToRedact = {}) : secret_(std::move(secretToRedact)) {}
    /// Feeds one line of the event stream. Returns true when the visible text changed.
    /// Throws fc::Error on error events. Sets done() on "[DONE]".
    bool feedLine(const std::string& line);
    const std::string& text() const { return text_; }
    std::string result() const { return finalText_.value_or(text_); }
    bool done() const { return done_; }

private:
    std::string secret_;
    std::string text_;
    std::optional<std::string> finalText_;
    bool done_ = false;
};

/// KIE task state from /api/v1/jobs/recordInfo.
struct KieTaskRecord {
    std::string state;
    int progress = 0;
    std::optional<std::string> failMessage;
    std::vector<std::string> resultURLs;
};

namespace kie {
inline const std::string kApiBase = "https://api.kie.ai";
inline const std::string kUploadURL = "https://kieai.redpandaai.co/api/file-stream-upload";
inline const std::string kKeyPageURL = "https://kie.ai/api-key";

std::string redact(const std::string& text, const std::string& secret);
std::string friendly(const std::string& message, int status);
/// Throws when a KIE JSON answer carries an error (HTTP status or "code" field).
void check(const json& answer, int status, const std::string& secret);
std::optional<double> parseCredits(const json& answer);
std::string parseTaskId(const json& answer);
KieTaskRecord parseRecord(const json& answer, const std::string& secret);
/// downloadUrl of a successful upload answer.
std::string parseUpload(const json& answer, int status);
/// Builds the multipart body for a reference upload.
std::string multipartUploadBody(const std::string& boundary, const std::string& fileBytes, const std::string& mime,
                                const std::string& displayName, const std::string& fileName);
/// Friendly error for an OpenAI HTTP status.
std::string openAIStatusMessage(int status);
}  // namespace kie

namespace inputs {
struct ImageTask {
    std::string model;
    json input;
};
ImageTask image(const std::string& model, const std::string& finalPrompt, const std::string& aspect,
                const std::string& resolution, const std::vector<std::string>& referenceURLs);
json video(const std::string& prompt, const std::string& resolution, const std::string& aspect, int duration,
           bool generateAudio, const std::vector<std::string>& images, const std::vector<std::string>& videos,
           const std::vector<std::string>& audios);
inline const std::string kUncertainSubmission =
    "Puede que el pedido haya llegado a KIE. Revisá tu historial en kie.ai antes de reintentar, para no pagar dos veces.";
}  // namespace inputs

/// File-type detection for reference files.
namespace media {
struct Match {
    ReferenceKind kind;
    std::string mime;
};
std::optional<Match> classifyExtension(std::string extension);
bool matchesSignature(const std::string& prefix, const std::string& mime, ReferenceKind kind);
std::string fileExtension(const std::string& mime);
std::string formattedDuration(double seconds);
}  // namespace media

}  // namespace fc
