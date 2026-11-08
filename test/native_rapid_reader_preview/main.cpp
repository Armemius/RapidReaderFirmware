#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <filesystem>

#include "ui/RapidReaderBrand.h"
#include "ui/screens/ScreenCommon.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ChaptersScreen.h"

// The preview models the built-in theme with no SD card installed.
const ui::themes::Theme& ThemeStore::resolve(std::string_view) const { return themes_.front(); }
const ui::themes::Theme& ThemeStore::next(std::string_view) const { return themes_.front(); }
std::string_view BookLibrary::displayName(const Entry& book) { return book.title; }
bool BookLibrary::isArticle(const Entry&) { return false; }

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    std::filesystem::create_directories(argv[1]);
    Arduino_Canvas panel(640, 172, nullptr);
    if (!panel.begin(GFX_SKIP_OUTPUT_BEGIN))
        return 1;
    ui::Context ui(panel);
    const auto theme = ui::themes::defaultTheme();
    ui.setTheme(theme);
    settings::InterfaceSettings config;
    screens::InterfaceScreen display;
    constexpr std::array<uint32_t, 5> standby{0, 60000, 300000, 900000, 1800000};
    for (const auto name : {"boot", "menu-ru", "settings-ru", "settings-en", "library-ru", "chapters-ru", "loading-ru", "long-title-ru"}) {
        ui.invalidate();
        auto screen = screens::Screen::Read;
        if (std::string_view{name} == "boot") {
            screens::rapidReaderBoot(ui);
        } else if (std::string_view{name} == "loading-ru") {
            ui.setLocale("ru");
            screens::status(ui, "Загрузка книги", "Война и мир", "Лев Толстой", 64);
        } else {
            ui.setLocale(std::string_view{name} == "settings-en" ? "en" : "ru");
            ui.beginFrame(static_cast<uint8_t>(screen));
            if (std::string_view{name} == "menu-ru") {
                screens::read(ui, "Война и мир", "Лев Толстой", 42, screen);
            } else if (std::string_view{name} == "long-title-ru") {
                screens::read(ui, "Понедельник начинается в субботу", "Аркадий и Борис Стругацкие", 7, screen);
            } else if (std::string_view{name} == "library-ru") {
                screens::LibraryScreen library;
                const std::array<BookLibrary::Entry, 3> books{{
                    {.title = "Война и мир", .author = "Лев Толстой"},
                    {.title = "Понедельник начинается в субботу", .author = "Аркадий и Борис Стругацкие"},
                    {.title = "Мастер и Маргарита", .author = "Михаил Булгаков"}}};
                const std::vector<screens::LibraryItem> items{{&books[0], "", 42}, {&books[1], "", 7}, {&books[2], "", 0}};
                library.draw(ui, items, 0, screen);
            } else if (std::string_view{name} == "chapters-ru") {
                screens::ChaptersScreen chapters;
                const std::array<ChapterMarker, 5> markers{{{"Вступление", 0}, {"Часть первая", 5},
                    {"Часть вторая", 10}, {"Часть третья", 15}, {"Послесловие", 20}}};
                ReadingSession reader;
                chapters.draw(ui, markers, reader, settings::ReadingSettings{}, 0, screen);
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
