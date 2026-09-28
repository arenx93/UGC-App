#include "framecraft/linter.h"

#include <algorithm>
#include <cmath>
#include <regex>

#include "framecraft/models.h"
#include "framecraft/presets.h"
#include "framecraft/util.h"

namespace fc {

int LintReport::count(LintItem::State state) const {
    return static_cast<int>(std::count_if(items.begin(), items.end(), [&](const LintItem& i) { return i.state == state; }));
}

const LintItem* LintReport::item(const std::string& id) const {
    for (const auto& i : items)
        if (i.id == id) return &i;
    return nullptr;
}

namespace linter {

namespace {

const char* const kOpenQuotes[] = {"\"", "\xE2\x80\x9C"};    // " “
const char* const kCloseQuotes[] = {"\"", "\xE2\x80\x9D"};   // " ”

/// Length of the quote at `index` from `set`, or 0.
size_t quoteAt(const std::string& text, size_t index, const char* const (&set)[2]) {
    for (const char* quote : set) {
        size_t length = std::char_traits<char>::length(quote);
        if (text.compare(index, length, quote) == 0) return length;
    }
    return 0;
}

bool anyContains(const std::string& text, std::initializer_list<const char*> needles) {
    for (const char* needle : needles)
        if (contains(text, needle)) return true;
    return false;
}

const std::vector<std::string>& negations() {
    static const std::vector<std::string> list = {"no ", "not ", "never ", "without ", "avoid", "sin ", "nunca ", "ni ",
                                                  "evitar", "prohibid", "zero ", "none"};
    return list;
}

}  // namespace

const std::vector<std::string>& adWords() {
    static const std::vector<std::string> list = {
        "cinematic", "shallow depth of field", "bokeh", "gimbal", "steadicam", "dolly", "slider", "crane shot",
        "drone", "orbit", "rack focus", "teal and orange", "film grain", "lut", "halation", "anamorphic",
        "beauty filter", "color grade", "studio lighting", "rim light", "slow motion",
        "cinematográfico", "cinematográfica", "cámara lenta",
    };
    return list;
}

int dialogueWords(const std::string& text) {
    int total = 0;
    size_t i = 0;
    while (i < text.size()) {
        size_t open = quoteAt(text, i, kOpenQuotes);
        if (!open) {
            ++i;
            continue;
        }
        size_t start = i + open;
        size_t j = start;
        size_t characters = 0;
        size_t closeLength = 0;
        while (j < text.size()) {
            closeLength = quoteAt(text, j, kCloseQuotes);
            if (closeLength) break;
            decodeUtf8(text, j);
            ++characters;
        }
        if (!closeLength) break;  // no closing quote: nothing more to match
        if (characters >= 1 && characters <= 2000) {
            total += static_cast<int>(wordCount(std::string_view(text).substr(start, j - start)));
            i = j + closeLength;
        } else {
            i = start;  // like a regex: retry from the next position
        }
    }
    return total;
}

int highestTag(const std::string& prefix, const std::string& text) {
    std::string haystack = lower(text);
    std::string needle = lower(prefix);
    int highest = 0;
    size_t position = 0;
    while ((position = haystack.find(needle, position)) != std::string::npos) {
        size_t k = position + needle.size();
        int value = 0;
        bool digits = false;
        while (k < haystack.size() && haystack[k] >= '0' && haystack[k] <= '9') {
            digits = true;
            value = std::min(value * 10 + (haystack[k] - '0'), 1000000);
            ++k;
        }
        if (digits) highest = std::max(highest, value);
        position += needle.size();
    }
    return highest;
}

bool hasTimestamps(const std::string& text) {
    static const std::string dash = "(?:\xE2\x80\x93|\xE2\x80\x94|-)";  // – — -
    static const std::string unit = "(?:s|sec|secs|seconds|seg|segundos)\\b";
    static const std::regex ranges("\\b\\d{1,2}(?:[.,]\\d)?\\s*(?:" + unit + ")?\\s*" + dash + "\\s*\\d{1,2}(?:[.,]\\d)?\\s*" + unit,
                                   std::regex::icase | std::regex::ECMAScript);
    static const std::regex clock("\\b\\d{1,2}:\\d{2}\\s*" + dash + "\\s*\\d{1,2}:\\d{2}\\b", std::regex::ECMAScript);
    for (const auto& line : splitLines(text)) {
        if (line.size() > 4000) continue;
        if (std::regex_search(line, ranges) || std::regex_search(line, clock)) return true;
    }
    return false;
}

bool hasDirectionBeforeLine(const std::string& text) {
    size_t position = 0;
    while ((position = text.find('(', position)) != std::string::npos) {
        size_t close = text.find(')', position + 1);
        if (close == std::string::npos) return false;
        size_t inner = characterCount(std::string_view(text).substr(position + 1, close - position - 1));
        if (inner >= 3) {
            size_t k = close + 1;
            auto skipSpaces = [&] {
                while (k < text.size() && (text[k] == ' ' || text[k] == '\t' || text[k] == '\n' || text[k] == '\r')) ++k;
            };
            skipSpaces();
            if (k < text.size() && text[k] == ':') {
                ++k;
                skipSpaces();
                if (quoteAt(text, k, kOpenQuotes)) return true;
            }
        }
        position += 1;
    }
    return false;
}

bool containsUnnegated(const std::string& lowered, const std::string& term) {
    size_t position = 0;
    while ((position = lowered.find(term, position)) != std::string::npos) {
        // Letter boundaries (like (?<![\p{L}]) … (?![\p{L}])).
        bool letterBefore = false;
        if (position > 0) {
            size_t back = position - 1;
            while (back > 0 && (static_cast<unsigned char>(lowered[back]) & 0xC0) == 0x80) --back;
            size_t probe = back;
            letterBefore = isLetterCodePoint(decodeUtf8(lowered, probe));
        }
        size_t after = position + term.size();
        bool letterAfter = false;
        if (after < lowered.size()) {
            size_t probe = after;
            letterAfter = isLetterCodePoint(decodeUtf8(lowered, probe));
        }
        if (!letterBefore && !letterAfter) {
            // Look back to the start of the clause (up to 80 characters).
            size_t start = position;
            for (int steps = 0; steps < 80 && start > 0; ++steps) {
                --start;
                while (start > 0 && (static_cast<unsigned char>(lowered[start]) & 0xC0) == 0x80) --start;
            }
            std::string window = lowered.substr(start, position - start);
            size_t cut = window.find_last_of(".;\n");
            if (cut != std::string::npos) window = window.substr(cut + 1);
            std::string padded = " " + window;
            bool negated = std::any_of(negations().begin(), negations().end(),
                                       [&](const std::string& negation) { return contains(padded, " " + negation); });
            if (!negated) return true;
        }
        position += 1;
    }
    return false;
}

LintReport lintVideo(const std::string& text, int duration, ReferenceCounts references, bool ugcStyle) {
    using S = LintItem::State;
    std::string lowered = lower(text);
    LintReport report;
    auto& items = report.items;
    int dialogueCount = dialogueWords(text);
    int target = dialogue::targetWords(duration);
    std::string seconds = std::to_string(duration);

    // 1. References @ImageN / @VideoN / @AudioN vs. the files actually selected.
    std::vector<std::string> referenceProblems;
    std::vector<std::string> unusedKinds;
    const std::pair<ReferenceKind, int> usage[] = {
        {ReferenceKind::image, references.images}, {ReferenceKind::video, references.videos}, {ReferenceKind::audio, references.audios}};
    for (const auto& [kind, selected] : usage) {
        int highest = highestTag(tagPrefix(kind), text);
        if (highest > selected) {
            referenceProblems.push_back("El prompt usa " + std::string(tagPrefix(kind)) + std::to_string(highest) +
                                        " pero seleccionaste " + std::to_string(selected) + " " + pluralNoun(kind) + ".");
        } else if (selected > 0 && highest == 0) {
            unusedKinds.push_back(tagPrefix(kind));
        }
    }
    if (!referenceProblems.empty()) {
        items.push_back({"refs", S::problem, "Referencias que no existen",
                         join(referenceProblems, " ") + " El número sigue el orden de la lista de referencias."});
    } else if (!unusedKinds.empty()) {
        items.push_back({"refs", S::info, "Referencias sin nombrar",
                         "Seleccionaste archivos que el prompt no menciona (" + join(unusedKinds, ", ") +
                             "…). Nombralos con su etiqueta para decirle al modelo para qué sirve cada uno."});
    } else if (references.images + references.videos + references.audios > 0) {
        items.push_back({"refs", S::ok, "Referencias en orden", "Cada etiqueta @ apunta a un archivo seleccionado."});
    }

    // 2. Time blocks (Seedance/UGC only: in Omni/Veo they force cuts).
    if (ugcStyle) {
        if (hasTimestamps(text)) {
            items.push_back({"timeline", S::ok, "Bloques de tiempo", "La acción está segmentada con timestamps."});
        } else {
            items.push_back({"timeline", S::warning, "Faltan bloques de tiempo",
                             "Segmentá la acción (por ejemplo 0–6 s, 6–12 s…). Sin eso el modelo adivina el timeline y aparecen saltos y manos rotas."});
        }
    }

    // 3. Dialogue density (~2.47 words per second).
    if (dialogueCount == 0) {
        items.push_back({"dialogue", S::info, "Sin diálogo entre comillas",
                         "Si alguien habla, poné sus líneas \"entre comillas\": eso activa el lip-sync. Para " + seconds +
                             " s son unas " + std::to_string(target) + " palabras."});
    } else {
        double ratio = static_cast<double>(dialogueCount) / std::max(target, 1);
        std::string counts = std::to_string(dialogueCount) + " de ~" + std::to_string(target) + " palabras";
        if (ratio < 0.75) {
            int silence = std::max(0, static_cast<int>(std::lround(duration - dialogue::secondsForWords(dialogueCount))));
            items.push_back({"dialogue", S::warning, "Poco diálogo: " + counts,
                             "El modelo rellena con silencio el tiempo que sobra (≈" + std::to_string(silence) +
                                 " s de aire). Sumá diálogo o acortá el clip."});
        } else if (ratio > 1.25) {
            items.push_back({"dialogue", S::warning, "Mucho diálogo: " + counts,
                             "No entra en " + seconds + " s a 2,47 palabras por segundo: se va a atropellar. Recortá o alargá el clip."});
        } else {
            items.push_back({"dialogue", S::ok, "Densidad de diálogo: " + std::to_string(dialogueCount) + " palabras",
                             "Bien para " + seconds + " s (ideal ≈ " + std::to_string(target) + ")."});
        }
    }

    // 4. Acting direction in parentheses must be declared as never spoken.
    if (hasDirectionBeforeLine(text)) {
        bool declared = anyContains(lowered, {"never spoken", "not spoken", "never said", "no se pronuncia", "no se dice", "not read aloud"});
        items.push_back(declared ? LintItem{"parentheses", S::ok, "Acotaciones protegidas",
                                            "Está aclarado que el texto entre paréntesis no se pronuncia."}
                                 : LintItem{"parentheses", S::warning, "Aclarar que el paréntesis no se dice",
                                            "Agregá: \"Text in parentheses before a spoken line is acting direction only. It is NEVER spoken aloud.\" Si no, el modelo puede leer la acotación en voz alta."});
    }

    // 5. Reference audio = timbre only.
    if (highestTag("@Audio", text) > 0) {
        bool timbre = contains(lowered, "timbre");
        bool noWords = anyContains(lowered, {"do not reproduce", "don't reproduce", "no repitas", "no reproduzcas", "not reproduce any words"});
        items.push_back(timbre && noWords ? LintItem{"audio", S::ok, "Audio de referencia = solo timbre",
                                                     "Bloqueadas las palabras y la emoción del sample."}
                                          : LintItem{"audio", S::warning, "Bloqueá el audio de referencia",
                                                     "Declaralo como solo timbre: \"match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1 and do not copy its emotion\"."});
    }

    if (ugcStyle) {
        // 6. Words that push the model into "ad" mode.
        std::vector<std::string> found;
        for (const auto& word : adWords())
            if (containsUnnegated(lowered, word)) found.push_back(word);
        if (found.empty()) {
            items.push_back({"adwords", S::ok, "Sin lenguaje de anuncio", "No hay términos cinematográficos que arruinen el look de celular."});
        } else {
            std::vector<std::string> shown(found.begin(), found.begin() + std::min<size_t>(4, found.size()));
            items.push_back({"adwords", S::warning, "Lenguaje de anuncio: " + join(shown, ", "),
                             "Estos términos empujan al modelo a verse como publicidad. Pedí causas físicas (\"el operador reencuadra tarde\") en vez de estilos."});
        }

        // 7. Restrictions.
        const std::pair<const char*, std::initializer_list<const char*>> restrictions[] = {
            {"sin música", {"no music", "without music", "sin música", "sin musica", "no background music"}},
            {"sin cortes", {"no cuts", "no cut", "sin cortes", "continuous take", "continuous shot", "one take", "jump cut", "un solo plano"}},
            {"sin texto", {"no text", "no on-screen text", "no subtitles", "no captions", "sin texto", "sin subtítulos", "no overlays", "no graphics"}},
        };
        std::vector<std::string> missing;
        for (const auto& [name, needles] : restrictions)
            if (!anyContains(lowered, needles)) missing.push_back(name);
        if (missing.empty()) {
            items.push_back({"restrictions", S::ok, "Restricciones completas", "Música, cortes y texto están controlados."});
        } else {
            items.push_back({"restrictions", S::warning, "Faltan restricciones: " + join(missing, ", "),
                             "Cerrá el prompt con restricciones en lenguaje natural. No existe un campo de negative prompt."});
        }

        // 8. Deep depth of field.
        bool deep = anyContains(lowered, {"deep depth of field", "wide depth of field", "large depth of field", "everything in focus",
                                          "profundidad de campo amplia", "todo en foco", "background stays sharp",
                                          "background fully readable", "fondo se lee"});
        items.push_back(deep ? LintItem{"dof", S::ok, "Profundidad de campo amplia", "El fondo se lee entero, como en un celular."}
                             : LintItem{"dof", S::info, "Declarar profundidad de campo amplia",
                                        "Un celular tiene todo en foco. Decilo explícito (\"deep depth of field, the background is fully readable\")."});
    }

    size_t length = characterCount(text);
    if (length > static_cast<size_t>(presets::kMaxVideoPromptLength)) {
        items.push_back({"length", S::problem, "Prompt demasiado largo",
                         "Tiene " + std::to_string(length) + " caracteres; el máximo es " +
                             std::to_string(presets::kMaxVideoPromptLength) + "."});
    }
    report.dialogueWords = dialogueCount;
    report.targetWords = target;
    report.promptWords = static_cast<int>(wordCount(text));
    return report;
}

}  // namespace linter
}  // namespace fc
