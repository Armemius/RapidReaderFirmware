#include "ui/RapidReaderBrand.h"

#include "product/RapidReader.h"
#include "ui/RapidReaderUi.h"
#include <cstdlib>

namespace screens {
    namespace {
        // RR monogram from RSVPConverter/public/favicon.svg, in its 48x48 viewBox.
        // Keep it as vectors: no SD dependency and no framebuffer-sized image.
        void logo(Arduino_GFX& output, ui::Rect rect) {
            const auto red = ui::themes::rgb565(230, 36, 64);
            const auto white = ui::themes::rgb565(255, 240, 237);
            output.fillRect(rect.x, rect.y, rect.w, rect.h, red);
            const auto point = [&](float x, float y) {
                return std::pair{static_cast<int16_t>(rect.x + x * rect.w / 48),
                                 static_cast<int16_t>(rect.y + y * rect.h / 48)};
            };
            const auto line = [&](float x1, float y1, float x2, float y2) {
                const auto [ax, ay] = point(x1, y1);
                const auto [bx, by] = point(x2, y2);
                const int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
                for (int i = 0; i <= steps; ++i) {
                    const float t = steps == 0 ? 0 : static_cast<float>(i) / steps;
                    output.fillCircle(static_cast<int16_t>(ax + (bx - ax) * t),
                                      static_cast<int16_t>(ay + (by - ay) * t),
                                      std::max<int16_t>(1, rect.w / 32), white);
                }
            };
            const auto bowl = [&](float x) {
                float px = x, py = 11;
                for (int i = 1; i <= 24; ++i) {
                    const float t = i / 24.0f, u = 1 - t;
                    const float nx = u*u*u*x + 3*u*u*t*(x+9) + 3*u*t*t*(x+10) + t*t*t*x;
                    const float ny = u*u*u*11 + 3*u*u*t*11 + 3*u*t*t*24 + t*t*t*24;
                    line(px, py, nx, ny);
                    px = nx;
                    py = ny;
                }
            };
            line(10, 36, 10, 11);
            line(10, 11, 20, 11);
            bowl(20);
            line(20, 24, 10, 24);
            line(20, 24, 30, 36);
            line(27, 11, 31, 11);
            bowl(31);
            line(31, 24, 27, 24);
            line(31, 24, 40, 36);
        }
    } // namespace

    void rapidReaderBoot(ui::Context& ui) {
        ui.invalidate();
        ui.beginFrame(static_cast<uint8_t>(Screen::Status));
        const int16_t x = static_cast<int16_t>((ui.width() - 456) / 2);
        const int16_t y = static_cast<int16_t>((ui.height() - 80) / 2);
        const ui::Rect mark{x, y, 72, 72};
        if (ui.redraw(mark, ui::Context::signature(product::kName)))
            ui.paint(mark, [](Arduino_GFX& output, ui::Rect rect) { logo(output, rect); });
        ui.label({static_cast<int16_t>(x + 96), y, 336, 42}, "RAPIDREADER", 2,
                 ui::themes::Foreground);
        ui.label({static_cast<int16_t>(x + 98), static_cast<int16_t>(y + 47), 336, 24}, product::kVersion, 1,
                 ui::themes::Muted);
        const ui::Rect detail{24, static_cast<int16_t>(ui.height() - 24), static_cast<int16_t>(ui.width() - 48), 2};
        if (ui.redraw(detail, 1))
            ui.paint(detail, [&](Arduino_GFX& output, ui::Rect r) {
                output.fillRect(r.x, r.y, r.w, r.h, ui.color(ui::themes::Outline));
                output.fillRect(r.x, r.y, 64, r.h, ui.color(ui::themes::Accent));
            });
        ui.endFrame();
    }
} // namespace screens
