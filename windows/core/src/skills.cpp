#include "framecraft/skills.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "framecraft/util.h"

namespace fc {

namespace fs = std::filesystem;

SkillResources::SkillResources(fs::path root) : root_(std::move(root)) {}

std::optional<SkillResources> SkillResources::locate(const fs::path& executableDir) {
    std::vector<fs::path> candidates = {executableDir / "Skills"};
    // Development: walk up to the repository and use the macOS resources (single source of truth).
    fs::path probe = executableDir;
    for (int i = 0; i < 8 && !probe.empty(); ++i) {
        candidates.push_back(probe / "macos" / "Sources" / "FramecraftCore" / "Resources" / "Skills");
        if (probe == probe.parent_path()) break;
        probe = probe.parent_path();
    }
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (fs::exists(candidate / "ugc-celular" / "SKILL.md", error)) return SkillResources(candidate);
    }
    return std::nullopt;
}

fs::path SkillResources::path(const std::string& relative) const { return root_ / pathFromUtf8(relative); }

std::string SkillResources::text(const std::string& relative) const {
    std::ifstream stream(path(relative), std::ios::binary);
    if (!stream) return "";
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    std::string content = buffer.str();
    if (startsWith(content, "\xEF\xBB\xBF")) content.erase(0, 3);
    return content;
}

const std::string& SkillResources::jsonProfile() const {
    if (!jsonProfile_) {
        json value = json::parse(text("json-image/json-profile.json"), nullptr, false);
        jsonProfile_ = value.is_string() ? value.get<std::string>() : std::string();
    }
    return *jsonProfile_;
}

const std::vector<PromptExample>& SkillResources::examples() const {
    if (!examples_) {
        std::vector<PromptExample> list;
        json value = json::parse(text("json-image/examples.json"), nullptr, false);
        if (value.is_array()) {
            for (const auto& item : value) {
                if (!item.is_object()) continue;
                list.push_back({item.value("title", ""), item.value("prompt", ""), item.value("author", ""), item.value("source", "")});
            }
        }
        examples_ = std::move(list);
    }
    return *examples_;
}

namespace skills {

std::vector<Skill> builtins(const SkillResources* resources) {
    std::vector<Skill> list;
    list.push_back({kJsonProfileID, "Perfil JSON detallado (avanzado)",
                    "Devuelve un perfil técnico JSON (color, luz, composición…) inspirado en 123 prompts de GPT Image 2. Para control fino; si querés texto simple, usá General.",
                    "", SkillMedia::image, true, 0});
    if (!resources) return list;
    ParsedSkill ugc = parse(resources->text("ugc-celular/SKILL.md"), "UGC de celular");
    list.push_back({kUgcID, "UGC de celular · Seedance 2.5",
                    "Videos que parecen grabados con un teléfono real: causas físicas en vez de adjetivos, bloques de tiempo, diálogo con densidad medida y restricciones anti-anuncio.",
                    join({ugc.body,
                          "# Knowledge: parámetros, video largo y arco narrativo\n\n" + resources->text("ugc-celular/PARAMETROS-Y-ARCO.md"),
                          "# Knowledge: lecciones del pack de Walter (lo que funcionó)\n\n" + resources->text("ugc-celular/LECCIONES-WALTER.md"),
                          "# Regla de idioma\n\nEl prompt se escribe en el idioma en que se habla en el video (campo dialogueLanguage del brief). Mezclar idioma de instrucción con idioma de diálogo invita a que el modelo cruce los dos."},
                         "\n\n---\n\n"),
                    SkillMedia::video, true, 0});
    ParsedSkill arthas = parse(resources->text("arthas-cachito/SKILL.md"), "Arthas y Cachito");
    list.push_back({kArthasID, "Arthas y Cachito · Omni / Veo 3.1",
                    "Prompts cinematográficos de 10 s de la saga, para Gemini Omni 1.1 Flash o Veo 3.1 (copiá el prompt a esa herramienta).",
                    arthas.body + "\n\n---\n\n# Knowledge: elenco, lore y filtros\n\n" + resources->text("arthas-cachito/ELENCO-LORE-FILTROS.md"),
                    SkillMedia::video, true, 0});
    return list;
}

std::string walterExample(const SkillResources* resources) {
    return resources ? resources->text("ugc-celular/EJEMPLO-WALTER.txt") : "";
}

ParsedSkill parse(const std::string& markdown, const std::string& fallbackName) {
    std::string text = replaceAll(markdown, "\r\n", "\n");
    std::vector<std::string> lines = splitLines(text);
    size_t end = 0;
    bool frontMatter = !lines.empty() && trim(lines[0]) == "---";
    if (frontMatter) {
        end = 0;
        for (size_t i = 1; i < lines.size(); ++i) {
            if (trim(lines[i]) == "---") {
                end = i;
                break;
            }
        }
        frontMatter = end > 0;
    }
    if (!frontMatter) return {fallbackName, "", trim(text)};

    std::map<std::string, std::string> fields;
    std::string currentKey;
    for (size_t i = 1; i < end; ++i) {
        const std::string& line = lines[i];
        size_t colon = line.find(':');
        if (colon != std::string::npos && !startsWith(line, " ") && !startsWith(line, "\t")) {
            currentKey = lower(trim(line.substr(0, colon)));
            fields[currentKey] = trim(line.substr(colon + 1));
        } else if (!currentKey.empty()) {
            fields[currentKey] += " " + trim(line);  // folded continuation line
        }
    }
    auto clean = [](std::string value) {
        value = trim(value);
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1, value.size() - 2);
        return trim(value);
    };
    std::vector<std::string> bodyLines(lines.begin() + static_cast<long>(end) + 1, lines.end());
    std::string name = clean(fields["name"]);
    return {name.empty() ? fallbackName : name, clean(fields["description"]), trim(join(bodyLines, "\n"))};
}

SkillMedia guessMedia(const std::string& name, const std::string& summary, const std::string& body) {
    std::string text = lower(name + " " + summary + " " + prefixCharacters(body, 3000));
    int video = 0, image = 0;
    for (const char* word : {"video", "vídeo", "seedance", "clip", "veo", "omni", "escena", "timestamps"})
        if (contains(text, word)) ++video;
    for (const char* word : {"imagen", "image", "foto", "photo", "gpt image", "nano banana"})
        if (contains(text, word)) ++image;
    if (video > image) return SkillMedia::video;
    if (image > video) return SkillMedia::image;
    return SkillMedia::any;
}

std::vector<PromptExample> relevantExamples(const std::string& idea, const std::vector<PromptExample>& examples) {
    static const std::set<std::string> stop = {"the", "and", "with", "for", "image", "photo", "make", "create"};
    std::vector<std::string> words;
    std::set<std::string> seenWords;
    std::string lowered = lower(idea);
    // Runs of 3+ letters/digits ([\p{L}\p{N}]{3,}).
    std::string current;
    size_t letters = 0;
    auto flush = [&] {
        if (letters >= 3 && !stop.count(current) && seenWords.insert(current).second) words.push_back(current);
        current.clear();
        letters = 0;
    };
    for (size_t i = 0; i < lowered.size();) {
        size_t start = i;
        char32_t c = decodeUtf8(lowered, i);
        if (isWordCodePoint(c)) {
            current.append(lowered, start, i - start);
            ++letters;
        } else {
            flush();
        }
    }
    flush();

    struct Scored {
        int score;
        size_t index;
        const PromptExample* example;
    };
    std::set<std::string> seenTitles;
    std::vector<Scored> scored;
    size_t index = 0;
    for (const auto& example : examples) {
        if (!seenTitles.insert(example.title).second) continue;
        std::string title = lower(example.title);
        std::string prompt = lower(example.prompt);
        int score = 0;
        for (const auto& word : words) score += (contains(title, word) ? 5 : 0) + (contains(prompt, word) ? 1 : 0);
        if (score > 0) scored.push_back({score, index, &example});
        ++index;
    }
    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        return a.score != b.score ? a.score > b.score : a.index < b.index;
    });
    std::vector<PromptExample> result;
    for (size_t i = 0; i < scored.size() && i < 3; ++i) {
        const auto& e = *scored[i].example;
        result.push_back({e.title, prefixCharacters(e.prompt, 6500), e.author, e.source});
    }
    return result;
}

json jsonProfileContext(const std::string& idea, const SkillResources& resources, std::vector<PromptExample>* chosen) {
    std::vector<PromptExample> examples = relevantExamples(idea, resources.examples());
    json list = json::array();
    for (const auto& e : examples)
        list.push_back({{"title", e.title}, {"prompt", e.prompt}, {"author", e.author}, {"source", e.source}});
    if (chosen) *chosen = examples;
    return {{"profile", resources.jsonProfile()},
            {"examples", list},
            {"attribution", kExampleAttribution},
            {"source", "https://github.com/YouMind-OpenLab/awesome-gpt-image-2"},
            {"license", "https://creativecommons.org/licenses/by/4.0/"}};
}

}  // namespace skills
}  // namespace fc
