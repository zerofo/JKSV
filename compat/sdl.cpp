//
// compat/sdl.cpp
//
// 旧版 `sdl::` API 兼容层实现，桥接到新版 SDLLib（commit 159856e, `sdl2::`）。
//

#include "sdl.hpp"

#include <SDL2/SDL_image.h>

#include <unordered_map>

namespace
{
    // ---- 全局组件，生命周期与 SDL 初始化绑定 ----
    sdl2::SDL2 *g_sdl2{};
    sdl2::Window *g_window{};
    sdl2::Renderer *g_renderer{};
    sdl2::Audio *g_audio{};

    /// @brief 系统字体按字号缓存，等价于旧版 SystemFont::resize(fontSize) 的语义。
    std::unordered_map<int, sdl2::SharedFont> g_fontCache{};

    sdl2::SharedFont get_cached_font(int fontSize)
    {
        const auto found = g_fontCache.find(fontSize);
        if (found != g_fontCache.end()) { return found->second; }

        const std::string name = "SystemFont_" + std::to_string(fontSize);
        auto font              = sdl2::FontManager::create_load_resource<sdl2::SystemFont>(name, fontSize);
        g_fontCache.emplace(fontSize, font);
        return font;
    }

    SDL_Color to_sdl_color(sdl::Color color) noexcept
    {
        return {.r = color.rgba[3], .g = color.rgba[2], .b = color.rgba[1], .a = color.rgba[0]};
    }

    bool set_render_target(const sdl::SharedTexture &target)
    {
        // target 为 Null（包装内无底层纹理）时切回主帧缓冲。
        const auto &wrapped = target->get_wrapped();
        if (!target || !wrapped) { return g_renderer->set_render_target(sdl2::SharedTexture{}); }
        return g_renderer->set_render_target(wrapped);
    }
} // namespace

//                      ---- Texture ----

bool sdl::Texture::render(SharedTexture &target, int x, int y) noexcept
{
    if (!m_texture) { return false; }
    return set_render_target(target) && m_texture->render(x, y);
}

bool sdl::Texture::render_stretched(SharedTexture &target, int x, int y, int width, int height) noexcept
{
    if (!m_texture) { return false; }
    return set_render_target(target) && m_texture->render_stretched(x, y, width, height);
}

bool sdl::Texture::render_part(SharedTexture &target, int x, int y, int sourceX, int sourceY, int sourceWidth, int sourceHeight) noexcept
{
    if (!m_texture) { return false; }
    return set_render_target(target) && m_texture->render_part(x, y, sourceX, sourceY, sourceWidth, sourceHeight);
}

bool sdl::Texture::render_part_stretched(SharedTexture &target,
                                         int sourceX,
                                         int sourceY,
                                         int sourceWidth,
                                         int sourceHeight,
                                         int destinationX,
                                         int destinationY,
                                         int destinationWidth,
                                         int destinationHeight) noexcept
{
    if (!m_texture) { return false; }
    // 注意：新版 render_part_stretched 参数顺序为 dest 在前、source 在后，这里做转换。
    const bool result = m_texture->render_part_stretched(destinationX, destinationY, destinationWidth, destinationHeight, sourceX, sourceY, sourceWidth, sourceHeight);
    return set_render_target(target) && result;
}

bool sdl::Texture::clear(Color color) noexcept
{
    if (!m_texture) { return false; }
    // 等价于旧版：设 target + draw color + RenderClear。
    const bool targetSet = g_renderer->set_render_target(m_texture);
    return targetSet && g_renderer->frame_begin(to_sdl_color(color));
}

bool sdl::Texture::set_color_mod(Color color) noexcept
{
    if (!m_texture) { return false; }
    const SDL_Color c = to_sdl_color(color);
    // 新版 set_color_mod 直接接收 SDL_Color。
    return m_texture->set_color_mod(c);
}

int sdl::Texture::get_width() const noexcept { return m_texture ? m_texture->get_width() : 0; }

int sdl::Texture::get_height() const noexcept { return m_texture ? m_texture->get_height() : 0; }

//                      ---- TextureManager ----

/// @brief 通用包装：把新库纹理包成 sdl::Texture。
static sdl::SharedTexture wrap_texture(const sdl2::SharedTexture &texture)
{
    if (!texture) { return nullptr; }
    auto wrapper   = std::make_shared<sdl::Texture>();
    wrapper->set_wrapped(texture);
    return wrapper;
}

sdl::SharedTexture sdl::TextureManager::load(std::string_view resourceName, std::string_view filePath)
{
    // 新库 Texture(string_view path) 从文件加载。
    auto texture = sdl2::TextureManager::create_load_resource(resourceName, filePath);
    return wrap_texture(texture);
}

sdl::SharedTexture sdl::TextureManager::load(std::string_view resourceName, const void *data, std::size_t dataSize)
{
    // 新库 Texture(const void*, size_t) 从内存加载。
    auto texture = sdl2::TextureManager::create_load_resource(resourceName, data, dataSize);
    return wrap_texture(texture);
}

sdl::SharedTexture sdl::TextureManager::load(std::string_view resourceName, int width, int height, SDL_TextureAccess access)
{
    // 新库 Texture(int,int,SDL_TextureAccess)。
    auto texture = sdl2::TextureManager::create_load_resource(resourceName, width, height, access);
    return wrap_texture(texture);
}

//                      ---- Sound / SoundManager ----

void sdl::Sound::play() const noexcept
{
    if (m_sound) { m_sound->play(); }
}

sdl::SharedSound sdl::SoundManager::load(std::string_view resourceName, std::string_view path)
{
    auto sound = sdl2::SoundManager::create_load_resource(resourceName, path);
    if (!sound) { return nullptr; }

    auto wrapper   = std::make_shared<sdl::Sound>();
    wrapper->set_wrapped(sound);
    return wrapper;
}

//                      ---- text ----

void sdl::text::render(SharedTexture &target, int x, int y, int fontSize, int wrapWidth, Color color, std::string_view text)
{
    if (!g_renderer) { return; }
    const auto font = get_cached_font(fontSize);
    if (!font || !set_render_target(target)) { return; }

    const SDL_Color c = to_sdl_color(color);
    // wrapWidth <= 0 或 NO_WRAP 表示不换行。
    if (wrapWidth == NO_WRAP || wrapWidth <= 0) { font->render_text(x, y, c, text); }
    else { font->render_text_wrapped(x, y, c, x + wrapWidth, text); }
}

int sdl::text::get_width(int fontSize, std::string_view text)
{
    const auto font = get_cached_font(fontSize);
    return font ? font->get_text_width(text) : 0;
}

void sdl::text::add_color_character(uint32_t codepoint, Color color)
{
    const SDL_Color c = to_sdl_color(color);
    // 新版改为 Font 的静态颜色点机制，语义等价：渲染到该字符时切换颜色。
    sdl2::Font::add_color_point(codepoint, c);
}

//                      ---- text::SystemFont ----

bool sdl::text::SystemFont::initialize()
{
    // 新版 SystemFont 的初始化（pl 服务 + freetype）由实例内部完成，这里校验一次可创建即可。
    const auto font = get_cached_font(1);
    return font != nullptr;
}

void sdl::text::SystemFont::exit() { g_fontCache.clear(); }

//                      ---- Top-level SDL functions ----

bool sdl::initialize(const char *windowTitle, int windowWidth, int windowHeight)
{
    (void)windowTitle; // 新库窗口不使用标题。

    g_sdl2     = new sdl2::SDL2{};
    g_window   = new sdl2::Window{windowWidth, windowHeight};
    g_renderer = new sdl2::Renderer(*g_window);
    if (!g_renderer->is_initialized()) { return false; }

    g_audio = new sdl2::Audio{};
    sdl2::Sound::initialize(*g_audio);

    // 保持旧版图标缩放质量。
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    const int imgError = IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    if (imgError == 0) { return false; }

    sdl2::Texture::initialize(*g_renderer);

    // 新版 Font 的断行点改为静态全局配置（旧版 text.cpp 内是硬编码集合），这里补上。
    sdl2::Font::add_break_points({static_cast<uint32_t>(L' '), static_cast<uint32_t>(L'　'), static_cast<uint32_t>(L'/'),
                                  static_cast<uint32_t>(L'_'), static_cast<uint32_t>(L'-'), static_cast<uint32_t>(L'。'),
                                  static_cast<uint32_t>(L'、')});

    return true;
}

void sdl::exit()
{
    delete g_renderer;
    delete g_window;
    delete g_audio;
    delete g_sdl2;

    g_renderer = nullptr;
    g_window   = nullptr;
    g_audio    = nullptr;
    g_sdl2     = nullptr;
}

bool sdl::frame_begin(Color color) noexcept
{
    if (!g_renderer) { return false; }
    const bool targetSet = set_render_target(sdl::SharedTexture{}); // 回到主帧缓冲
    return targetSet && g_renderer->frame_begin(to_sdl_color(color));
}

void sdl::frame_end() noexcept
{
    if (g_renderer) { g_renderer->frame_end(); }
}

bool sdl::render_rect(SharedTexture &target, int x, int y, int width, int height, Color color) noexcept
{
    // 新版无空心矩形；JKSV 未使用，这里按填充处理。
    if (!set_render_target(target)) { return false; }
    return g_renderer->render_rectangle(x, y, width, height, to_sdl_color(color));
}

bool sdl::render_rect_fill(SharedTexture &target, int x, int y, int width, int height, Color color) noexcept
{
    if (!set_render_target(target)) { return false; }
    return g_renderer->render_rectangle(x, y, width, height, to_sdl_color(color));
}

bool sdl::render_line(SharedTexture &target, int xA, int yA, int xB, int yB, Color color) noexcept
{
    if (!set_render_target(target)) { return false; }
    return g_renderer->render_line(xA, yA, xB, yB, to_sdl_color(color));
}
