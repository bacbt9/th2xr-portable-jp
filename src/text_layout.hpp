#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace th2 {

// Ruby (furigana) is carried inside visible message text using the Unicode
// interlinear annotation characters:
//   ruby_anchor base ruby_separator reading ruby_terminator
// This keeps byte offsets into the visible text stable for the reveal
// animation, backlog voice ranges and saves.
inline constexpr std::string_view ruby_anchor = "\xEF\xBF\xB9";
inline constexpr std::string_view ruby_separator = "\xEF\xBF\xBA";
inline constexpr std::string_view ruby_terminator = "\xEF\xBF\xBB";

struct RubyGroup {
    std::size_t base_begin = 0;  // byte offsets into the parsed text
    std::size_t base_end = 0;
    std::size_t end = 0;         // one past the terminator
    std::string_view reading;
};

// Parse the ruby group whose anchor starts at `anchor`. A missing terminator
// ends the group at the end of `text`.
RubyGroup parse_ruby_group(std::string_view text, std::size_t anchor);

// Byte length of the UTF-8 sequence starting at text[position].
std::size_t utf8_glyph_bytes(std::string_view text, std::size_t position);

// Width in half-width cells, matching the original renderer: ASCII and
// half-width katakana advance fno/2, everything else a full fno.
int glyph_cells(std::string_view glyph);

// Characters the original TXT_DrawTextEx refuses to put at the start of a
// line (句読点, closing brackets, gaiji punctuation, ...).
bool is_line_start_forbidden(std::string_view glyph);

// Split visible message text into display lines the way the original
// TXT_DrawTextEx does: a line breaks once it is within one full-width glyph of
// `wrap_cells` half-width cells, one forbidden glyph may hang past the edge,
// ruby groups never split, and an explicit newline right after an automatic
// wrap is swallowed. Every returned line is a substring of `source`.
std::vector<std::string> wrap_display_lines(
    std::string_view source, std::size_t wrap_cells);

// Remove ruby readings and markers, leaving only the base text.
std::string strip_ruby(std::string_view text);

// Number of glyphs the reveal animation steps through (ruby readings and
// markers do not count).
std::size_t revealed_glyph_count(std::string_view text);

// Save slot excerpt: the original copies the first 18 Shift_JIS bytes of the
// first displayed line (9 full-width glyphs).
std::string save_excerpt(std::string_view visible_text);

}  // namespace th2
