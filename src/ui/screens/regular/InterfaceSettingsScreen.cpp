#include "ui/screens/ScreenCommon.h"

#include "localization/LocaleCatalog.h"

namespace screens {
    namespace {

        std::string_view nextLocale(const locales::Catalog& catalog, std::string_view current) {
#ifdef RAPID_READER
            return current == "ru" ? "en" : "ru";
#else
            bool returnNext = current == Localization::kDefaultLocale;
            for (const auto& pack: catalog) {
                if (returnNext)
                    return pack.locale;
                returnNext = pack.locale == current;
            }
            return Localization::kDefaultLocale;
#endif
        }

    } // namespace

    bool InterfaceScreen::draw(ui::Context& ui, settings::InterfaceSettings& config,
                               std::span<const uint32_t> standbyDurations, void (*setBrightness)(uint8_t),
                               Screen& screen) {
        bool changed = false;
#ifdef RAPID_READER
        navigationAction = detail::navigation(ui, Screen::Settings, screen);
        const ui::Rect content = detail::tabContent(ui);
#else
        const ui::Rect content = detail::content(ui);
#endif
        constexpr int16_t gap = 6;
#ifdef RAPID_READER
        constexpr int16_t backWidth = 0;
#else
        constexpr int16_t backWidth = 56;
#endif
        constexpr int16_t topHeight = 42;
#ifndef RAPID_READER
        if (ui.button({content.x, content.y, backWidth, topHeight}, "<<"))
            screen = Screen::Settings;
#endif
        if (ui.slider({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                       static_cast<int16_t>(content.w - backWidth - gap), topHeight},
                      ui.text(UiText::Brightness), config.brightnessPercent, "%")) {
            if (setBrightness != nullptr)
                setBrightness(config.brightnessPercent);
            changed = true;
        }

        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        const int16_t sectionsY = static_cast<int16_t>(content.y + topHeight + 4);
        ui.separator({content.x, sectionsY, halfWidth, 10}, ui.text(UiText::AppearanceSection));
        ui.separator({static_cast<int16_t>(content.x + halfWidth + gap), sectionsY, halfWidth, 10},
                     ui.text(UiText::StandbySection));

        const int16_t firstRowY = static_cast<int16_t>(sectionsY + 14);
        constexpr int16_t rowHeight = 40;
        const int16_t secondRowY = static_cast<int16_t>(firstRowY + rowHeight + 4);
        const ui::themes::Theme& selectedTheme = themes.resolve(config.selectedThemeId);
        if (ui.setting({content.x, firstRowY, halfWidth, rowHeight}, ui.text(UiText::Theme),
                       selectedTheme.definition.name, ui::SettingLayout::Inline)) {
            const ui::themes::Theme& nextTheme = themes.next(config.selectedThemeId);
            config.selectedThemeId = nextTheme.id;
            ui.setTheme(nextTheme);
            changed = true;
        }

        if (ui.setting({content.x, secondRowY, halfWidth, rowHeight}, ui.text(UiText::Language),
#ifdef RAPID_READER
                       config.locale == "en" ? "English" : "Русский",
#else
                       languages_ ? locales::localeName(*languages_, config.locale) : std::string_view{config.locale},
#endif
                       ui::SettingLayout::Inline)) {
#ifdef RAPID_READER
            config.locale = config.locale == "ru" ? "en" : "ru";
#else
            config.locale = !languages_ ? std::string{Localization::kDefaultLocale}
                                        : std::string{nextLocale(*languages_, config.locale)};
#endif
            ui.setLocale(config.locale);
            changed = true;
        }

        std::string standby{ui.text(UiText::Off)};
        const size_t standbyIndex = config.standbyTimerIndex;
        if (standbyIndex < standbyDurations.size() && standbyDurations[standbyIndex] != 0) {
            standby = std::to_string(standbyDurations[standbyIndex] / 60000UL)
#ifdef RAPID_READER
                    + (config.locale == "ru" ? " мин" : " min");
#else
                    + "m";
#endif
        }
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), firstRowY, halfWidth, rowHeight},
                       ui.text(UiText::Standby), standby, ui::SettingLayout::Inline)) {
            config.standbyTimerIndex.cycle();
            changed = true;
        }

        const UiText screensaver = config.screensaver == standby::Kind::maze      ? UiText::Maze
                                 : config.screensaver == standby::Kind::voronoi   ? UiText::Voronoi
                                 : config.screensaver == standby::Kind::reaction  ? UiText::Reaction
                                 : config.screensaver == standby::Kind::screenOff ? UiText::ScreenOff
                                                                                  : UiText::Life;
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), secondRowY, halfWidth, rowHeight},
                       ui.text(UiText::Screensaver), ui.text(screensaver), ui::SettingLayout::Inline)) {
            config.screensaver = settings::cycleEnum(config.screensaver);
            changed = true;
        }
        return changed;
    }

} // namespace screens
