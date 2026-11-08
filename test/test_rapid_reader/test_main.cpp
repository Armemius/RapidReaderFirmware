#include <unity.h>

#include <array>
#include <string>

#include "product/RapidReaderSettings.h"
#include "settings/SettingsCodec.h"
#include "ui/RapidReaderBrand.h"
#include "ui/screens/ScreenCommon.h"

// The host filesystem shim has no SD directories. Supply its single built-in
// theme; the firmware uses the real ThemeStore implementation.
const ui::themes::Theme& ThemeStore::resolve(std::string_view) const { return themes_.front(); }
const ui::themes::Theme& ThemeStore::next(std::string_view) const { return themes_.front(); }

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
    ui.setTheme(ui::themes::defaultTheme());
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    const auto draw = [&] {
        ui.beginFrame(static_cast<uint8_t>(screens::Screen::Settings));
        display.draw(ui, config, standbyMs, nullptr, screen);
        ui.endFrame();
    };
    draw();
    TEST_ASSERT_TRUE(gfx.text.find("Яркость") != std::string::npos);
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
    tap(ui, 80, 40, 1800, draw);
    TEST_ASSERT_TRUE(screen == screens::Screen::Read);
}

void test_reading_menu_keeps_library_chapters_and_has_no_font_override() {
    RecordingGfx gfx;
    ui::Context ui(gfx);
    ui.setTheme(ui::themes::defaultTheme());
    auto screen = screens::Screen::Read;
    ui.beginFrame(static_cast<uint8_t>(screen));
    screens::read(ui, "Война и мир", "Лев Толстой", 42, screen);
    ui.endFrame();
    TEST_ASSERT_TRUE(gfx.text.find("Война и мир") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Главы") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("Библиотека") != std::string::npos);
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
    ui.setTheme(ui::themes::defaultTheme());
    screens::rapidReaderBoot(ui);
    TEST_ASSERT_TRUE(gfx.text.find("RapidReader") != std::string::npos);
    TEST_ASSERT_TRUE(gfx.text.find("rapid-reader-0.1.0") != std::string::npos);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_languages_are_complete_and_available_without_sd);
    RUN_TEST(test_imported_hidden_settings_reset_and_visible_settings_survive);
    RUN_TEST(test_settings_draws_display_controls_and_toggles_language_repeatedly);
    RUN_TEST(test_reading_menu_keeps_library_chapters_and_has_no_font_override);
    RUN_TEST(test_boot_identifies_product_and_version);
    return UNITY_END();
}
