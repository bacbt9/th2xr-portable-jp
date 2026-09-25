#include "message.hpp"

#include "text_layout.hpp"

namespace th2 {
namespace {

bool is_ascii_letter(char character, char lower)
{
    return character == lower || character == lower - 'a' + 'A';
}

// Mirrors the markup handling of the original my_inc2/text.cpp
// TXT_DrawTextEx. Tags open with `<` plus a letter and close with `>`; the
// text between them stays visible. `<r base|reading>` is ruby, which is kept
// in the visible text between th2::ruby_* markers.
std::vector<std::string> split_segments(std::string_view source)
{
    enum class Tag { plain, ruby };
    std::vector<Tag> tags;
    bool ruby_has_reading = false;
    std::vector<std::string> segments(1);
    for (std::size_t position = 0; position < source.size();) {
        const char character = source[position];
        if (character == '^') {
            segments.back().push_back(' ');
            ++position;
            continue;
        }
        if (character == '~') {
            segments.back().push_back(',');
            ++position;
            continue;
        }
        if (character == '\\') {
            const char command =
                position + 1 < source.size() ? source[position + 1] : '\0';
            position += 2;
            if (command == 'k') {
                if (segments.back().find_first_not_of(" \t")
                    == std::string::npos) {
                    segments.back().clear();
                }
                segments.emplace_back();
            } else if (command == 'n') {
                segments.back().push_back('\n');
            } else if (command == '^' || command == '~' || command == '<'
                       || command == '>' || command == '|'
                       || command == '\\') {
                segments.back().push_back(command);
            }
            continue;
        }
        if (character == '<') {
            const char command =
                position + 1 < source.size() ? source[position + 1] : '\0';
            position += 2;
            if (is_ascii_letter(command, 'r')) {
                tags.push_back(Tag::ruby);
                ruby_has_reading = false;
                segments.back().append(ruby_anchor);
            } else if (is_ascii_letter(command, 'a')) {
                tags.push_back(Tag::plain);
            } else if (is_ascii_letter(command, 'd')
                       || is_ascii_letter(command, 'c')
                       || is_ascii_letter(command, 'f')
                       || is_ascii_letter(command, 's')
                       || is_ascii_letter(command, 'w')) {
                tags.push_back(Tag::plain);
                while (position < source.size()
                       && source[position] >= '0' && source[position] <= '9') {
                    ++position;
                }
                if (position < source.size() && source[position] == ':') {
                    ++position;
                }
            }
            continue;
        }
        if (character == '|') {
            ++position;
            if (!tags.empty() && tags.back() == Tag::ruby && !ruby_has_reading) {
                auto end = source.find('>', position);
                if (end == std::string_view::npos) {
                    end = source.size();
                }
                segments.back().append(ruby_separator);
                segments.back().append(source.substr(position, end - position));
                ruby_has_reading = true;
                position = end;
            }
            continue;
        }
        if (character == '>') {
            ++position;
            if (!tags.empty()) {
                if (tags.back() == Tag::ruby) {
                    if (!ruby_has_reading) {
                        segments.back().append(ruby_separator);
                    }
                    segments.back().append(ruby_terminator);
                }
                tags.pop_back();
            }
            continue;
        }
        const auto byte = static_cast<unsigned char>(character);
        ++position;
        if (byte == '\n' || (byte >= ' ' && byte <= '~')) {
            segments.back().push_back(character);
        } else if (byte >= 0xc2 && byte <= 0xf4) {
            const auto start = position - 1;
            const std::size_t extra = byte <= 0xdf ? 1 : byte <= 0xef ? 2 : 3;
            bool valid = start + extra < source.size();
            for (std::size_t j = 1; valid && j <= extra; ++j) {
                const auto next =
                    static_cast<unsigned char>(source[start + j]);
                valid = next >= 0x80 && next <= 0xbf;
            }
            if (valid) {
                segments.back().append(source.substr(start, 1 + extra));
                position = start + 1 + extra;
            }
        }
    }
    for (auto tag = tags.rbegin(); tag != tags.rend(); ++tag) {
        if (*tag == Tag::ruby) {
            if (!ruby_has_reading) {
                segments.back().append(ruby_separator);
            }
            segments.back().append(ruby_terminator);
            ruby_has_reading = true;
        }
    }
    if (segments.size() > 1 && segments.back().empty()) {
        segments.pop_back();
    }
    return segments;
}

}  // namespace

std::string message_markup_text(std::string_view source)
{
    std::string result;
    for (const auto& segment : split_segments(source)) {
        result += segment;
    }
    return result;
}

void Message::set(std::string_view source)
{
    segments_ = split_segments(source);
    revealed_ = 0;
    visible_.clear();
    reveal_current();
}

void Message::append(std::string_view source)
{
    auto added = split_segments(source);
    if (segments_.empty()) {
        segments_ = std::move(added);
        revealed_ = 0;
        visible_.clear();
        reveal_current();
        return;
    }
    segments_.insert(
        segments_.end(), std::make_move_iterator(added.begin()),
        std::make_move_iterator(added.end()));
    reveal_current();
}

bool Message::reveal_next()
{
    if (revealed_ >= segments_.size()) {
        return false;
    }
    reveal_current();
    return true;
}

void Message::reveal_current()
{
    if (revealed_ < segments_.size()) {
        visible_ += segments_[revealed_++];
    }
}

void Message::restore_state(
    const std::vector<std::string>& segments,
    std::size_t revealed, const std::string& visible)
{
    segments_ = segments;
    revealed_ = revealed;
    visible_ = visible;
}

}  // namespace th2
