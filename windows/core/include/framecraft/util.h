#pragma once
// Small string helpers shared by the Framecraft core (UTF-8 std::string everywhere).

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fc {

/// Error shown to the user. `definite` means the service clearly rejected the request
/// (nothing was created); otherwise it may have reached it (possible charge).
class Error : public std::runtime_error {
public:
    Error(const std::string& message, bool definite) : std::runtime_error(message), definite_(definite) {}
    bool definite() const { return definite_; }

private:
    bool definite_;
};

std::string trim(std::string_view text);
bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);
bool contains(std::string_view text, std::string_view needle);
std::string replaceAll(std::string text, std::string_view from, std::string_view to);
std::string join(const std::vector<std::string>& parts, std::string_view separator);
std::vector<std::string> splitLines(std::string_view text);

/// Lowercase for ASCII and the Latin-1 / Latin Extended-A letters used in Spanish and Portuguese.
std::string lower(std::string_view text);
std::string upper(std::string_view text);

/// Decodes one UTF-8 code point at `index` and advances it. Invalid bytes decode as U+FFFD.
char32_t decodeUtf8(std::string_view text, size_t& index);
void appendUtf8(std::string& out, char32_t codePoint);
/// Number of Unicode code points (what the user perceives as characters, roughly).
size_t characterCount(std::string_view text);
/// First `count` code points.
std::string prefixCharacters(std::string_view text, size_t count);

/// Letter or digit (Unicode-aware approximation of \p{L}\p{N}).
bool isWordCodePoint(char32_t c);
bool isLetterCodePoint(char32_t c);
bool isSpaceCodePoint(char32_t c);

/// Words separated by whitespace.
size_t wordCount(std::string_view text);

/// UTF-8 <-> filesystem paths (portable across Windows and POSIX).
std::filesystem::path pathFromUtf8(const std::string& text);
std::string utf8(const std::filesystem::path& path);

std::string newUUID();
int64_t nowMs();
std::string formatDouble(double value, int maxFractionDigits);

}  // namespace fc
