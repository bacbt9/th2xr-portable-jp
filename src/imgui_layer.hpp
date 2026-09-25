#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>

namespace th2 {

class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window* window, SDL_Renderer* renderer);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void process_event(const SDL_Event& event);
    void new_frame(SDL_Window* window, float display_scale = 1.0f);
    void render();
    bool wants_input() const;
    bool wants_mouse() const;
    void rebuild_font_atlas(float display_scale);
    // IME caret rectangle in ImGui coordinates (Platform_SetImeDataFn).
    void set_text_input_area(float x, float y, float line_height);

    // Enable vertical drag-to-scroll for the current ImGui window/child on
    // touch screens. Call once per frame inside the scrollable region.
    void touch_drag_scroll();

    // Direct touch feed for ImGui (position only). Used on Android where SDL
    // touch-to-mouse synthesis is disabled.
    void on_touch_down(float normalized_x, float normalized_y);
    void on_touch_motion(
        float normalized_x, float normalized_y,
        float normalized_dx, float normalized_dy);
    void on_touch_up(float normalized_x, float normalized_y);

private:
    void apply_mobile_style(float scale);

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    bool touch_scroll_active_ = false;
    bool touch_down_ = false;
    std::uint64_t last_frame_ticks_ = 0;
    std::string imgui_font_path_;
    float display_scale_ = 1.0f;
    float last_font_scale_ = 0.0f;
    float last_style_scale_ = 0.0f;
    // ImGui 1.92 rasterises glyphs on demand, so the TTF data must outlive
    // the atlas; it is replaced when the atlas is rebuilt.
    std::unique_ptr<void, decltype(&SDL_free)> imgui_font_data_{nullptr, SDL_free};
};

}  // namespace th2
