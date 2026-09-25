#pragma once

#include "archive.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace th2 {

// Japanese font shipped in fonts/ next to the executable (OFL, see
// fonts/OFL.txt). It is the default modern font and the kana/kanji fallback
// for any other family.
inline constexpr std::string_view bundled_font_family = "Zen Maru Gothic";

// UTF-8 path of the bundled font: <exe>/fonts, macOS <exe>/../Resources/fonts,
// or the fonts/ APK asset on Android.
std::string bundled_font_path();

class GameFont {
public:
    explicit GameFont(const Archive& archive);
    ~GameFont();

    int glyph_width(unsigned char character) const;
    bool authentic() const { return authentic_; }
    void configure(bool authentic, std::string_view family,
                   int font_size, float framebuffer_scale);
    static const std::vector<std::string>& system_families();
    float text_width(std::string_view text) const;
    void draw(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t red = 255, std::uint8_t green = 255,
        std::uint8_t blue = 255, std::uint8_t alpha = 255) const;
    void draw_original(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t red = 255, std::uint8_t green = 255,
        std::uint8_t blue = 255, std::uint8_t alpha = 255) const;
    void draw_save_menu(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t red = 255, std::uint8_t green = 255,
        std::uint8_t blue = 255, std::uint8_t alpha = 255) const;
    void draw_authentic_shadow(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t alpha = 255) const;

    // Furigana: 16px like the original RUBI_FONT (font16.fd0 when authentic,
    // otherwise the modern family at two thirds of the message size).
    float ruby_text_width(std::string_view text) const;
    void draw_ruby(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha) const;

private:
    struct Modern;
    static constexpr int size = 24;
    static constexpr int width = 12;
    std::vector<std::uint8_t> data_;
    std::vector<std::uint8_t> save_menu_data_;
    std::vector<std::uint8_t> shadow_data_;
    int shadow_width_ = 0;
    std::unique_ptr<Modern> modern_;
    std::unique_ptr<Modern> ruby_modern_;
    bool authentic_ = false;
    std::string family_;
    int font_size_ = size;
    float framebuffer_scale_ = 1.0f;

    const std::uint8_t* glyph(unsigned char character) const;
    // font24.fd0 bitmap for gaiji U+E000 + index (CP932 0xF040 + index).
    const std::uint8_t* gaiji_bitmap(int index) const;
    int ruby_size() const;
    float bitmap_text_width(
        std::string_view text, int full_width, int half_width) const;
    void draw_bitmap(
        SDL_Renderer* renderer, float x, float y, std::string_view text,
        std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha) const;
    void draw_bitmap_face(
        SDL_Renderer* renderer, const std::vector<std::uint8_t>& data,
        int font_size, int half_width, float x, float y,
        std::string_view text, std::uint8_t red, std::uint8_t green,
        std::uint8_t blue, std::uint8_t alpha) const;
};

}  // namespace th2
