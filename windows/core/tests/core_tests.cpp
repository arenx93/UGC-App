// Tests for the Framecraft C++ core. Mirrors macos/Tests/FramecraftCoreTests (same fixtures).
// Usage: framecraft_core_tests <repository root>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "framecraft/library.h"
#include "framecraft/linter.h"
#include "framecraft/presets.h"
#include "framecraft/prompts.h"
#include "framecraft/skills.h"
#include "framecraft/stories.h"
#include "framecraft/templates.h"
#include "framecraft/util.h"

using namespace fc;
namespace fs = std::filesystem;

static int failures = 0;
static int checks = 0;
static std::string currentTest;
static fs::path repoRoot;

#define CHECK(condition)                                                                              \
    do {                                                                                              \
        ++checks;                                                                                     \
        if (!(condition)) {                                                                           \
            ++failures;                                                                               \
            std::cerr << "FAIL [" << currentTest << "] " << __FILE__ << ":" << __LINE__ << ": " #condition "\n"; \
        }                                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                                \
    do {                                                                                              \
        ++checks;                                                                                     \
        auto _a = (a);                                                                                \
        auto _b = (b);                                                                                \
        if (!(_a == _b)) {                                                                            \
            ++failures;                                                                               \
            std::cerr << "FAIL [" << currentTest << "] " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n"; \
        }                                                                                             \
    } while (0)

template <typename F>
bool throwsError(F&& f) {
    try {
        f();
    } catch (const Error&) {
        return true;
    }
    return false;
}

static std::string readFile(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// MARK: - Web parity

static void testWebParity() {
    json fixture = json::parse(readFile(repoRoot / "macos/Tests/FramecraftCoreTests/Fixtures/web-parity.json"));
    for (const auto& item : fixture["ratios"]) {
        auto expected = item["ratios"].get<std::vector<std::string>>();
        CHECK(presets::ratios(item["model"], item["resolution"]) == expected);
    }
    int composed = 0;
    for (const auto& item : fixture["compose"]) {
        std::string result = presets::composePrompt(item["prompt"], item["camera"], item["film"], item["aspect"].get<std::string>());
        std::string expected = item["result"];
        json expectedJSON = json::parse(expected, nullptr, false);
        if (!expectedJSON.is_discarded() && expectedJSON.is_object()) {
            json actual = json::parse(result, nullptr, false);
            CHECK(actual == expectedJSON);
        } else {
            if (result != expected) std::cerr << "compose mismatch:\n" << result << "\n---\n" << expected << "\n";
            CHECK_EQ(result, expected);
        }
        ++composed;
    }
    CHECK(composed > 0);

    auto resources = SkillResources::locate(repoRoot / "windows");
    CHECK(resources.has_value());
    if (!resources) return;
    CHECK_EQ(resources->examples().size(), size_t(123));
    for (const auto& item : fixture["examples"]) {
        std::vector<std::string> titles;
        for (const auto& e : skills::relevantExamples(item["idea"], resources->examples())) titles.push_back(e.title);
        auto expected = item["titles"].get<std::vector<std::string>>();
        if (titles != expected) std::cerr << "examples mismatch for idea: " << item["idea"].get<std::string>() << "\n";
        CHECK(titles == expected);
    }
}

// MARK: - Skills

static void testSkills() {
    auto resources = SkillResources::locate(repoRoot / "windows");
    CHECK(resources.has_value());
    if (!resources) return;
    auto list = skills::builtins(&*resources);
    CHECK_EQ(list.size(), size_t(3));
    CHECK_EQ(list[0].id, skills::kJsonProfileID);
    CHECK_EQ(list[1].id, skills::kUgcID);
    CHECK_EQ(list[2].id, skills::kArthasID);
    CHECK(list[1].media == SkillMedia::video);
    CHECK(contains(list[1].content, "Principio rector"));
    CHECK(contains(list[1].content, "PACING"));
    CHECK(!startsWith(list[1].content, "---"));
    CHECK(!resources->jsonProfile().empty());
    CHECK(contains(skills::walterExample(&*resources), "BLOQUE BASE"));
    CHECK(fs::exists(resources->guidePDF()));

    auto parsed = skills::parse("---\nname: \"mi-skill\"\ndescription: 'Escribe prompts de video\n  para Seedance.'\n---\n\n# Cuerpo\nTexto.", "x");
    CHECK_EQ(parsed.name, std::string("mi-skill"));
    CHECK_EQ(parsed.summary, std::string("Escribe prompts de video para Seedance."));
    CHECK_EQ(parsed.body, std::string("# Cuerpo\nTexto."));
    CHECK(skills::guessMedia(parsed.name, parsed.summary, parsed.body) == SkillMedia::video);
    auto plain = skills::parse("Solo texto", "archivo");
    CHECK_EQ(plain.name, std::string("archivo"));
    CHECK_EQ(plain.body, std::string("Solo texto"));
}

// MARK: - Linter

static void testLinter() {
    const std::string good =
        "FORMAT: Vertical 9:16, 10 seconds, one continuous take, no cuts. Deep depth of field, the background is fully readable.\n"
        "0–5 s: she lifts the jar. Text in parentheses before a spoken line is acting direction only. It is NEVER spoken aloud.\n"
        "P1 (quiet, a breath before the last word): \"I have been using this every single morning and my skin finally stopped fighting me.\"\n"
        "5–10 s: P1 (laughing, volume up): \"Honestly I did not expect it to work this fast, look at this.\"\n"
        "Match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1 and do not copy its emotion.\n"
        "@Image1 is the product. No music, no text, no gimbal, no bokeh.";
    auto report = linter::lintVideo(good, 10, {1, 0, 1}, true);
    for (const auto& item : report.items)
        if (item.state == LintItem::State::warning || item.state == LintItem::State::problem) std::cerr << "  unexpected: " << item.title << "\n";
    CHECK_EQ(report.problems(), 0);
    CHECK_EQ(report.warnings(), 0);
    CHECK_EQ(report.targetWords, 25);

    std::string bad = "A cinematic shot with shallow depth of field and bokeh. She says \"hi\". Use @Image3 as the start.";
    auto badReport = linter::lintVideo(bad, 30, {2, 0, 0}, true);
    auto state = [&](const char* id) { const auto* item = badReport.item(id); return item ? item->state : LintItem::State::info; };
    CHECK(state("refs") == LintItem::State::problem);
    CHECK(state("timeline") == LintItem::State::warning);
    CHECK(state("dialogue") == LintItem::State::warning);
    CHECK(state("adwords") == LintItem::State::warning);
    CHECK(state("restrictions") == LintItem::State::warning);

    CHECK(linter::hasTimestamps("0–6 s: walks in"));
    CHECK(linter::hasTimestamps("from 12-20s the camera"));
    CHECK(linter::hasTimestamps("[00:00 - 00:03]"));
    CHECK(!linter::hasTimestamps("a 26mm lens at 1.5 m"));
    CHECK_EQ(linter::dialogueWords("He says “one two three” and \"four five\"."), 5);
    CHECK_EQ(linter::highestTag("@Image", "@Image1 and @image12"), 12);
    CHECK(!linter::containsUnnegated("no gimbal, steadicam or dolly", "steadicam"));
    CHECK(linter::containsUnnegated("a smooth gimbal shot", "gimbal"));
    CHECK(!linter::containsUnnegated("a pianolut", "lut"));
    CHECK_EQ(dialogue::targetWords(30), 74);
}

// MARK: - Prompt requests

static void testPromptRequests() {
    auto result = prompts::finalize("```json\n{\"prompt\": \"FORMAT: vertical\", \"notes\": \"Cargá @Image1 primero.\"}\n```", MediaKind::video, false);
    CHECK_EQ(result.prompt, std::string("FORMAT: vertical"));
    CHECK(result.notes == std::optional<std::string>("Cargá @Image1 primero."));
    CHECK_EQ(prompts::finalize("Just a prompt", MediaKind::video, false).prompt, std::string("Just a prompt"));

    CHECK(prompts::mainPromptFromProfile("{\"generation_parameters\":{\"prompts\":[\"A waiter\"]}}") == std::optional<std::string>("A waiter"));
    CHECK(!prompts::mainPromptFromProfile("Plain prompt").has_value());

    std::vector<std::string> sections;
    for (const auto& s : prompts::profileSections()) sections.push_back("\"" + s + "\": {}");
    CHECK(!throwsError([&] { prompts::finalize("{" + join(sections, ",") + "}", MediaKind::image, true); }));
    CHECK(throwsError([&] { prompts::finalize("{\"metadata\": {}}", MediaKind::image, true); }));

    CHECK_EQ(prompts::readChat(json::parse(R"({"choices":[{"message":{"content":" hola "}}]})")), std::string("hola"));
    CHECK_EQ(prompts::readCodex(json::parse(R"({"output":[{"content":[{"type":"output_text","text":"a"}]}]})")), std::string("a"));
    CHECK_EQ(prompts::readOpenAI(json::parse(R"({"output":[{"type":"message","content":[{"type":"output_text","text":"b"}]}]})")), std::string("b"));
    CHECK(throwsError([] { prompts::readChat(json::object()); }));

    PromptBrief brief;
    brief.idea = "idea";
    brief.media = MediaKind::video;
    brief.targetModel = presets::kVideoModelID;
    brief.aspect = "9:16";
    brief.resolution = "720p";
    brief.duration = 20;
    brief.dialogueLanguage = "Español";
    brief.referenceTags = {"@Image1 = a.png"};
    json briefJSON = json::parse(prompts::briefJSON(brief, std::nullopt));
    CHECK_EQ(briefJSON["targetDialogueWords"].get<int>(), 49);
    CHECK(briefJSON["referenceTags"] == json::array({"@Image1 = a.png"}));

    auto image = inputs::image("gpt-image-2", "p", "1:1", "1K", {});
    CHECK_EQ(image.model, std::string("gpt-image-2-text-to-image"));
    auto nano = inputs::image("nano-banana-pro", "p", "1:1", "1K", {"https://x/y.png"});
    CHECK_EQ(nano.model, std::string("nano-banana-pro"));
    CHECK(nano.input["image_input"] == json::array({"https://x/y.png"}));

    for (const auto& model : prompts::promptModels()) CHECK(!startsWith(model.id, "gemini-2.5"));
    const auto& gemini = prompts::promptModel("gemini-3.8-flash");
    CHECK(gemini.api.slugs == std::vector<std::string>({"gemini-3.8-flash", "gemini-3-8-flash"}));
    auto claude = prompts::kieRequests(prompts::promptModel("claude-opus-4-6"), "i", "b", {"https://x/y.png"});
    CHECK_EQ(claude.size(), size_t(1));
    CHECK_EQ(claude[0].path, std::string("/claude/v1/messages"));
    CHECK_EQ(claude[0].body["model"].get<std::string>(), std::string("claude-opus-4-6"));
    CHECK_EQ(prompts::readClaude(json::parse(R"({"content":[{"type":"text","text":"hola"}]})")), std::string("hola"));
    CHECK_EQ(prompts::readChat(json::parse(R"({"code":200,"data":{"choices":[{"message":{"content":"x"}}]}})")), std::string("x"));
    auto terra = prompts::kieRequests(prompts::promptModel(prompts::kDefaultPromptModel), "i", "b", {});
    CHECK_EQ(terra[0].path, std::string("/codex/v1/responses"));
    CHECK_EQ(terra[0].body["model"].get<std::string>(), std::string("gpt-5-6-terra"));
    CHECK(terra[0].body.contains("input"));
    CHECK_EQ(prompts::kieRequests(prompts::promptModel("gpt-5-2"), "i", "b", {})[0].path, std::string("/gpt-5-2/v1/chat/completions"));
    CHECK(prompts::kieChatBody("gemini-3.8-flash", "i", "b", {})["stream"] == true);
    CHECK(endsWith(prompts::codexCLIPrompt("I", "B"), "Creative brief:\nB"));
    CHECK_EQ(prompts::promptModel("unknown").id, prompts::kDefaultPromptModel);
}

static void testStreaming() {
    StreamAccumulator chat;
    CHECK(chat.feedLine("data: {\"choices\":[{\"delta\":{\"content\":\"Ho\"}}]}"));
    CHECK(chat.feedLine("data: {\"choices\":[{\"delta\":{\"content\":\"la\"}}]}\r"));
    CHECK(!chat.feedLine(": keep-alive"));
    chat.feedLine("data: [DONE]");
    CHECK(chat.done());
    CHECK_EQ(chat.result(), std::string("Hola"));

    StreamAccumulator responses;
    responses.feedLine(R"(data: {"type":"response.output_text.delta","delta":"A"})");
    responses.feedLine(R"(data: {"type":"response.output_text.delta","delta":"B"})");
    responses.feedLine(R"(data: {"type":"response.completed","response":{"output":[{"content":[{"type":"output_text","text":"AB final"}]}]}})");
    CHECK_EQ(responses.text(), std::string("AB"));
    CHECK_EQ(responses.result(), std::string("AB final"));

    StreamAccumulator claude;
    claude.feedLine(R"(data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"Hi"}})");
    CHECK_EQ(claude.result(), std::string("Hi"));

    StreamAccumulator failing("secret-key");
    CHECK(throwsError([&] { failing.feedLine(R"(data: {"type":"error","message":"bad secret-key"})"); }));
    try {
        failing.feedLine(R"(data: {"type":"error","message":"bad secret-key"})");
    } catch (const Error& error) {
        CHECK(!contains(error.what(), "secret-key"));
    }
    StreamAccumulator kieError;
    CHECK(throwsError([&] { kieError.feedLine(R"(data: {"code":401,"msg":"Unauthorized"})"); }));
}

static void testKieParsing() {
    CHECK(kie::parseCredits(json::parse(R"({"code":200,"data":1250.5})")) == std::optional<double>(1250.5));
    CHECK(kie::parseCredits(json::parse(R"({"data":"12"})")) == std::optional<double>(12));
    CHECK_EQ(kie::parseTaskId(json::parse(R"({"data":{"taskId":"t1"}})")), std::string("t1"));
    CHECK(throwsError([] { kie::parseTaskId(json::parse(R"({"data":{}})")); }));
    auto record = kie::parseRecord(json::parse(R"({"data":{"state":"success","progress":"100","resultJson":"{\"resultUrls\":[\"https://a/b.png\"]}"}})"), "");
    CHECK_EQ(record.state, std::string("success"));
    CHECK_EQ(record.progress, 99);
    CHECK(record.resultURLs == std::vector<std::string>({"https://a/b.png"}));
    auto generating = kie::parseRecord(json::parse(R"({"data":{"state":"generating","failMsg":"x key123"}})"), "key123");
    CHECK_EQ(generating.progress, 10);
    CHECK(generating.failMessage == std::optional<std::string>("x [oculto]"));
    CHECK(throwsError([] { kie::check(json::parse(R"({"code":401,"msg":"x"})"), 200, ""); }));
    try {
        kie::check(json::parse(R"({"code":402,"msg":"insufficient credits"})"), 200, "");
    } catch (const Error& error) {
        CHECK(contains(error.what(), "créditos"));
    }
    CHECK(!throwsError([] { kie::check(json::parse(R"({"code":200})"), 200, ""); }));
    CHECK_EQ(kie::parseUpload(json::parse(R"({"success":true,"data":{"downloadUrl":"https://u"}})"), 200), std::string("https://u"));
    CHECK(throwsError([] { kie::parseUpload(json::parse(R"({"success":false})"), 200); }));
    std::string body = kie::multipartUploadBody("B", "DATA", "image/png", "a\"b.png", "f.png");
    CHECK(contains(body, "filename=\"a'b.png\""));
    CHECK(contains(body, "images/framecraft"));
    CHECK(endsWith(body, "--B--\r\n"));
}

static void testMedia() {
    CHECK(media::matchesSignature(std::string("\x89PNG\r\n\x1A\n", 8), "image/png", ReferenceKind::image));
    CHECK(!media::matchesSignature(std::string(4, '\0'), "image/png", ReferenceKind::image));
    CHECK(media::matchesSignature(std::string("\0\0\0\x18" "ftypisom", 12), "video/mp4", ReferenceKind::video));
    CHECK_EQ(media::classifyExtension(".MOV")->mime, std::string("video/quicktime"));
    CHECK(!media::classifyExtension("gif").has_value());
    CHECK_EQ(media::formattedDuration(5), std::string("5.0 s"));
    CHECK_EQ(media::formattedDuration(12.4), std::string("12 s"));
}

// MARK: - Stories

static void testStories() {
    std::string raw = "```json\n{\"title\":\"Walter\",\"summary\":\"Un mozo de 82 años.\",\"continuity\":\"BIBLE\",\"referenceOrder\":\"Cargá @Image1 primero.\",\n"
                      " \"scenes\":[{\"number\":1,\"title\":\"Hook\",\"summary\":\"Presentación\",\"duration\":30,\"prompt\":\"BIBLE\\n0–5 s: …\",\"notes\":\"70 palabras\"},\n"
                      "           {\"number\":2,\"title\":\"La hija\",\"summary\":\"\",\"duration\":\"25 s\",\"prompt\":\"BIBLE 2\"},\n"
                      "           {\"number\":3,\"title\":\"Vacía\",\"prompt\":\"\"}]}\n```";
    auto draft = stories::parseStory(raw, 20);
    CHECK_EQ(draft.title, std::string("Walter"));
    CHECK_EQ(draft.scenes.size(), size_t(2));
    CHECK_EQ(draft.scenes[1].duration, 25);
    CHECK(draft.scenes[0].notes == std::optional<std::string>("70 palabras"));
    CHECK(throwsError([] { stories::parseStory("no json", 20); }));
    CHECK_EQ(stories::parseSingleScene("{\"title\":\"T\",\"summary\":\"S\",\"duration\":99,\"prompt\":\"P\"}", 10).duration, 30);

    {  // The bible is written once and placed before every scene, never twice.
        std::string bible = "FORMAT: Vertical 9:16.\n\nPACING — CRITICAL: dense dialogue.";
        std::string packed = "{\"title\":\"T\",\"continuity\":\"FORMAT: Vertical 9:16.\\n\\nPACING — CRITICAL: dense dialogue.\",\"scenes\":["
                             "{\"number\":1,\"duration\":30,\"prompt\":\"DURATION: 30 seconds.\"},"
                             "{\"number\":2,\"duration\":25,\"prompt\":\"FORMAT: Vertical 9:16.\\n\\nPACING — CRITICAL: dense dialogue.\\n\\nDURATION: 25 seconds.\"}]}";
        auto composed = stories::parseStory(packed, 20);
        CHECK_EQ(composed.continuity, bible);
        CHECK_EQ(composed.scenes[0].prompt, bible + "\n\nDURATION: 30 seconds.");
        CHECK_EQ(composed.scenes[1].prompt, bible + "\n\nDURATION: 25 seconds.");
        CHECK_EQ(stories::scenePart(bible, composed.scenes[0].prompt), std::string("DURATION: 30 seconds."));
        CHECK_EQ(stories::parseSingleScene("{\"prompt\":\"DURATION: 20 seconds.\"}", 20, bible).prompt, bible + "\n\nDURATION: 20 seconds.");
        CHECK(stories::storyInstructions().find("PACING — CRITICAL") != std::string::npos);
    }

    Story story;
    story.id = newUUID();
    story.title = "Walter";
    story.brief = "b";
    story.slots = {{newUUID(), ReferenceKind::image, "img", false, "tríptico"},
                   {newUUID(), ReferenceKind::audio, "aud", false, ""},
                   {newUUID(), ReferenceKind::image, std::nullopt, true, ""}};
    CHECK_EQ(story.tag(story.slots[2]), std::string("@Image2"));
    CHECK_EQ(story.tag(story.slots[1]), std::string("@Audio1"));
    auto lines = stories::referenceLines(story, {{"img", "walter.png"}, {"aud", "voz.mp3"}});
    CHECK_EQ(lines[0], std::string("@Image1 = walter.png — tríptico"));
    CHECK(startsWith(lines[2], "@Image2 = the last frame"));
    story.scenes = {{newUUID(), 1, "Hook", "s", 30, "PROMPT 1", std::nullopt, {}}};
    std::string text = stories::exportText(story, lines);
    CHECK(contains(text, "ESCENA 1 — Hook (30s)"));
    CHECK(contains(text, "PROMPT 1"));
    CHECK(contains(text, "WALTER — Seedance 2.5"));
    std::string storyBrief = stories::storyBrief(story, lines, std::nullopt, true, std::string("más corto"));
    CHECK(contains(storyBrief, "previousStory"));
    CHECK(contains(storyBrief, "más corto"));

    // JSON round trip, including old files without the newer fields.
    json encoded = story;
    encoded.erase("finalCutJobID");
    Story decoded = encoded.get<Story>();
    CHECK_EQ(decoded.title, std::string("Walter"));
    CHECK(!decoded.finalCutJobID.has_value());
    CHECK_EQ(decoded.slots.size(), size_t(3));
    CHECK(decoded.slots[2].isLastFrame);
}

static void testTemplates() {
    std::set<std::string> ids;
    bool image = false, video = false;
    for (const auto& t : templates::creative()) {
        ids.insert(t.id);
        CHECK(!t.placeholders().empty());
        if (t.media == MediaKind::video) {
            video = true;
            int duration = t.duration.value_or(0);
            CHECK(duration >= presets::kMinVideoDuration && duration <= presets::kMaxVideoDuration);
            const auto& aspects = presets::videoAspects();
            CHECK(std::find(aspects.begin(), aspects.end(), t.aspect) != aspects.end());
        } else {
            image = true;
        }
    }
    CHECK_EQ(ids.size(), templates::creative().size());
    CHECK(image && video);
    CHECK_EQ(templates::stories().size(), size_t(4));
    CHECK(templates::placeholders("Uso [tu producto] en [la cocina] y [x] no cuenta") ==
          std::vector<std::string>({"[tu producto]", "[la cocina]"}));
}

static void testLibraryRoundTrip() {
    fs::path folder = fs::temp_directory_path() / ("framecraft-test-" + newUUID());
    fs::path file = folder / "library.json";
    LibraryIndex index;
    Job job;
    job.id = newUUID();
    job.batchID = newUUID();
    job.kind = MediaKind::video;
    job.model = presets::kVideoModelID;
    job.prompt = "Chica con sérum “hola”";
    job.settings.resolution = "720p";
    job.settings.aspect = "9:16";
    job.settings.duration = 15;
    job.status = JobStatus::generating;
    job.progress = 46;
    job.taskId = "t";
    index.jobs.push_back(job);
    index.preferences.provider = PromptProvider::codex;
    index.preferences.theme = "dark";
    CHECK(library::save(file, index));
    LibraryIndex loaded = library::load(file);
    CHECK_EQ(loaded.jobs.size(), size_t(1));
    CHECK_EQ(loaded.jobs[0].prompt, job.prompt);
    CHECK(loaded.jobs[0].status == JobStatus::generating);
    CHECK(loaded.jobs[0].settings.duration == std::optional<int>(15));
    CHECK(loaded.preferences.provider == PromptProvider::codex);
    CHECK_EQ(loaded.preferences.theme, std::string("dark"));
    {
        std::ofstream broken(file, std::ios::trunc);
        broken << "{not json";
    }
    CHECK(library::load(file).jobs.empty());
    CHECK(fs::exists(fs::path(file).concat(".bak")));
    fs::remove_all(folder);
}

static void testUtil() {
    CHECK_EQ(lower("ÁRBOL Ñandú"), std::string("árbol ñandú"));
    CHECK_EQ(upper("walter, 82 años"), std::string("WALTER, 82 AÑOS"));
    CHECK_EQ(characterCount("añá"), size_t(3));
    CHECK_EQ(prefixCharacters("añá", 2), std::string("añ"));
    CHECK_EQ(wordCount("  uno dos\ntres  "), size_t(3));
    CHECK_EQ(newUUID().size(), size_t(36));
    CHECK_EQ(trim("  x \n"), std::string("x"));
}

int main(int argc, char** argv) {
    repoRoot = argc > 1 ? fs::path(argv[1]) : fs::current_path();
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"util", testUtil},         {"webParity", testWebParity}, {"skills", testSkills},
        {"linter", testLinter},     {"promptRequests", testPromptRequests}, {"streaming", testStreaming},
        {"kieParsing", testKieParsing}, {"media", testMedia},     {"stories", testStories},
        {"templates", testTemplates}, {"library", testLibraryRoundTrip},
    };
    for (const auto& [name, test] : tests) {
        currentTest = name;
        try {
            test();
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL [" << name << "] threw: " << error.what() << "\n";
        }
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
