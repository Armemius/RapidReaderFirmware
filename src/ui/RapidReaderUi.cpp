#include "ui/RapidReaderUi.h"

#include "text/Utf8Text.h"

namespace screens::rapid {
    namespace {
        using namespace ui::themes;

        bool pressed(ui::Context& ui, ui::Rect rect) {
            return ui.pressedIn(rect);
        }

        void panel(Arduino_GFX& output, ui::Rect rect, ui::Context& ui, bool active = false) {
            output.fillRect(rect.x, rect.y, rect.w, rect.h, ui.color(active ? SurfaceActive : Surface));
            output.drawRect(rect.x, rect.y, rect.w, rect.h, ui.color(active ? Accent : Outline));
        }

        void chevron(Arduino_GFX& output, int16_t x, int16_t y, uint16_t color) {
            output.drawLine(x, y - 4, x + 4, y, color);
            output.drawLine(x + 4, y, x, y + 4, color);
        }

        void swipeArrow(ui::Context& ui, ui::Rect rect, bool right, bool available) {
            const bool down = available && pressed(ui, rect);
            const uint8_t alpha = !available ? 36 : down ? 255 : 208;
            const auto state = ui::Context::combine(alpha, right);
            if (ui.redraw(rect, state))
                ui.paint(rect, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(Background));
                    const int16_t cx = r.x + r.w / 2;
                    const int16_t cy = r.y + r.h / 2;
                    const int16_t halfHeight = std::min<int16_t>(36, (r.h - 16) / 2);
                    const int16_t tip = cx + (right ? 10 : -10);
                    const int16_t tail = cx + (right ? -10 : 10);
                    const auto ink = ui.blend(Accent, alpha);
                    // A slightly wider chevron with a two-pixel horizontal stroke.
                    // The full-height touch target remains independent of the drawing.
                    for (int16_t offset = 0; offset < 2; ++offset) {
                        output.drawLine(tail + offset, cy - halfHeight, tip + offset, cy, ink);
                        output.drawLine(tip + offset, cy, tail + offset, cy + halfHeight, ink);
                    }
                });
        }

        bool tab(ui::Context& ui, ui::Rect rect, std::string_view code, std::string_view title, bool active) {
            const bool hit = ui.tap(rect);
            const bool down = pressed(ui, rect);
            auto state = ui::Context::combine(ui::Context::signature(title), active | (down << 1));
            if (ui.redraw(rect, state)) {
                const auto number = ui.prepareText({10, 0, 20, rect.h}, code, 1);
                const auto label = ui.prepareText({38, 0, static_cast<int16_t>(rect.w - 44), rect.h}, uppercase(title), 1);
                ui.paint(rect, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(active || down ? SurfaceActive : Background));
                    if (active)
                        output.fillRect(r.x + 8, r.y + r.h - 3, r.w - 16, 2, ui.color(Accent));
                    ui.drawText(output, number, ui.color(active ? Accent : Subtle), r.x, r.y);
                    ui.drawText(output, label, ui.color(active ? Foreground : Muted), r.x, r.y);
                });
            }
            return hit;
        }
    } // namespace

    std::string uppercase(std::string_view text) {
        std::string result;
        result.reserve(text.size());
        uint32_t cp;
        while (Utf8Text::next(text, cp)) {
            if ((cp >= 'a' && cp <= 'z') || (cp >= 0x430 && cp <= 0x44f))
                cp -= 32;
            else if (cp == 0x451)
                cp = 0x401;
            Utf8Text::append(result, cp);
        }
        return result;
    }

    ui::Rect content(ui::Context& ui) {
        return {12, 52, static_cast<int16_t>(ui.width() - 24), static_cast<int16_t>(ui.height() - 64)};
    }

    Action navigation(ui::Context& ui, Screen active, Screen& screen) {
        const auto heading = active == Screen::Library ? uppercase(ui.text(UiText::Library))
                           : active == Screen::Chapters ? uppercase(ui.text(UiText::Chapters))
                                                        : std::string{"RAPIDREADER"};
        ui.label({24, 0, 154, 42}, heading, 1, Foreground);
        const ui::Rect marker{12, 15, 4, 12};
        if (ui.redraw(marker, 1))
            ui.paint(marker, [&](Arduino_GFX& output, ui::Rect r) {
                output.fillRect(r.x, r.y, r.w, r.h, ui.color(Accent));
            });
        const ui::Rect rule{12, 43, static_cast<int16_t>(ui.width() - 24), 1};
        if (ui.redraw(rule, 1))
            ui.paint(rule, [&](Arduino_GFX& output, ui::Rect r) {
                output.fillRect(r.x, r.y, r.w, r.h, ui.color(Outline));
            });
        const bool settings = active == Screen::Settings || active == Screen::InterfaceSettings;
        if (tab(ui, {static_cast<int16_t>(ui.width() - 344), 0, 136, 42}, "01", ui.text(UiText::Read), !settings))
            screen = Screen::Read;
        if (tab(ui, {static_cast<int16_t>(ui.width() - 204), 0, 148, 42}, "02", ui.text(UiText::Settings), settings))
            screen = Screen::Settings;
        const ui::Rect power{static_cast<int16_t>(ui.width() - 48), 0, 44, 42};
        const bool hit = ui.tap(power);
        const bool down = pressed(ui, power);
        if (ui.redraw(power, down))
            ui.paint(power, [&](Arduino_GFX& output, ui::Rect r) {
                const auto background = ui.color(down ? SurfaceActive : Background);
                const auto ink = ui.color(down ? Accent : Muted);
                output.fillRect(r.x, r.y, r.w, r.h, background);
                output.drawCircle(r.x + 22, r.y + 21, 7, ink);
                output.fillRect(r.x + 19, r.y + 11, 7, 7, background);
                output.drawFastVLine(r.x + 22, r.y + 11, 10, ink);
            });
        return hit ? Action::PowerOff : Action::None;
    }

    bool action(ui::Context& ui, ui::Rect rect, std::string_view title, std::string_view detail,
                std::string_view trailing, bool selected, bool enabled) {
        const bool hit = ui.tap(rect, enabled);
        const bool down = enabled && pressed(ui, rect);
        auto state = ui::Context::signature(title, ui::Context::signature(detail, ui::Context::signature(trailing)));
        state = ui::Context::combine(state, selected | (down << 1) | (enabled << 2));
        if (ui.redraw(rect, state)) {
            const bool compact = rect.w < 80;
            const int16_t right = compact ? 0 : trailing.empty() ? 28 : 66;
            const int16_t left = compact ? 0 : 14;
            const auto text = ui.prepareText({left, 0, static_cast<int16_t>(rect.w - left - right),
                                             static_cast<int16_t>(detail.empty() ? rect.h : rect.h - 19)}, title, 1,
                                             compact ? ui::TextAlign::Center : ui::TextAlign::Left);
            const auto sub = ui.prepareText({14, static_cast<int16_t>(rect.h - 24),
                                            static_cast<int16_t>(rect.w - 80), 20}, detail, 1);
            const auto value = ui.prepareText({static_cast<int16_t>(rect.w - 70), 0, 56, rect.h}, trailing, 1,
                                              ui::TextAlign::Right);
            ui.paint(rect, [&](Arduino_GFX& output, ui::Rect r) {
                if (!enabled && title.empty()) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(Background));
                    return;
                }
                panel(output, r, ui, down);
                if (selected)
                    output.fillRect(r.x, r.y + 1, 3, r.h - 2, ui.color(Accent));
                ui.drawText(output, text, ui.color(enabled ? Foreground : Subtle), r.x, r.y);
                ui.drawText(output, sub, ui.color(Muted), r.x, r.y);
                ui.drawText(output, value, ui.color(selected ? Accent : Muted), r.x, r.y);
                if (enabled && trailing.empty() && !compact)
                    chevron(output, r.x + r.w - 18, r.y + r.h / 2, ui.color(Accent));
            });
        }
        return hit;
    }

    bool choice(ui::Context& ui, ui::Rect rect, std::string_view label, std::string_view value) {
        const bool hit = ui.tap(rect);
        const bool down = pressed(ui, rect);
        auto state = ui::Context::combine(ui::Context::signature(label, ui::Context::signature(value)), down);
        if (ui.redraw(rect, state)) {
            const auto name = ui.prepareText({12, 4, static_cast<int16_t>(rect.w - 24), 20}, uppercase(label), 1);
            const auto selected = ui.prepareText({12, 25, static_cast<int16_t>(rect.w - 40), 20}, value, 1);
            ui.paint(rect, [&](Arduino_GFX& output, ui::Rect r) {
                panel(output, r, ui, down);
                ui.drawText(output, name, ui.color(Muted), r.x, r.y);
                ui.drawText(output, selected, ui.color(Foreground), r.x, r.y);
                chevron(output, r.x + r.w - 18, r.y + 34, ui.color(Accent));
            });
        }
        return hit;
    }

    Action read(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress, Screen& screen) {
        if (const auto action = navigation(ui, Screen::Read, screen); action != Action::None)
            return action;
        const auto area = content(ui);
        const int16_t actionsWidth = 200;
        const ui::Rect book{area.x, area.y, static_cast<int16_t>(area.w - actionsWidth - 12), area.h};
        const bool resume = ui.tap(book);
        const bool down = pressed(ui, book);
        auto state = ui::Context::signature(title, ui::Context::signature(author));
        state = ui::Context::combine(state, progress | (down << 8));
        if (ui.redraw(book, state)) {
            const auto caption = ui.prepareText({14, 5, static_cast<int16_t>(book.w - 28), 20},
                                                uppercase(ui.text(UiText::Resume)), 1);
            const auto name = ui.prepareText({14, 28, static_cast<int16_t>(book.w - 42), 40}, title, 2,
                                             ui::TextAlign::Left, 2);
            const auto byline = ui.prepareText({14, 70, static_cast<int16_t>(book.w - 90), 20}, author, 1);
            const auto percent = ui.prepareText({static_cast<int16_t>(book.w - 68), 70, 54, 20},
                                                std::to_string(progress) + "%", 1, ui::TextAlign::Right);
            ui.paint(book, [&](Arduino_GFX& output, ui::Rect r) {
                panel(output, r, ui, down);
                output.drawFastHLine(r.x, r.y, 36, ui.color(Accent));
                ui.drawText(output, caption, ui.color(Accent), r.x, r.y);
                ui.drawText(output, name, ui.color(Foreground), r.x, r.y);
                ui.drawText(output, byline, ui.color(Muted), r.x, r.y);
                ui.drawText(output, percent, ui.color(Foreground), r.x, r.y);
                const int16_t length = r.w - 28;
                output.fillRect(r.x + 14, r.y + r.h - 10, length, 2, ui.color(ProgressTrack));
                output.fillRect(r.x + 14, r.y + r.h - 10, length * std::min<int>(progress, 100) / 100, 2,
                                ui.color(Accent));
                chevron(output, r.x + r.w - 21, r.y + 47, ui.color(Accent));
            });
        }
        const int16_t x = area.x + area.w - actionsWidth;
        const int16_t height = (area.h - 8) / 2;
        if (action(ui, {x, area.y, actionsWidth, height}, uppercase(ui.text(UiText::Library))))
            screen = Screen::Library;
        if (action(ui, {x, static_cast<int16_t>(area.y + height + 8), actionsWidth, height},
                   uppercase(ui.text(UiText::Chapters))))
            screen = Screen::Chapters;
        return resume ? Action::Resume : Action::None;
    }

    bool display(ui::Context& ui, settings::InterfaceSettings& config, ThemeStore& themes,
                 std::span<const uint32_t> durations, void (*brightness)(uint8_t)) {
        const auto area = content(ui);
        bool changed = false;
        if (ui.slider({area.x, area.y, 192, area.h}, uppercase(ui.text(UiText::Brightness)), config.brightnessPercent, "%")) {
            if (brightness)
                brightness(config.brightnessPercent);
            changed = true;
        }
        const int16_t x = area.x + 204;
        const int16_t width = (area.w - 212) / 2;
        const int16_t height = (area.h - 8) / 2;
        const ui::Rect theme{x, area.y, width, height};
        const ui::Rect language{x, static_cast<int16_t>(area.y + height + 8), width, height};
        const ui::Rect standby{static_cast<int16_t>(x + width + 8), area.y, width, height};
        const ui::Rect saver{standby.x, language.y, width, height};
        if (choice(ui, theme, ui.text(UiText::Theme), themes.resolve(config.selectedThemeId).definition.name)) {
            const auto& selected = themes.next(config.selectedThemeId);
            config.selectedThemeId = selected.id;
            ui.setTheme(selected);
            changed = true;
        }
        if (choice(ui, language, ui.text(UiText::Language), config.locale == "ru" ? "Русский" : "English")) {
            config.locale = config.locale == "ru" ? "en" : "ru";
            ui.setLocale(config.locale);
            changed = true;
        }
        const size_t index = config.standbyTimerIndex;
        const std::string duration = index < durations.size() && durations[index] != 0
            ? std::to_string(durations[index] / 60000) + (config.locale == "ru" ? " мин" : " min")
            : std::string{ui.text(UiText::Off)};
        if (choice(ui, standby, ui.text(UiText::Standby), duration)) {
            config.standbyTimerIndex.cycle();
            changed = true;
        }
        const auto name = config.screensaver == standby::Kind::maze ? UiText::Maze
                        : config.screensaver == standby::Kind::voronoi ? UiText::Voronoi
                        : config.screensaver == standby::Kind::reaction ? UiText::Reaction
                        : config.screensaver == standby::Kind::screenOff ? UiText::ScreenOff : UiText::Life;
        if (choice(ui, saver, ui.text(UiText::Screensaver), ui.text(name))) {
            config.screensaver = settings::cycleEnum(config.screensaver);
            changed = true;
        }
        return changed;
    }

    ui::Rect listRow(ui::Context& ui, size_t row) {
        const auto area = content(ui);
        const int16_t height = (area.h - 8) / 2;
        return {68, static_cast<int16_t>(area.y + row * (height + 8)), static_cast<int16_t>(area.w - 112), height};
    }

    bool listPaging(ui::Context& ui, size_t count, size_t& page, ui::CarouselGesture& gesture, PagingStyle style) {
        const auto area = content(ui);
        const size_t pages = std::max<size_t>(1, (count + 1) / 2);
        page = std::min(page, pages - 1);
        const auto old = page;
        const int delta = gesture.update(ui.touch(), area);
        if (delta != 0)
            ui.cancelTap();
        if (delta < 0 && page > 0)
            --page;
        else if (delta > 0 && page + 1 < pages)
            ++page;
        const ui::Rect previous{area.x, area.y, 44, area.h};
        const ui::Rect next{static_cast<int16_t>(area.x + area.w - 44), area.y, 44, area.h};
        if (style == PagingStyle::SwipeArrows) {
            const bool back = ui.tap(previous, page > 0);
            const bool forward = ui.tap(next, page + 1 < pages);
            if (back)
                --page;
            else if (forward)
                ++page;
            swipeArrow(ui, previous, false, page > 0);
            swipeArrow(ui, next, true, page + 1 < pages);
        } else {
            if (action(ui, previous, "<", {}, {}, false, page > 0))
                --page;
            if (action(ui, next, ">", {}, {}, false, page + 1 < pages))
                ++page;
        }
        ui.label({190, 0, 84, 42}, std::to_string(page + 1) + " / " + std::to_string(pages), 1,
                 ui::themes::Muted, ui::TextAlign::Right);
        return page != old;
    }
} // namespace screens::rapid
