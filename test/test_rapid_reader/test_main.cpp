#include <unity.h>

#include <array>
#include <string>

#include "product/RapidReaderSettings.h"
#include "fonts/LiterataFallbackAlpha4.h"
#include "settings/SettingsCodec.h"
#include "ui/RapidReaderBrand.h"
#include "ui/RapidReaderUi.h"
#include "ui/RapidReaderConnection.h"
#include "ui/RapidReaderTutorial.h"
#include "product/RapidReaderText.h"
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
    const auto swipeLibrary = [&](uint16_t fromX, uint16_t toX, uint32_t time) {
        contact = {true, fromX, 75};
        ui.pollTouch(time);
        drawLibrary();
        contact = {true, toX, 75};
        ui.pollTouch(time + 40);
        drawLibrary();
        contact = {};
        ui.pollTouch(time + 80);
        drawLibrary();
        TEST_ASSERT_TRUE(action == screens::Action::None);
        TEST_ASSERT_EQUAL_UINT32(0, library.selectedIndex());
    };
    drawLibrary();
    tap(ui, 606, 106, 100, drawLibrary);
    TEST_ASSERT_TRUE(action == screens::Action::None);
    tap(ui, 34, 106, 300, drawLibrary);
    TEST_ASSERT_TRUE(action == screens::Action::None);
    tap(ui, 300, 75, 500, drawLibrary);
    TEST_ASSERT_TRUE(action == screens::Action::OpenBook);
    TEST_ASSERT_EQUAL_UINT32(0, library.selectedIndex());
    // Fast swipes only change pages, including at either end of the library.
    swipeLibrary(160, 480, 700);
    swipeLibrary(480, 160, 900);
    swipeLibrary(480, 160, 1100);
    tap(ui, 606, 106, 1300, drawLibrary);
    TEST_ASSERT_TRUE(action == screens::Action::None);
    swipeLibrary(160, 480, 1500);
    tap(ui, 606, 106, 1700, drawLibrary);
    tap(ui, 300, 75, 1900, drawLibrary);
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
    // Plain clicks on the arrow itself turn pages in both directions.
    tap(ui, 606, 106, 2100, drawChapters);
    TEST_ASSERT_TRUE(action == screens::Action::None);
    tap(ui, 34, 106, 2300, drawChapters);
    TEST_ASSERT_TRUE(action == screens::Action::None);
    tap(ui, 300, 75, 2500, drawChapters);
    TEST_ASSERT_TRUE(action == screens::Action::Resume);
    TEST_ASSERT_EQUAL_UINT32(0, reader.state.wordIndex);
    tap(ui, 606, 106, 2700, drawChapters);
    tap(ui, 300, 75, 2900, drawChapters);
    TEST_ASSERT_TRUE(action == screens::Action::Resume);
    TEST_ASSERT_EQUAL_UINT32(10, reader.state.wordIndex);
}

void test_boot_keeps_branding_over_low_level_storage_statuses() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    ui.setBooting(true);
    screens::rapidReaderBoot(ui, product::text(product::Text::LibrarySetup));
    TEST_ASSERT_TRUE(gfx.text.find("RAPIDREADER") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("ПОДГОТОВКА БИБЛИОТЕКИ") != std::string::npos);
    screens::status(ui, "SD", "Mounting card", "EPUB converts on open", 5);
    TEST_ASSERT_TRUE(ui.bootMessage() == "Подготовка библиотеки");
    TEST_ASSERT_TRUE(gfx.text.find("Mounting card") == std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("EPUB converts") == std::string::npos);
    ui.setBooting(false);
    screens::status(ui, "Библиотека", "Книга открыта");
    TEST_ASSERT_TRUE(gfx.text.find("Книга открыта") != std::string::npos);
}

void test_usb_screens_follow_the_device_language_and_real_transfer_counts() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    screens::rapidReaderUsb(ui);
    TEST_ASSERT_TRUE(gfx.text.find("ПОДКЛЮЧЕНО") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Не закрывайте вкладку браузера") != std::string::npos);
    screens::rapidReaderUsb(ui, {.activity = companion::serial::TransferActivity::Receiving,
                                 .completedBytes = 50, .totalBytes = 100, .name = "Война и мир.rsvp"});
    TEST_ASSERT_TRUE(gfx.text.find("ПРИЁМ ДАННЫХ") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Война и мир.rsvp") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("50%") != std::string::npos);
    ui.setLocale("en");
    screens::rapidReaderUsb(ui);
    TEST_ASSERT_TRUE(gfx.text.find("CONNECTED") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Keep the browser tab open") != std::string::npos);
    ui.setLocale("ru");
    screens::rapidReaderUsbStorage(ui, "Mounted read/write");
    TEST_ASSERT_TRUE(gfx.text.find("ЧТЕНИЕ И ЗАПИСЬ") != std::string::npos);
    screens::rapidReaderUsbStorage(ui, "Ejected");
    TEST_ASSERT_TRUE(gfx.text.find("ДИСК ИЗВЛЕЧЁН") != std::string::npos);
}

void test_product_status_messages_and_transfer_progress_are_bounded() {
    for (const auto locale : {"ru", "en"})
        for (size_t i = 0; i < static_cast<size_t>(product::Text::Count); ++i)
            TEST_ASSERT_FALSE(product::text(static_cast<product::Text>(i), locale).empty());
    TEST_ASSERT_TRUE(product::text(product::Text::Connected) == "Подключено");
    companion::serial::TransferStatus transfer;
    TEST_ASSERT_EQUAL_INT(-1, transfer.percentage());
    transfer.totalBytes = 256ULL * 1024 * 1024;
    TEST_ASSERT_EQUAL_INT(0, transfer.percentage());
    transfer.completedBytes = transfer.totalBytes / 2;
    TEST_ASSERT_EQUAL_INT(50, transfer.percentage());
    transfer.completedBytes = UINT64_MAX;
    TEST_ASSERT_EQUAL_INT(100, transfer.percentage());
}

void test_tutorial_first_boot_completion_and_interrupted_restart() {
    using product::TutorialAction;
    product::Tutorial tutorial;
    tutorial.begin(0);
    TEST_ASSERT_TRUE(tutorial.active());
    TEST_ASSERT_FALSE(tutorial.apply(TutorialAction::Previous));
    TEST_ASSERT_EQUAL_UINT8(0, tutorial.step());
    TEST_ASSERT_FALSE(tutorial.apply(TutorialAction::Next));
    TEST_ASSERT_EQUAL_UINT8(1, tutorial.step());
    // An interrupted guide has no saved completion marker.
    tutorial.begin(0);
    TEST_ASSERT_TRUE(tutorial.active());
    TEST_ASSERT_EQUAL_UINT8(0, tutorial.step());
    for (uint8_t i = 1; i < product::Tutorial::kStepCount; ++i) {
        TEST_ASSERT_FALSE(tutorial.apply(TutorialAction::Next));
        TEST_ASSERT_TRUE(tutorial.active());
        TEST_ASSERT_EQUAL_UINT8(i, tutorial.step());
    }
    TEST_ASSERT_TRUE(tutorial.apply(TutorialAction::Next));
    TEST_ASSERT_FALSE(tutorial.active());
    TEST_ASSERT_FALSE(tutorial.apply(TutorialAction::Next));
    tutorial.begin(product::Tutorial::kVersion);
    TEST_ASSERT_FALSE(tutorial.active());
    tutorial.begin(product::Tutorial::kVersion + 1);
    TEST_ASSERT_FALSE(tutorial.active());
}

void test_tutorial_skip_and_back_work_on_every_step() {
    using product::TutorialAction;
    for (uint8_t step = 0; step < product::Tutorial::kStepCount; ++step) {
        product::Tutorial tutorial;
        tutorial.begin(0);
        for (uint8_t i = 0; i < step; ++i)
            tutorial.apply(TutorialAction::Next);
        tutorial.apply(TutorialAction::Previous);
        TEST_ASSERT_EQUAL_UINT8(step == 0 ? 0 : step - 1, tutorial.step());
        TEST_ASSERT_TRUE(tutorial.active());
        TEST_ASSERT_TRUE(tutorial.apply(TutorialAction::Skip));
        TEST_ASSERT_FALSE(tutorial.active());
        TEST_ASSERT_FALSE(tutorial.apply(TutorialAction::Skip));
        tutorial.begin(product::Tutorial::kVersion);
        TEST_ASSERT_FALSE(tutorial.active());
    }
}

void test_tutorial_touch_navigation_and_both_languages() {
    using product::TutorialAction;
    const std::array<std::array<const char*, 4>, 2> titles{{
        {"КНИГИ НА УСТРОЙСТВЕ", "ЧТЕНИЕ И МЕНЮ", "СКОРОСТЬ И НАВИГАЦИЯ", "ЭКРАН ПОД ВАС"},
        {"YOUR BOOKS", "READING AND MENUS", "SPEED AND NAVIGATION", "MAKE IT YOURS"},
    }};
    for (size_t locale = 0; locale < 2; ++locale) {
        RecordingGfx gfx;
        ui::Context ui(gfx);
        ui.setTheme(testTheme());
        ui.setLocale(locale == 0 ? "ru" : "en");
        ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
        product::Tutorial tutorial;
        tutorial.begin(0);
        uint8_t completions = 0;
        const auto draw = [&] {
            if (tutorial.apply(screens::rapidReaderTutorial(ui, tutorial)))
                ++completions;
        };
        draw();
        tap(ui, 390, 146, 100, draw); // Back is disabled at the beginning.
        TEST_ASSERT_EQUAL_UINT8(0, tutorial.step());
        tap(ui, 260, 85, 300, draw); // Body taps do not advance the guide.
        TEST_ASSERT_EQUAL_UINT8(0, tutorial.step());
        for (uint8_t step = 0; step < product::Tutorial::kStepCount; ++step) {
            // App renders the new page on the update following a released tap.
            draw();
            TEST_ASSERT_TRUE(gfx.text.find(titles[locale][step]) != std::string::npos);
            TEST_ASSERT_TRUE(gfx.text.find("0" + std::to_string(step + 1) + " / 04") != std::string::npos);
            tap(ui, 540, 146, 500 + step * 200, draw);
        }
        TEST_ASSERT_EQUAL_UINT8(1, completions);
        TEST_ASSERT_FALSE(tutorial.active());
        TEST_ASSERT_TRUE(gfx.text.find(locale == 0 ? "НАЧАТЬ" : "START READING") != std::string::npos);
        TEST_ASSERT_TRUE(gfx.text.find("Фокус") == std::string::npos);
        TEST_ASSERT_TRUE(gfx.text.find("Wi-Fi") == std::string::npos);
        // A fresh session can also be skipped immediately with its touch button.
        tutorial.begin(0);
        draw();
        tap(ui, 80, 146, 1500, draw);
        TEST_ASSERT_EQUAL_UINT8(2, completions);
        TEST_ASSERT_FALSE(tutorial.active());
    }
}

void test_chapter_swipes_are_bounded_and_do_not_open_a_chapter() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    screens::ChaptersScreen chapters;
    const std::array<ChapterMarker, 5> markers{{{"Первая", 0}, {"Вторая", 5},
        {"Третья", 10}, {"Четвёртая", 15}, {"Пятая", 20}}};
    ReadingSession reader;
    auto screen = screens::Screen::Chapters;
    auto action = screens::Action::None;
    const auto draw = [&] {
        ui.beginFrame(static_cast<uint8_t>(screen));
        action = chapters.draw(ui, markers, reader, settings::ReadingSettings{}, 0, screen);
        ui.endFrame();
    };
    uint32_t time = 100;
    const auto swipe = [&](uint16_t fromX, uint16_t toX) {
        contact = {true, fromX, 75};
        ui.pollTouch(time);
        draw();
        contact = {true, toX, 75};
        ui.pollTouch(time + 40);
        draw();
        contact = {};
        ui.pollTouch(time + 80);
        draw();
        time += 200;
        TEST_ASSERT_TRUE(action == screens::Action::None);
        TEST_ASSERT_EQUAL_UINT32(0, reader.state.wordIndex);
    };
    draw();
    // Swiping right at the first page does not wrap around.
    swipe(160, 480);
    tap(ui, 300, 75, time, draw);
    time += 200;
    TEST_ASSERT_TRUE(action == screens::Action::Resume);
    TEST_ASSERT_EQUAL_UINT32(0, reader.state.wordIndex);
    swipe(480, 160);
    TEST_ASSERT_TRUE(gfx.text.find("2 / 3") != std::string::npos);
    swipe(480, 160);
    TEST_ASSERT_TRUE(gfx.text.find("3 / 3") != std::string::npos);
    // A further swipe and a tap on the disabled edge stay on the last page.
    swipe(480, 160);
    tap(ui, 606, 95, time, draw);
    time += 200;
    TEST_ASSERT_TRUE(action == screens::Action::None);
    tap(ui, 300, 75, time, draw);
    TEST_ASSERT_TRUE(action == screens::Action::Resume);
    TEST_ASSERT_EQUAL_UINT32(20, reader.state.wordIndex);
}

void test_swipe_arrow_opacity_follows_available_directions() {
    class ArrowGfx : public RecordingGfx {
    public:
        uint16_t left = 0, right = 0;
        void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) override {
            // Observe only side indicators, outside the chapter rows and header.
            if (y1 >= 52 && y2 >= 52) {
                if (x1 < 60 && x2 < 60)
                    left = color;
                if (x1 > 580 && x2 > 580)
                    right = color;
            }
            RecordingGfx::drawLine(x1, y1, x2, y2, color);
        }
    } gfx;
    ui::Context ui(gfx);
    ui.setTheme(testTheme());
    ui::CarouselGesture gesture;
    size_t page = 0;
    const auto draw = [&](size_t count) {
        ui.invalidate();
        ui.beginFrame(1);
        screens::rapid::listPaging(ui, count, page, gesture, screens::rapid::PagingStyle::SwipeArrows);
        ui.endFrame();
    };
    draw(5);
    const auto dim = gfx.left;
    const auto bright = gfx.right;
    TEST_ASSERT_NOT_EQUAL(dim, bright);
    page = 1;
    draw(5);
    TEST_ASSERT_EQUAL_UINT16(bright, gfx.left);
    TEST_ASSERT_EQUAL_UINT16(bright, gfx.right);
    page = 2;
    draw(5);
    TEST_ASSERT_EQUAL_UINT16(bright, gfx.left);
    TEST_ASSERT_EQUAL_UINT16(dim, gfx.right);
    draw(1);
    TEST_ASSERT_EQUAL_UINT32(0, page);
    TEST_ASSERT_EQUAL_UINT16(dim, gfx.left);
    TEST_ASSERT_EQUAL_UINT16(dim, gfx.right);
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
    RUN_TEST(test_boot_keeps_branding_over_low_level_storage_statuses);
    RUN_TEST(test_usb_screens_follow_the_device_language_and_real_transfer_counts);
    RUN_TEST(test_product_status_messages_and_transfer_progress_are_bounded);
    RUN_TEST(test_tutorial_first_boot_completion_and_interrupted_restart);
    RUN_TEST(test_tutorial_skip_and_back_work_on_every_step);
    RUN_TEST(test_tutorial_touch_navigation_and_both_languages);
    RUN_TEST(test_chapter_swipes_are_bounded_and_do_not_open_a_chapter);
    RUN_TEST(test_swipe_arrow_opacity_follows_available_directions);
    return UNITY_END();
}
