#pragma once
//
// compat/sdl.hpp
//
// 旧版 `sdl::` API 兼容层，内部桥接到新版 SDLLib（commit 159856e, `sdl2::` 命名空间）。
// 主仓库源码继续按旧的"显式渲染目标"模式调用本头文件提供的 API，无需大规模改写。
//

#include "../Libraries/SDLLib/SDL/include/sdl.hpp"

#include <SDL2/SDL.h>
#include <cstdint>
#include <memory>

namespace sdl
{
    /// @brief 与旧版一致的 32 位颜色（ARGB）。
    union Color
    {
        uint32_t raw;
        uint8_t rgba[4];
    };

    namespace color
    {
        inline uint8_t get_red(Color c) noexcept { return c.rgba[3]; }
        inline uint8_t get_green(Color c) noexcept { return c.rgba[2]; }
        inline uint8_t get_blue(Color c) noexcept { return c.rgba[1]; }
        inline uint8_t get_alpha(Color c) noexcept { return c.rgba[0]; }

        inline void set_red(Color &c, uint8_t value) noexcept { c.rgba[3] = value; }
        inline void set_green(Color &c, uint8_t value) noexcept { c.rgba[2] = value; }
        inline void set_blue(Color &c, uint8_t value) noexcept { c.rgba[1] = value; }
        inline void set_alpha(Color &c, uint8_t value) noexcept { c.rgba[0] = value; }

        inline Color create_color(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha) noexcept
        {
            return {(red << 24 & 0xFF000000) | (green << 16 & 0x00FF0000) | (blue << 8 & 0x0000FF00) | (alpha & 0xFF)};
        }
    } // namespace color

    class Texture;
    using SharedTexture = std::shared_ptr<Texture>;

    /// @brief 旧版 `sdl::Texture` 的兼容实现，包装新库 `sdl2::Texture`。
    /// @note 所有渲染方法都先切换渲染目标，保持旧的显式 target 语义。
    class Texture
    {
        public:
            Texture() = default;

            bool render(SharedTexture &target, int x, int y) noexcept;
            bool render_stretched(SharedTexture &target, int x, int y, int width, int height) noexcept;
            bool render_part(SharedTexture &target, int x, int y, int sourceX, int sourceY, int sourceWidth, int sourceHeight) noexcept;
            bool render_part_stretched(SharedTexture &target,
                                       int sourceX,
                                       int sourceY,
                                       int sourceWidth,
                                       int sourceHeight,
                                       int destinationX,
                                       int destinationY,
                                       int destinationWidth,
                                       int destinationHeight) noexcept;
            bool clear(Color color) noexcept;
            bool set_color_mod(Color color) noexcept;
            int get_width() const noexcept;
            int get_height() const noexcept;

            /// @brief 传给 target 参数表示渲染到主帧缓冲（对应旧的 `sdl::Texture::Null`）。
            static inline SharedTexture Null = std::make_shared<Texture>();

        public:
            /// @brief 返回包装的底层纹理（Null 纹理为空），供兼容层读取。
            const sdl2::SharedTexture &get_wrapped() const noexcept { return m_texture; }

            /// @brief 仅供内部工厂使用。
            void set_wrapped(const sdl2::SharedTexture &texture) noexcept { m_texture = texture; }

        private:
            friend class TextureManager;
            sdl2::SharedTexture m_texture{};
    }; // class Texture

    /// @brief 旧版 `sdl::TextureManager` 的兼容实现。
    /// @note load() 的实参与旧版一致：名称 + 文件路径 / 内存缓冲 / 宽高中任一种组合。
    class TextureManager
    {
        public:
            /// @brief 从文件路径加载。
            static SharedTexture load(std::string_view resourceName, std::string_view filePath);
            /// @brief 从内存缓冲加载（任意指针类型按 void* 传入）。
            static SharedTexture load(std::string_view resourceName, const void *data, std::size_t dataSize);
            /// @brief 创建空白目标纹理。
            static SharedTexture load(std::string_view resourceName, int width, int height, SDL_TextureAccess access);
    }; // class TextureManager

    /// @brief 旧版 `sdl::Sound` 的兼容实现，包装新库 `sdl2::Sound`。
    class Sound
    {
        public:
            void play() const noexcept;

            /// @brief 返回包装的底层声音。
            const sdl2::SharedSound &get_wrapped() const noexcept { return m_sound; }

            /// @brief 仅供内部工厂使用。
            void set_wrapped(const sdl2::SharedSound &sound) noexcept { m_sound = sound; }

        private:
            friend class SoundManager;
            sdl2::SharedSound m_sound{};
    }; // class Sound

    using SharedSound = std::shared_ptr<Sound>;

    /// @brief 旧版 `sdl::SoundManager` 的兼容实现。
    class SoundManager
    {
        public:
            static SharedSound load(std::string_view resourceName, std::string_view path);
    }; // class SoundManager

    namespace text
    {
        /// @brief 告诉 render() 不要自动换行。
        constexpr int NO_WRAP = -1;

        /// @brief 在 target 上渲染文本。wrapWidth 为 -1(NO_WRAP) 或正数（相对 x 的最大宽度）。
        void render(SharedTexture &target, int x, int y, int fontSize, int wrapWidth, Color color, std::string_view text);

        /// @brief 返回指定字号下文本的像素宽度。
        int get_width(int fontSize, std::string_view text);

        /// @brief 注册一个会在渲染时切换颜色的特殊字符。
        void add_color_character(uint32_t codepoint, Color color);

        namespace SystemFont
        {
            /// @brief 旧 API：初始化系统字体。新版由实例 RAII 管理，这里做一次有效性校验。
            bool initialize();

            /// @brief 旧 API：释放。
            void exit();
        } // namespace SystemFont
    } // namespace text

    /// @brief 初始化 SDL（窗口、渲染器、音频、纹理系统）。windowTitle 在新库中不使用，保留以兼容旧签名。
    bool initialize(const char *windowTitle, int windowWidth, int windowHeight);

    /// @brief 反初始化 SDL。
    void exit();

    /// @bool 开始一帧并把主帧缓冲清成指定颜色。
    bool frame_begin(Color color) noexcept;

    /// @brief 结束一帧并交换缓冲。
    void frame_end() noexcept;

    /// @brief 在 target 上画矩形（旧版为空心；新库只支持填充，JKSV 实际未使用此函数）。
    bool render_rect(SharedTexture &target, int x, int y, int width, int height, Color color) noexcept;

    /// @brief 在 target 上画填充矩形。
    bool render_rect_fill(SharedTexture &target, int x, int y, int width, int height, Color color) noexcept;

    /// @brief 在 target 上画线段。
    bool render_line(SharedTexture &target, int xA, int yA, int xB, int yB, Color color) noexcept;

} // namespace sdl
