#include "ui/screens/Screens.h"
#ifdef RAPID_READER
#include "ui/RapidReaderUi.h"
#include "ui/RapidReaderBrand.h"
#endif

namespace screens {

    void status(ui::Context& ui, std::string_view title, std::string_view line1, std::string_view line2,
                int progressValue) {
#ifdef RAPID_READER
        if (ui.booting()) {
            rapidReaderBoot(ui);
            return;
        }
#endif
        ui.beginFrame(static_cast<uint8_t>(Screen::Status));
#ifdef RAPID_READER
        ui.label({24, 0, 420, 38}, "RAPIDREADER", 1, ui::themes::Muted);
        const auto area = rapid::content(ui);
        ui.label({area.x, area.y, static_cast<int16_t>(area.w - 90), 24}, rapid::uppercase(title), 1, ui::themes::Accent);
        if (progressValue >= 0)
            ui.label({static_cast<int16_t>(area.x + area.w - 80), area.y, 80, 24},
                     std::to_string(progressValue) + "%", 1, ui::themes::Foreground, ui::TextAlign::Right);
        ui.label({area.x, static_cast<int16_t>(area.y + 26), area.w, 40}, line1, 2,
                 ui::themes::Foreground, ui::TextAlign::Left, 2);
        ui.label({area.x, static_cast<int16_t>(area.y + 68), area.w, 24}, line2, 1, ui::themes::Muted);
        const ui::Rect track{area.x, static_cast<int16_t>(area.y + area.h - 2), area.w, 2};
        if (ui.redraw(track, progressValue + 2))
            ui.paint(track, [&](Arduino_GFX& output, ui::Rect r) {
                output.fillRect(r.x, r.y, r.w, r.h, ui.color(ui::themes::ProgressTrack));
                if (progressValue >= 0)
                    output.fillRect(r.x, r.y, r.w * std::clamp(progressValue, 0, 100) / 100, r.h, ui.color(ui::themes::Accent));
            });
#else
        const int16_t width = std::min<int16_t>(static_cast<int16_t>(ui.width() - 24), 560);
        const int16_t x = static_cast<int16_t>((ui.width() - width) / 2);
        const int16_t totalHeight = static_cast<int16_t>(48 + (!line1.empty() ? 52 : 0) + (!line2.empty() ? 40 : 0)
                                                         + (progressValue >= 0 ? 22 : 0));
        int16_t y = std::max<int16_t>(8, static_cast<int16_t>((ui.height() - totalHeight) / 2));

        ui.label({x, y, width, 48}, title, 3, ui::themes::ColorRole::Accent, ui::TextAlign::Center, 2);
        y = static_cast<int16_t>(y + 48);
        if (!line1.empty()) {
            y = static_cast<int16_t>(y + 4);
            ui.label({x, y, width, 48}, line1, 3, ui::themes::ColorRole::Foreground, ui::TextAlign::Center, 2);
            y = static_cast<int16_t>(y + 48);
        }
        if (!line2.empty()) {
            y = static_cast<int16_t>(y + 4);
            ui.label({x, y, width, 36}, line2, 2, ui::themes::ColorRole::Muted, ui::TextAlign::Center, 2);
            y = static_cast<int16_t>(y + 36);
        }
        if (progressValue >= 0) {
            const int16_t progressWidth = std::min<int16_t>(width, 480);
            ui.progress({static_cast<int16_t>((ui.width() - progressWidth) / 2), static_cast<int16_t>(y + 8),
                         progressWidth, 14},
                        progressValue);
        }
#endif
        ui.endFrame();
    }

} // namespace screens
