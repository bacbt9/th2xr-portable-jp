#pragma once

// Internal to font.cpp / font_modern.cpp: the SDL_ttf renderer behind
// GameFont's modern (non-authentic) mode.

#include "font.hpp"

#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace th2 {

struct TtfFontCloser {
    void operator()(TTF_Font* font) const { TTF_CloseFont(font); }
};
using TtfFont = std::unique_ptr<TTF_Font, TtfFontCloser>;

struct GameFont::Modern {
    struct TextureDeleter {
        void operator()(SDL_Texture* texture) const
        {
            SDL_DestroyTexture(texture);
        }
    };
    using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;

    // The bundled Japanese font attached with TTF_AddFallbackFont. Declared
    // before `font` so it is closed after the font that references it.
    TtfFont fallback;
    TtfFont font;
    std::string family;
    int logical_size = 0;
    float scale = 0.0f;
    std::unordered_map<std::string, Texture> textures;
    std::array<Texture, 8> gaiji;

    void open(std::string_view requested_family, int requested_size,
              float requested_scale);
    // Single line in logical (800x600) pixels. Gaiji advance logical_size.
    float width(std::string_view line);
    void draw(const GameFont& owner, SDL_Renderer* renderer, float x, float y,
              std::string_view line, std::uint8_t red, std::uint8_t green,
              std::uint8_t blue, std::uint8_t alpha);

private:
    SDL_Texture* text_texture(SDL_Renderer* renderer, std::string_view run,
                              std::uint8_t red, std::uint8_t green,
                              std::uint8_t blue);
    SDL_Texture* gaiji_texture(const GameFont& owner, SDL_Renderer* renderer,
                               int index);
};

}  // namespace th2
