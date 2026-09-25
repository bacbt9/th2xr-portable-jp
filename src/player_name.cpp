#include "player_name.hpp"

#include <algorithm>
#include <array>

#include <iconv.h>

namespace th2 {
namespace {

std::size_t utf8_sequence_bytes(std::string_view value, std::size_t position)
{
    const auto lead = static_cast<unsigned char>(value[position]);
    const std::size_t length = (lead & 0x80) == 0 ? 1
        : (lead & 0xe0) == 0xc0 ? 2
        : (lead & 0xf0) == 0xe0 ? 3
        : (lead & 0xf8) == 0xf0 ? 4 : 1;
    return std::min(length, value.size() - position);
}

std::string indexed_value(std::string_view value, char index)
{
    if (index < '1' || index > '6') {
        return std::string(value);
    }
    const auto wanted = static_cast<std::size_t>(index - '1');
    std::size_t position = 0;
    for (std::size_t character = 0; position < value.size(); ++character) {
        const auto length = utf8_sequence_bytes(value, position);
        if (character == wanted) {
            return std::string(value.substr(position, length));
        }
        position += length;
    }
    return {};
}

// Byte length of `glyph` in CP932, or 0 when it has no CP932 encoding.
std::size_t cp932_bytes(std::string_view glyph)
{
    iconv_t converter = iconv_open("CP932", "UTF-8");
    if (converter == reinterpret_cast<iconv_t>(-1)) {
        converter = iconv_open("SHIFT_JIS", "UTF-8");
    }
    if (converter == reinterpret_cast<iconv_t>(-1)) {
        return 0;
    }
    std::array<char, 8> output{};
    char* input = const_cast<char*>(glyph.data());
    std::size_t input_left = glyph.size();
    char* destination = output.data();
    std::size_t output_left = output.size();
    const auto result = iconv(
        converter, &input, &input_left, &destination, &output_left);
    iconv_close(converter);
    if (result == static_cast<std::size_t>(-1) || input_left != 0) {
        return 0;
    }
    return output.size() - output_left;
}

}  // namespace

PlayerName load_default_player_name()
{
    return {"河野", "貴明", "こうの", "たかあき", "たか", "タカ"};
}

bool uses_default_voice_name(
    const PlayerName& name, const PlayerName& default_name)
{
    return name.family == default_name.family
        && name.given == default_name.given
        && name.family_reading == default_name.family_reading
        && name.given_reading == default_name.given_reading
        && name.nickname == default_name.nickname;
}

std::string validate_player_name(const PlayerName& name)
{
    const std::array fields{
        &name.family, &name.family_reading, &name.given,
        &name.given_reading, &name.nickname,
    };
    for (const auto* field : fields) {
        if (field->empty()) {
            return "名前に未入力の項目があります";
        }
    }
    // The original compares strlen with _mbslen * 2: every character must be
    // a double-byte Shift_JIS code. Characters Windows cannot convert to
    // CP932 become a half-width '?', so they fail the same check.
    for (const auto* field : fields) {
        for (std::size_t position = 0; position < field->size();) {
            const auto length = utf8_sequence_bytes(*field, position);
            if (cp932_bytes(std::string_view(*field).substr(position, length))
                != 2) {
                return "名前に半角が含まれています";
            }
            position += length;
        }
    }
    for (const auto* field : fields) {
        std::size_t characters = 0;
        for (std::size_t position = 0; position < field->size();
             position += utf8_sequence_bytes(*field, position)) {
            ++characters;
        }
        if (characters > max_player_name_characters) {
            return "名前は全角6文字以内で入力してください";
        }
    }
    return {};
}

std::string substitute_player_name(
    std::string_view source, const PlayerName& name,
    bool use_komaki_given_name)
{
    // AVG_SetName reads NameNNK only while DefaultCharName is set; a custom
    // name makes *nnk expand to the nickname instead.
    const auto& nickname_reading =
        uses_default_voice_name(name, load_default_player_name())
        ? name.nickname_reading : name.nickname;
    struct Replacement {
        std::string_view token;
        const std::string* value;
    };
    const std::array replacements{
        Replacement{"*nnk", &nickname_reading},
        Replacement{"*nlk", &name.family_reading},
        Replacement{"*nfk", &name.given_reading},
        Replacement{"*nn", &name.nickname},
        Replacement{"*nl", &name.family},
        Replacement{"*nf", &name.given},
    };

    std::string result;
    for (std::size_t position = 0; position < source.size();) {
        if (source.substr(position).starts_with("*h2")) {
            result += use_komaki_given_name ? "愛佳" : "小牧";
            position += 3;
            continue;
        }
        bool replaced = false;
        for (const auto& replacement : replacements) {
            if (source.substr(position).starts_with(replacement.token)) {
                position += replacement.token.size();
                char index = '\0';
                if (position < source.size()
                    && source[position] >= '1' && source[position] <= '6') {
                    index = source[position++];
                }
                result += indexed_value(*replacement.value, index);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            result.push_back(source[position++]);
        }
    }
    return result;
}

}  // namespace th2
