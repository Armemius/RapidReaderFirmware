#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <filesystem>

#include "ui/RapidReaderBrand.h"
#include "ui/screens/ScreenCommon.h"

// The preview models the built-in theme with no SD card installed.
const ui::themes::Theme& ThemeStore::resolve(std::string_view) const { return themes_.front(); }
const ui::themes::Theme& ThemeStore::next(std::string_view) const { return themes_.front(); }

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    std::filesystem::create_directories(argv[1]);
    Arduino_Canvas panel(640, 172, nullptr);
    if (!panel.begin(GFX_SKIP_OUTPUT_BEGIN))
        return 1;
    ui::Context ui(panel);
    ui.setTheme(ui::themes::defaultTheme());
    settings::InterfaceSettings config;
    screens::InterfaceScreen display;
    constexpr std::array<uint32_t, 5> standby{0, 60000, 300000, 900000, 1800000};
    for (const auto name : {"boot", "menu-ru", "settings-ru", "settings-en"}) {
        ui.invalidate();
        auto screen = screens::Screen::Read;
        if (std::string_view{name} == "boot") {
            screens::rapidReaderBoot(ui);
        } else {
            ui.beginFrame(static_cast<uint8_t>(screen));
            if (std::string_view{name} == "menu-ru") {
                screens::read(ui, "Война и мир", "Лев Толстой", 42, screen);
            } else {
                config.locale = std::string_view{name} == "settings-en" ? "en" : "ru";
                ui.setLocale(config.locale);
                // Apply the language before beginFrame so the entire screen redraws.
                ui.beginFrame(static_cast<uint8_t>(screens::Screen::Settings));
                display.draw(ui, config, standby, nullptr, screen);
            }
            ui.endFrame();
        }
        const auto path = std::filesystem::path{argv[1]} / (std::string{name} + ".ppm");
        auto* output = std::fopen(path.string().c_str(), "wb");
        if (!output)
            return 1;
        std::fprintf(output, "P6\n640 172\n255\n");
        for (size_t i = 0; i < 640 * 172; ++i) {
            const uint16_t pixel = panel.getFramebuffer()[i];
            const unsigned char rgb[]{static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
                                      static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63),
                                      static_cast<unsigned char>((pixel & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, output);
        }
        std::fclose(output);
    }
}
