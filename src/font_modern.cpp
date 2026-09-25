#include "font_modern.hpp"

#ifndef __ANDROID__
#include <fontconfig/fontconfig.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace th2 {
namespace {

constexpr std::string_view bundled_font_file = "ZenMaruGothic-Medium.ttf";
constexpr int gaiji_count = 8;
constexpr int gaiji_size = 24;

void initialize_font_libraries()
{
    static const bool initialized = [] {
        if (!TTF_Init()) {
            throw std::runtime_error(SDL_GetError());
        }
#ifndef __ANDROID__
        if (!FcInit()) {
            throw std::runtime_error("fontconfig initialization failed");
        }
#endif
        return true;
    }();
    (void)initialized;
}

std::filesystem::path utf8_path(std::string_view text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string font_path(std::string_view family)
{
    if (family == bundled_font_family) {
        return bundled_font_path();
    }
    std::error_code error;
    if (std::filesystem::is_regular_file(utf8_path(family), error)) {
        return std::string(family);
    }
#ifdef __ANDROID__
    return TH2_ANDROID_FONT_PATH;
#else
    const std::string name(family);
    FcPattern* pattern =
        FcNameParse(reinterpret_cast<const FcChar8*>(name.c_str()));
    if (!pattern) {
        throw std::runtime_error("cannot parse font family");
    }
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result = FcResultNoMatch;
    FcPattern* match = FcFontMatch(nullptr, pattern, &result);
    FcPatternDestroy(pattern);
    FcChar8* matched_path = nullptr;
    if (!match
        || FcPatternGetString(match, FC_FILE, 0, &matched_path)
            != FcResultMatch) {
        if (match) {
            FcPatternDestroy(match);
        }
        throw std::runtime_error("font family not found: " + name);
    }
    std::string path = reinterpret_cast<const char*>(matched_path);
    FcPatternDestroy(match);
    return path;
#endif
}

TtfFont open_font(const std::string& path, float size)
{
    TtfFont font(TTF_OpenFont(path.c_str(), size));
    if (!font) {
        throw std::runtime_error(
            "TTF_OpenFont failed for " + path + ": " + SDL_GetError());
    }
    TTF_SetFontHinting(font.get(), TTF_HINTING_LIGHT_SUBPIXEL);
    return font;
}

// Gaiji U+E000-U+E007 (CP932 0xF040-0xF047) encode as EE 80 80..87.
int gaiji_index_at(std::string_view text, std::size_t position)
{
    if (position + 3 > text.size()
        || static_cast<unsigned char>(text[position]) != 0xee
        || static_cast<unsigned char>(text[position + 1]) != 0x80) {
        return -1;
    }
    const int index = static_cast<unsigned char>(text[position + 2]) - 0x80;
    return index >= 0 && index < gaiji_count ? index : -1;
}

// Calls visit(run, gaiji) for each TrueType run (gaiji == -1) and each gaiji.
template <typename Visit>
void for_each_run(std::string_view text, Visit&& visit)
{
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size();) {
        const int gaiji = gaiji_index_at(text, i);
        if (gaiji < 0) {
            ++i;
            continue;
        }
        if (i > start) {
            visit(text.substr(start, i - start), -1);
        }
        visit(text.substr(i, 3), gaiji);
        i += 3;
        start = i;
    }
    if (start < text.size()) {
        visit(text.substr(start), -1);
    }
}

}  // namespace

std::string bundled_font_path()
{
#ifdef __ANDROID__
    return "fonts/" + std::string(bundled_font_file);
#else
    const char* base_path = SDL_GetBasePath();
    const auto base = utf8_path(base_path ? base_path : "");
    auto path = base / "fonts" / bundled_font_file;
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        path = base / ".." / "Resources" / "fonts" / bundled_font_file;
    }
    const auto utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
#endif
}

const std::vector<std::string>& GameFont::system_families()
{
    static const std::vector<std::string> families = [] {
        std::vector<std::string> result;
#ifdef __ANDROID__
        result.emplace_back("Liberation Serif");
#else
        if (!FcInit()) {
            return result;
        }
        FcPattern* pattern = FcPatternCreate();
        FcObjectSet* objects = FcObjectSetBuild(FC_FAMILY, nullptr);
        FcFontSet* fonts = FcFontList(nullptr, pattern, objects);
        if (fonts) {
            for (int i = 0; i < fonts->nfont; ++i) {
                FcChar8* family = nullptr;
                if (FcPatternGetString(
                        fonts->fonts[i], FC_FAMILY, 0, &family)
                    == FcResultMatch) {
                    result.emplace_back(
                        reinterpret_cast<const char*>(family));
                }
            }
            FcFontSetDestroy(fonts);
        }
        FcObjectSetDestroy(objects);
        FcPatternDestroy(pattern);
        std::erase(result, std::string(bundled_font_family));
        std::ranges::sort(result);
        result.erase(std::unique(result.begin(), result.end()), result.end());
#endif
        result.insert(result.begin(), std::string(bundled_font_family));
        return result;
    }();
    return families;
}

void GameFont::Modern::open(
    std::string_view requested_family, int requested_size,
    float requested_scale)
{
    initialize_font_libraries();
    if (font && family == requested_family && logical_size == requested_size
        && std::abs(scale - requested_scale) < 0.01f) {
        return;
    }
    font.reset();
    fallback.reset();
    textures.clear();

    const auto path = font_path(requested_family);
    const float point_size = requested_size * requested_scale;
    font = open_font(path, point_size);

    const auto bundled = bundled_font_path();
    std::error_code error;
    if (path != bundled
        && !std::filesystem::equivalent(
            utf8_path(path), utf8_path(bundled), error)) {
        try {
            fallback = open_font(bundled, point_size);
            if (!TTF_AddFallbackFont(font.get(), fallback.get())) {
                throw std::runtime_error(SDL_GetError());
            }
        } catch (const std::exception& failure) {
            SDL_Log("Japanese fallback font unavailable: %s", failure.what());
            fallback.reset();
        }
    }
    family = requested_family;
    logical_size = requested_size;
    scale = requested_scale;
}

float GameFont::Modern::width(std::string_view line)
{
    float result = 0.0f;
    for_each_run(line, [&](std::string_view run, int gaiji) {
        if (gaiji >= 0) {
            result += static_cast<float>(logical_size);
            return;
        }
        int pixel_width = 0;
        if (!TTF_GetStringSize(
                font.get(), run.data(), run.size(), &pixel_width, nullptr)) {
            throw std::runtime_error(SDL_GetError());
        }
        result += static_cast<float>(pixel_width) / scale;
    });
    return result;
}

SDL_Texture* GameFont::Modern::text_texture(
    SDL_Renderer* renderer, std::string_view run, std::uint8_t red,
    std::uint8_t green, std::uint8_t blue)
{
    std::string key(run);
    key.append({
        static_cast<char>(red),
        static_cast<char>(green),
        static_cast<char>(blue),
    });
    if (const auto found = textures.find(key); found != textures.end()) {
        return found->second.get();
    }
    auto* surface = TTF_RenderText_Blended(
        font.get(), run.data(), run.size(), SDL_Color{red, green, blue, 255});
    if (!surface) {
        throw std::runtime_error(SDL_GetError());
    }
    Texture texture(SDL_CreateTextureFromSurface(renderer, surface));
    SDL_DestroySurface(surface);
    if (!texture) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    if (textures.size() >= 512) {
        textures.clear();
    }
    return textures.emplace(std::move(key), std::move(texture))
        .first->second.get();
}

SDL_Texture* GameFont::Modern::gaiji_texture(
    const GameFont& owner, SDL_Renderer* renderer, int index)
{
    auto& texture = gaiji[static_cast<std::size_t>(index)];
    if (texture) {
        return texture.get();
    }
    // font24.fd0 glyphs are 24x24, 4-bit coverage, low nibble first.
    const auto* bitmap = owner.gaiji_bitmap(index);
    std::vector<std::uint8_t> rgba(gaiji_size * gaiji_size * 4, 255);
    for (int pixel = 0; pixel < gaiji_size * gaiji_size; ++pixel) {
        const auto packed = bitmap[pixel / 2];
        const auto coverage = pixel % 2 == 0 ? packed & 0x0f : packed >> 4;
        rgba[pixel * 4 + 3] = static_cast<std::uint8_t>(coverage * 17);
    }
    texture.reset(SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
        gaiji_size, gaiji_size));
    if (!texture
        || !SDL_UpdateTexture(
            texture.get(), nullptr, rgba.data(), gaiji_size * 4)) {
        texture.reset();
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_LINEAR);
    return texture.get();
}

void GameFont::Modern::draw(
    const GameFont& owner, SDL_Renderer* renderer, float x, float y,
    std::string_view line, std::uint8_t red, std::uint8_t green,
    std::uint8_t blue, std::uint8_t alpha)
{
    // Centre gaiji in the TrueType line box so they sit with the text.
    const float line_height =
        static_cast<float>(TTF_GetFontHeight(font.get())) / scale;
    const float gaiji_y = y + (line_height - logical_size) / 2.0f;
    for_each_run(line, [&](std::string_view run, int gaiji) {
        SDL_Texture* texture = nullptr;
        SDL_FRect destination{};
        if (gaiji >= 0) {
            texture = gaiji_texture(owner, renderer, gaiji);
            SDL_SetTextureColorMod(texture, red, green, blue);
            const auto size = static_cast<float>(logical_size);
            destination = {x, gaiji_y, size, size};
        } else {
            texture = text_texture(renderer, run, red, green, blue);
            float texture_width = 0.0f;
            float texture_height = 0.0f;
            SDL_GetTextureSize(texture, &texture_width, &texture_height);
            destination = {
                x, y, texture_width / scale, texture_height / scale};
        }
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        SDL_SetTextureAlphaMod(texture, 255);
        x += destination.w;
    });
}

}  // namespace th2
