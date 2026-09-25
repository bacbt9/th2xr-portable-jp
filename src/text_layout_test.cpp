#include "text_layout.hpp"

#include <string>

namespace {

std::string repeat(std::string_view glyph, int count)
{
    std::string result;
    for (int i = 0; i < count; ++i) {
        result += glyph;
    }
    return result;
}

std::string ruby(std::string_view base, std::string_view reading)
{
    return std::string(th2::ruby_anchor) + std::string(base)
        + std::string(th2::ruby_separator) + std::string(reading)
        + std::string(th2::ruby_terminator);
}

}  // namespace

int main()
{
    constexpr std::size_t cells = 60;
    const auto thirty = repeat("あ", 30);

    // 30 full-width glyphs fill a line exactly.
    auto lines = th2::wrap_display_lines(thirty, cells);
    if (lines.size() != 1 || lines[0] != thirty) {
        return 1;
    }
    lines = th2::wrap_display_lines(thirty + "い", cells);
    if (lines.size() != 2 || lines[0] != thirty || lines[1] != "い") {
        return 2;
    }

    // One forbidden glyph hangs past the edge; the next one wraps.
    lines = th2::wrap_display_lines(thirty + "。」い", cells);
    if (lines.size() != 2 || lines[0] != thirty + "。"
        || lines[1] != "」い") {
        return 3;
    }

    // Ideographic spaces keep hanging.
    lines = th2::wrap_display_lines(thirty + "、　　い", cells);
    if (lines.size() != 2 || lines[0] != thirty + "、　　"
        || lines[1] != "い") {
        return 4;
    }

    // Gaiji punctuation (U+E000) is also forbidden at line start.
    lines = th2::wrap_display_lines(thirty + "\xEE\x80\x80", cells);
    if (lines.size() != 1) {
        return 5;
    }

    // A newline right after an automatic wrap is swallowed.
    lines = th2::wrap_display_lines(thirty + "\nい\n\nう", cells);
    if (lines.size() != 4 || lines[1] != "い" || !lines[2].empty()
        || lines[3] != "う") {
        return 6;
    }

    // Half-width glyphs are one cell; the line breaks within a full glyph
    // of the edge, so 59 cells is the limit.
    lines = th2::wrap_display_lines(repeat("a", 60), cells);
    if (lines.size() != 2 || lines[0].size() != 59 || lines[1] != "a") {
        return 7;
    }
    if (th2::glyph_cells("\xEF\xBD\xB1") != 1  // ｱ
        || th2::glyph_cells("ア") != 2 || th2::glyph_cells("a") != 1) {
        return 8;
    }

    // Ruby readings take no width and a ruby group is never split.
    const auto group = ruby("小牧", "こまき");
    lines = th2::wrap_display_lines(repeat("あ", 29) + group + "い", cells);
    if (lines.size() != 2 || lines[0] != repeat("あ", 29) + group
        || lines[1] != "い") {
        return 9;
    }
    if (th2::strip_ruby(group + "さん") != "小牧さん"
        || th2::revealed_glyph_count(group + "さん") != 4) {
        return 10;
    }

    // Save excerpt: 18 Shift_JIS bytes of the first line.
    if (th2::save_excerpt("あいうえおかきくけこ") != "あいうえおかきくけ"
        || th2::save_excerpt("ab\ncd") != "ab"
        || th2::save_excerpt("aあいうえおかきくけ") != "aあいうえおかきく"
        || th2::save_excerpt(group + "さん") != "小牧さん") {
        return 11;
    }
    return 0;
}
