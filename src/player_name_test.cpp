#include "player_name.hpp"

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (!condition) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures;
    }
}

}  // namespace

int main()
{
    const auto defaults = th2::load_default_player_name();
    expect(defaults.family == "河野" && defaults.given == "貴明"
               && defaults.family_reading == "こうの"
               && defaults.given_reading == "たかあき"
               && defaults.nickname == "たか"
               && defaults.nickname_reading == "タカ",
        "defaults are DEF_NAME_*");

    expect(th2::substitute_player_name(
               "*nl*nf（*nlk*nfk）*nn/*nnk", defaults)
            == "河野貴明（こうのたかあき）たか/タカ",
        "default substitution");
    expect(th2::substitute_player_name("*nf2/*nlk3/*nnk1/*nn6", defaults)
            == "明/の/タ/",
        "indexed substitution counts characters");
    expect(th2::substitute_player_name("*nn7", defaults) == "たか7",
        "index outside 1-6 is literal");

    expect(th2::substitute_player_name("*h2さん", defaults) == "小牧さん",
        "*h2 without flag 213");
    expect(th2::substitute_player_name("*h2さん", defaults, true)
            == "愛佳さん",
        "*h2 with flag 213");

    // The dialog never edits NameNNK; with a custom name DefaultCharName is
    // cleared and *nnk falls back to the nickname.
    auto custom = defaults;
    custom.nickname = "ぽち";
    expect(!th2::uses_default_voice_name(custom, defaults),
        "custom nickname is not the default name");
    expect(th2::substitute_player_name("*nnk/*nnk2", custom) == "ぽち/ち",
        "*nnk uses the nickname for custom names");
    auto reading_only = defaults;
    reading_only.nickname_reading = "たか";
    expect(th2::uses_default_voice_name(reading_only, defaults),
        "nickname reading is not part of DefaultCharName");

    expect(th2::validate_player_name(defaults).empty(), "defaults valid");
    auto missing = defaults;
    missing.given_reading.clear();
    expect(th2::validate_player_name(missing)
            == "名前に未入力の項目があります",
        "empty field rejected");
    auto half = defaults;
    half.family = "Kono";
    expect(th2::validate_player_name(half) == "名前に半角が含まれています",
        "ASCII rejected");
    half.family = "ｺｳﾉ";
    expect(th2::validate_player_name(half) == "名前に半角が含まれています",
        "half-width katakana rejected");
    half.family = "河野😀";
    expect(th2::validate_player_name(half) == "名前に半角が含まれています",
        "characters without a CP932 code rejected");
    auto mixed = half;
    mixed.nickname.clear();
    expect(th2::validate_player_name(mixed)
            == "名前に未入力の項目があります",
        "missing fields are reported before half-width ones");
    auto full_width = defaults;
    full_width.family = "ＡＢＣ①";
    full_width.nickname = "たかたかたか";
    expect(th2::validate_player_name(full_width).empty(),
        "full-width and NEC special characters accepted, six characters ok");
    full_width.nickname = "たかたかたかた";
    expect(th2::validate_player_name(full_width)
            == "名前は全角6文字以内で入力してください",
        "seven characters rejected");

    return failures == 0 ? 0 : 1;
}
