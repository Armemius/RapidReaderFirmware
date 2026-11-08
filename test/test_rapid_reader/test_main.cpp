#include <unity.h>

#include <array>
#include <string>

#include "product/RapidReaderSettings.h"
#include "fonts/LiterataFallbackAlpha4.h"
#include "settings/SettingsCodec.h"
#include "ui/RapidReaderBrand.h"
#include "ui/RapidReaderUi.h"
#include "fonts/RapidMono13.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/ScreenCommon.h"

// The host filesystem shim has no SD directories. Supply its single built-in
// theme; the firmware uses the real ThemeStore implementation.
const ui::themes::Theme& ThemeStore::resolve(std::string_view) const { return themes_.front(); }
const ui::themes::Theme& ThemeStore::next(std::string_view) const { return themes_.front(); }
std::string_view BookLibrary::displayName(const Entry& book) { return book.title; }
bool BookLibrary::isArticle(const Entry&) { return false; }

namespace {
    class RecordingGfx : public Arduino_GFX {
    public:
        RecordingGfx() : Arduino_GFX(640, 172) {}
        size_t write(uint8_t byte) override {
            text.push_back(static_cast<char>(byte));
            return Arduino_GFX::write(byte);
        }
        std::string text;
    };
    const ui::themes::Theme& testTheme() {
        static const auto theme = ui::themes::defaultTheme();
        return theme;
    }
    ui::TouchContact contact;
    ui::TouchSampleResult poll(ui::TouchContact& result) {
        result = contact;
        return ui::TouchSampleResult::Contact;
    }
    constexpr std::array<uint32_t, 5> standbyMs{0, 60000, 300000, 900000, 1800000};

    template<class Draw>
    void tap(ui::Context& ui, uint16_t x, uint16_t y, uint32_t time, Draw draw) {
        contact = {true, x, y};
        ui.pollTouch(time);
        draw();
        contact = {};
        ui.pollTouch(time + 80);
        draw();
    }
}

void setUp() { contact = {}; }
void tearDown() {}

void test_languages_are_complete_and_available_without_sd() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    TEST_ASSERT_EQUAL_STRING("ru", settings::InterfaceSettings{}.locale.c_str());
    TEST_ASSERT_TRUE(ui.text(UiText::Settings) == "Настройки");
    for (const auto locale : {"ru", "en"}) {
        ui.setLocale(locale);
        for (size_t i = 0; i < static_cast<size_t>(UiText::Count); ++i)
            TEST_ASSERT_FALSE(ui.text(static_cast<UiText>(i)).empty());
    }
    TEST_ASSERT_TRUE(ui.text(UiText::Settings) == "Settings");
    ui.setLocale("ru");
    TEST_ASSERT_TRUE(ui.text(UiText::Library) == "Библиотека");
}

void test_embedded_reading_fonts_cover_the_russian_alphabet_in_every_size() {
    for (const auto* font : ui::fonts::LiterataFallbackAlpha4_Sizes) {
        const auto hasGlyph = [&](uint32_t codepoint) {
            uint16_t index = ui::fonts::kMissingGlyphIndex;
            return ui::fonts::residentGlyphIndex(*font, codepoint, index)
                && index != ui::fonts::kMissingGlyphIndex;
        };
        for (uint32_t codepoint = 0x0410; codepoint <= 0x044f; ++codepoint)
            TEST_ASSERT_TRUE(hasGlyph(codepoint));
        TEST_ASSERT_TRUE(hasGlyph(0x0401)); // Ё
        TEST_ASSERT_TRUE(hasGlyph(0x0451)); // ё
    }
}

void test_imported_hidden_settings_reset_and_visible_settings_survive() {
    auto imported = settings::codec::decodeToml(
        "[reading]\nwpm = 420\nleftHanded = true\nphantomWords = false\n"
        "[reading.typography]\nfontId = \"custom\"\ntracking = 3\n"
        "[interface]\nlocale = \"en\"\nbrightnessPercent = 85\n"
        "[updates]\ncheckOnStartup = true\nrepositoryOwner = \"other\"\n",
        settings::SettingsSource::Sd);
    TEST_ASSERT_TRUE(imported.has_value());
    product::applyDefaults(*imported);
    auto expected = settings::ReadingSettings{};
    expected.wpm = 420;
    TEST_ASSERT_TRUE(imported->reading == expected);
    TEST_ASSERT_TRUE(imported->updates == settings::UpdateSettings{});
    TEST_ASSERT_EQUAL_UINT8(85, imported->interface.brightnessPercent);
    TEST_ASSERT_EQUAL_STRING("en", imported->interface.locale.c_str());
    imported->interface.locale = "ja";
    product::applyDefaults(*imported);
    TEST_ASSERT_EQUAL_STRING("ru", imported->interface.locale.c_str());
}

void test_settings_draws_display_controls_and_toggles_language_repeatedly() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    screens::InterfaceScreen display;
    settings::InterfaceSettings config;
    auto screen = screens::Screen::Settings;
    ui.setTheme(testTheme());
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    const auto draw = [&] {
        ui.beginFrame(static_cast<uint8_t>(screens::Screen::Settings));
        display.draw(ui, config, standbyMs, nullptr, screen);
        ui.endFrame();
    };
    draw();
    TEST_ASSERT_TRUE(gfx.text.find("ЯРКОСТЬ") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Русский") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Устройство") == std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Фокус") == std::string::npos);
    for (uint32_t i = 0; i < 6; ++i) {
        tap(ui, 270, 130, 100 + i * 200, draw);
        TEST_ASSERT_EQUAL_STRING(i % 2 == 0 ? "en" : "ru", config.locale.c_str());
        TEST_ASSERT_TRUE(ui.text(UiText::Settings) == (i % 2 == 0 ? "Settings" : "Настройки"));
    }
    tap(ui, 615, 20, 1600, draw);
    TEST_ASSERT_TRUE(display.navigationAction == screens::Action::PowerOff);
    tap(ui, 345, 20, 1800, draw);
    TEST_ASSERT_TRUE(screen == screens::Screen::Read);
}

void test_reading_menu_keeps_library_chapters_and_has_no_font_override() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    auto screen = screens::Screen::Read;
    ui.beginFrame(static_cast<uint8_t>(screen));
    screens::read(ui, "Война и мир", "Лев Толстой", 42, screen);
    ui.endFrame();
    TEST_ASSERT_TRUE(gfx.text.find("Война и мир") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("ГЛАВЫ") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("БИБЛИОТЕКА") != std::string::npos);
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    const auto draw = [&] {
        ui.beginFrame(static_cast<uint8_t>(screens::Screen::Read));
        screens::read(ui, "Война и мир", "Лев Толстой", 42, screen);
        ui.endFrame();
    };
    tap(ui, 570, 40, 100, draw);
    TEST_ASSERT_TRUE(screen != screens::Screen::BookFonts);
}

void test_boot_identifies_product_and_version() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    screens::rapidReaderBoot(ui);
    TEST_ASSERT_TRUE(gfx.text.find("RAPIDREADER") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("rapid-reader-0.1.0") != std::string::npos);
}

void test_modern_ui_font_and_case_conversion_support_russian() {
    TEST_ASSERT_TRUE(locales::validateU8g2Font(std::span{ui::fonts::RapidMono13}).has_value());
    TEST_ASSERT_TRUE(screens::rapid::uppercase("Ёжик, съешь ещё яблок. Read 42%") == "ЁЖИК, СЪЕШЬ ЕЩЁ ЯБЛОК. READ 42%");
    TEST_ASSERT_EQUAL_UINT8(8, ui::fonts::kRapidUiCellWidth);
}

void test_brightness_capture_clamps_a_drag_outside_the_panel() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    screens::InterfaceScreen display;
    settings::InterfaceSettings config;
    auto screen = screens::Screen::Settings;
    ui.setTheme(testTheme());
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    const auto draw = [&] {
        ui.beginFrame(static_cast<uint8_t>(screen));
        display.draw(ui, config, standbyMs, nullptr, screen);
        ui.endFrame();
    };
    draw();
    tap(ui, 26, 140, 100, draw);
    TEST_ASSERT_EQUAL_UINT8(5, config.brightnessPercent);
    contact = {true, 100, 140};
    ui.pollTouch(300);
    draw();
    TEST_ASSERT_TRUE(ui.pressedIn({12, 52, 192, 108}));
    contact = {true, 600, 140};
    ui.pollTouch(340);
    draw();
    contact = {};
    ui.pollTouch(380);
    draw();
    TEST_ASSERT_EQUAL_UINT8(100, config.brightnessPercent);
    TEST_ASSERT_TRUE(screen == screens::Screen::Settings);
    TEST_ASSERT_FALSE(ui.pressedIn({12, 52, 192, 108}));
}

void test_library_and_chapter_pages_keep_every_item_accessible() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    screens::LibraryScreen library;
    const std::array<BookLibrary::Entry, 3> books{{{.title = "Первая"}, {.title = "Вторая"}, {.title = "Третья"}}};
    const std::vector<screens::LibraryItem> items{{&books[0]}, {&books[1]}, {&books[2]}};
    auto screen = screens::Screen::Library;
    auto action = screens::Action::None;
    const auto drawLibrary = [&] {
        ui.beginFrame(static_cast<uint8_t>(screen));
        action = library.draw(ui, items, 0, screen);
        ui.endFrame();
    };
    drawLibrary();
    tap(ui, 606, 95, 100, drawLibrary);
    tap(ui, 300, 75, 300, drawLibrary);
    TEST_ASSERT_TRUE(action == screens::Action::OpenBook);
    TEST_ASSERT_EQUAL_UINT32(2, library.selectedIndex());

    screens::ChaptersScreen chapters;
    const std::array<ChapterMarker, 3> markers{{{"Начало", 0}, {"Середина", 5}, {"Конец", 10}}};
    ReadingSession reader;
    screen = screens::Screen::Chapters;
    const auto drawChapters = [&] {
        ui.beginFrame(static_cast<uint8_t>(screen));
        action = chapters.draw(ui, markers, reader, settings::ReadingSettings{}, 0, screen);
        ui.endFrame();
    };
    drawChapters();
    tap(ui, 606, 95, 500, drawChapters);
    tap(ui, 300, 75, 700, drawChapters);
    TEST_ASSERT_TRUE(action == screens::Action::Resume);
    TEST_ASSERT_EQUAL_UINT32(10, reader.state.wordIndex);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_languages_are_complete_and_available_without_sd);
    RUN_TEST(test_embedded_reading_fonts_cover_the_russian_alphabet_in_every_size);
    RUN_TEST(test_imported_hidden_settings_reset_and_visible_settings_survive);
    RUN_TEST(test_settings_draws_display_controls_and_toggles_language_repeatedly);
    RUN_TEST(test_reading_menu_keeps_library_chapters_and_has_no_font_override);
    RUN_TEST(test_boot_identifies_product_and_version);
    RUN_TEST(test_modern_ui_font_and_case_conversion_support_russian);
    RUN_TEST(test_brightness_capture_clamps_a_drag_outside_the_panel);
    RUN_TEST(test_library_and_chapter_pages_keep_every_item_accessible);
    return UNITY_END();
}
