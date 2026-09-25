#include "text_layout.hpp"

#include <algorithm>
#include <array>

namespace th2 {
namespace {

constexpr std::string_view ideographic_space = "\xE3\x80\x80";  // U+3000

// Order follows the list in the original my_inc2/text.cpp. The final eight
// entries are the gaiji 0xF040-0xF047, which CP932 maps to U+E000-U+E007.
constexpr std::array<std::string_view, 25> line_start_forbidden{
    "\xE3\x80\x82",  // 。
    "\xE3\x80\x81",  // 、
    "\xEF\xBC\x8C",  // ，
    "\xEF\xBC\x8E",  // ．
    "\xE3\x83\xBB",  // ・
    "\xE2\x80\xA6",  // …
    "\xE3\x83\xBC",  // ー
    "\xEF\xBC\x9A",  // ：
    "\xEF\xBC\x9B",  // ；
    "\xEF\xBC\x9F",  // ？
    "\xEF\xBC\x81",  // ！
    "\xEF\xBC\xBD",  // ］
    "\xE2\x80\x9D",  // ”
    ideographic_space,
    "\xEF\xBC\x89",  // ）
    "\xE3\x80\x8D",  // 」
    "\xE3\x80\x8F",  // 』
    "\xEE\x80\x80", "\xEE\x80\x81", "\xEE\x80\x82", "\xEE\x80\x83",
    "\xEE\x80\x84", "\xEE\x80\x85", "\xEE\x80\x86", "\xEE\x80\x87",
};

std::string_view glyph_at(std::string_view text, std::size_t position)
{
    if (position >= text.size()) {
        return {};
    }
    return text.substr(position, utf8_glyph_bytes(text, position));
}

}  // namespace

RubyGroup parse_ruby_group(std::string_view text, std::size_t anchor)
{
    RubyGroup group;
    group.base_begin = anchor + ruby_anchor.size();
    auto terminator = text.find(ruby_terminator, group.base_begin);
    group.end = terminator == std::string_view::npos
        ? text.size()
        : terminator + ruby_terminator.size();
    if (terminator == std::string_view::npos) {
        terminator = text.size();
    }
    const auto separator = text.find(ruby_separator, group.base_begin);
    if (separator == std::string_view::npos || separator > terminator) {
        group.base_end = terminator;
    } else {
        group.base_end = separator;
        const auto reading_begin = separator + ruby_separator.size();
        group.reading =
            text.substr(reading_begin, terminator - reading_begin);
    }
    return group;
}

std::size_t utf8_glyph_bytes(std::string_view text, std::size_t position)
{
    const auto lead = static_cast<unsigned char>(text[position]);
    const std::size_t length = lead < 0x80 ? 1
        : lead < 0xe0 ? 2
        : lead < 0xf0 ? 3
        : 4;
    return std::min(length, text.size() - position);
}

int glyph_cells(std::string_view glyph)
{
    if (glyph.empty()) {
        return 0;
    }
    const auto lead = static_cast<unsigned char>(glyph[0]);
    if (lead < 0x80) {
        return 1;
    }
    if (glyph.size() == 3 && lead == 0xef) {
        // Half-width katakana U+FF61-U+FF9F.
        const auto second = static_cast<unsigned char>(glyph[1]);
        const auto third = static_cast<unsigned char>(glyph[2]);
        if ((second == 0xbd && third >= 0xa1)
            || (second == 0xbe && third <= 0x9f)) {
            return 1;
        }
    }
    return 2;
}

bool is_line_start_forbidden(std::string_view glyph)
{
    for (const auto candidate : line_start_forbidden) {
        if (glyph == candidate) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> wrap_display_lines(
    std::string_view source, std::size_t wrap_cells)
{
    std::vector<std::string> lines;
    std::string line;
    std::size_t cells = 0;
    bool hanging = false;
    bool just_wrapped = false;
    for (std::size_t position = 0; position < source.size();) {
        if (source[position] == '\n') {
            // The original ignores a newline that immediately follows an
            // automatic wrap (kaig).
            if (!just_wrapped) {
                lines.push_back(line);
            }
            line.clear();
            cells = 0;
            hanging = false;
            just_wrapped = false;
            ++position;
            continue;
        }
        if (source.substr(position).starts_with(ruby_anchor)) {
            // A ruby group is laid out as its base text and never broken.
            const auto ruby = parse_ruby_group(source, position);
            const auto base = source.substr(
                ruby.base_begin, ruby.base_end - ruby.base_begin);
            for (std::size_t i = 0; i < base.size();) {
                const auto glyph = glyph_at(base, i);
                cells += static_cast<std::size_t>(glyph_cells(glyph));
                i += glyph.size();
            }
            const auto end = ruby.end;
            const auto group = source.substr(position, end - position);
            line.append(group);
            position = end;
        } else {
            const auto glyph = glyph_at(source, position);
            cells += static_cast<std::size_t>(glyph_cells(glyph));
            line.append(glyph);
            position += glyph.size();
        }
        just_wrapped = false;

        // px - sx >= w * font - fno + 1, expressed in half-width cells.
        if (cells + 1 < wrap_cells) {
            continue;
        }
        const auto next = glyph_at(source, position);
        bool wrap = false;
        if (hanging) {
            if (next != ideographic_space) {
                hanging = false;
                wrap = true;
            }
        } else if (is_line_start_forbidden(next)) {
            hanging = true;
        } else {
            wrap = true;
        }
        if (wrap) {
            lines.push_back(line);
            line.clear();
            cells = 0;
            just_wrapped = true;
        }
    }
    if (!line.empty() || lines.empty()) {
        lines.push_back(line);
    }
    return lines;
}

std::string strip_ruby(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool in_reading = false;
    for (std::size_t position = 0; position < text.size();) {
        const auto rest = text.substr(position);
        if (rest.starts_with(ruby_anchor)) {
            position += ruby_anchor.size();
        } else if (rest.starts_with(ruby_separator)) {
            in_reading = true;
            position += ruby_separator.size();
        } else if (rest.starts_with(ruby_terminator)) {
            in_reading = false;
            position += ruby_terminator.size();
        } else {
            const auto length = utf8_glyph_bytes(text, position);
            if (!in_reading) {
                result.append(text.substr(position, length));
            }
            position += length;
        }
    }
    return result;
}

std::size_t revealed_glyph_count(std::string_view text)
{
    const auto plain = strip_ruby(text);
    std::size_t count = 0;
    for (std::size_t position = 0; position < plain.size();
         position += utf8_glyph_bytes(plain, position)) {
        ++count;
    }
    return count;
}

std::string save_excerpt(std::string_view visible_text)
{
    const auto plain = strip_ruby(visible_text);
    std::string result;
    std::size_t sjis_bytes = 0;
    for (std::size_t position = 0; position < plain.size();) {
        if (plain[position] == '\n') {
            break;
        }
        const auto glyph = glyph_at(plain, position);
        const auto bytes = static_cast<std::size_t>(glyph_cells(glyph));
        if (sjis_bytes + bytes > 18) {
            break;
        }
        sjis_bytes += bytes;
        result.append(glyph);
        position += glyph.size();
    }
    return result;
}

}  // namespace th2
