#include "framecraft/util.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

namespace fc {

std::string trim(std::string_view text) {
    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
    size_t start = 0, end = text.size();
    while (start < end && isSpace(static_cast<unsigned char>(text[start]))) ++start;
    while (end > start && isSpace(static_cast<unsigned char>(text[end - 1]))) --end;
    // Also strip non-breaking spaces (U+00A0) at the edges.
    std::string result(text.substr(start, end - start));
    while (startsWith(result, "\xC2\xA0")) result.erase(0, 2);
    while (endsWith(result, "\xC2\xA0")) result.erase(result.size() - 2);
    return result;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(std::string_view text, std::string_view needle) { return text.find(needle) != std::string_view::npos; }

std::string replaceAll(std::string text, std::string_view from, std::string_view to) {
    if (from.empty()) return text;
    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
    return text;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += separator;
        out += parts[i];
    }
    return out;
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            std::string line(text.substr(start, i - start));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            start = i + 1;
        }
    }
    return lines;
}

char32_t decodeUtf8(std::string_view text, size_t& index) {
    auto byte = [&](size_t i) { return static_cast<unsigned char>(text[i]); };
    unsigned char c = byte(index);
    if (c < 0x80) {
        ++index;
        return c;
    }
    int length = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (length == 0 || index + length > text.size()) {
        ++index;
        return 0xFFFD;
    }
    char32_t value = length == 2 ? (c & 0x1F) : length == 3 ? (c & 0x0F) : (c & 0x07);
    for (int k = 1; k < length; ++k) {
        unsigned char next = byte(index + k);
        if ((next & 0xC0) != 0x80) {
            ++index;
            return 0xFFFD;
        }
        value = (value << 6) | (next & 0x3F);
    }
    index += length;
    return value;
}

void appendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

namespace {
char32_t lowerCodePoint(char32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if ((c >= 0xC0 && c <= 0xDE) && c != 0xD7) return c + 32;             // À-Þ (not ×)
    if (c >= 0x100 && c <= 0x17F && c % 2 == 0 && c != 0x130) return c + 1;  // Latin Extended-A pairs (approx.)
    if (c >= 0x391 && c <= 0x3A9) return c + 32;                            // Greek capitals
    if (c >= 0x410 && c <= 0x42F) return c + 32;                            // Cyrillic capitals
    return c;
}

char32_t upperCodePoint(char32_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if ((c >= 0xE0 && c <= 0xFE) && c != 0xF7) return c - 32;
    if (c >= 0x100 && c <= 0x17F && c % 2 == 1 && c != 0x131) return c - 1;
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    return c;
}
}  // namespace

std::string lower(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            out += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
            ++i;
        } else {
            appendUtf8(out, lowerCodePoint(decodeUtf8(text, i)));
        }
    }
    return out;
}

std::string upper(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            out += static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
            ++i;
        } else {
            appendUtf8(out, upperCodePoint(decodeUtf8(text, i)));
        }
    }
    return out;
}

size_t characterCount(std::string_view text) {
    size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

std::string prefixCharacters(std::string_view text, size_t count) {
    size_t seen = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            if (seen == count) return std::string(text.substr(0, i));
            ++seen;
        }
    }
    return std::string(text);
}

bool isLetterCodePoint(char32_t c) {
    if (c < 0x80) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (c < 0xC0) return c == 0xAA || c == 0xB5 || c == 0xBA;  // Latin-1 punctuation and symbols are not letters
    if (c == 0xD7 || c == 0xF7) return false;                   // × ÷
    if (c >= 0x2000 && c <= 0x2BFF) return false;               // punctuation, symbols, arrows, math, box drawing
    if (c >= 0x3000 && c <= 0x303F) return false;               // CJK punctuation
    if (c >= 0xFE00 && c <= 0xFE6F) return false;
    if (c >= 0xFF00 && c <= 0xFF20) return false;
    if (c >= 0x1F000) return false;                             // emoji
    if (c == 0xFFFD) return false;
    return true;
}

bool isWordCodePoint(char32_t c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 0x660 && c <= 0x669) return true;
    return isLetterCodePoint(c);
}

bool isSpaceCodePoint(char32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == 0xA0 || c == 0x2028 ||
           c == 0x2029 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000 || c == 0x85;
}

size_t wordCount(std::string_view text) {
    size_t count = 0;
    bool inWord = false;
    for (size_t i = 0; i < text.size();) {
        char32_t c = decodeUtf8(text, i);
        if (isSpaceCodePoint(c)) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++count;
        }
    }
    return count;
}

std::filesystem::path pathFromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string utf8(const std::filesystem::path& path) {
    std::u8string value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::string newUUID() {
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(engine), b = dist(engine);
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // version 4
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;  // variant
    char buffer[37];
    std::snprintf(buffer, sizeof buffer, "%08X-%04X-%04X-%04X-%012llX", static_cast<unsigned>(a >> 32),
                  static_cast<unsigned>((a >> 16) & 0xFFFF), static_cast<unsigned>(a & 0xFFFF),
                  static_cast<unsigned>(b >> 48), static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    return buffer;
}

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string formatDouble(double value, int maxFractionDigits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", maxFractionDigits, value);
    std::string text = buffer;
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    return text;
}

}  // namespace fc
