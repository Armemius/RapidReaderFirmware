#include "ui/RapidReaderTubeList.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "ui/RapidReaderUi.h"

namespace screens::rapid {
    namespace {
        constexpr int kWidth = 704;
        constexpr int kHeight = 56;
        constexpr float kStepAngle = 1.04719755f; // Six flat faces, 60 degrees apart.
        constexpr float kCamera = 8.0f;
        constexpr float kHalfFace = 0.53693575f; // tan(30 degrees), inset for a panel gap.
        constexpr uint32_t kSnapMs = 120;

        float perspective(float angle, float localY = 0) {
            return (kCamera - 1) / (kCamera - std::cos(angle) + localY * std::sin(angle));
        }
        float worldScale(ui::Rect view) {
            // The three visible faces span the content area. The central face is
            // about 53 px high on Rev2, larger than the previous curved picker.
            return (view.h - 8) / (2.0f * 1.15470054f * (kCamera - 1) / kCamera);
        }
        float projectedY(float angle, float localY, ui::Rect view) {
            const float worldY = std::sin(angle) + localY * std::cos(angle);
            return view.y + view.h * 0.5f + worldScale(view) * worldY * perspective(angle, localY);
        }
        struct FaceLine {
            int sourceY;
            float scale;
        };
        // Invert the planar perspective transform for one face. Unlike a cylinder,
        // each panel has straight edges; depth only changes linearly across its face.
        FaceLine faceLine(float angle, int y, ui::Rect view) {
            const float sine = std::sin(angle), cosine = std::cos(angle);
            const float q = (y + 0.5f - view.y - view.h * 0.5f) / worldScale(view);
            const float localY = (q * (kCamera - cosine) - (kCamera - 1) * sine)
                               / ((kCamera - 1) * cosine - q * sine);
            return {static_cast<int>(std::floor((localY / (2 * kHalfFace) + 0.5f) * kHeight)),
                    perspective(angle, localY)};
        }

        // One shared 9.625 KiB, two-bit texture, reused for the visible rows.
        // Draw the real Cyrillic UI font into it, then project its scanlines.
        class TubeTexture final : public Arduino_GFX {
        public:
            TubeTexture() : Arduino_GFX(kWidth, kHeight) {}
            bool begin(int32_t = -1) override { return true; }
            void clear() { pixels_.fill(0); }
            uint8_t pixel(int x, int y) const {
                const size_t index = static_cast<size_t>(y * kWidth + x);
                return (pixels_[index / 4] >> ((index & 3) * 2)) & 3;
            }
            void writePixelPreclipped(int16_t x, int16_t y, uint16_t ink) override {
                if (x < 0 || x >= kWidth || y < 0 || y >= kHeight)
                    return;
                const size_t index = static_cast<size_t>(y * kWidth + x);
                const uint8_t shift = (index & 3) * 2;
                pixels_[index / 4] = (pixels_[index / 4] & ~(3U << shift)) | ((ink & 3) << shift);
            }
            void writeFastHLine(int16_t x, int16_t y, int16_t width, uint16_t ink) override {
                for (int i = std::max<int>(0, x); i < std::min<int>(kWidth, x + width); ++i)
                    writePixelPreclipped(i, y, ink);
            }
            void writeFastVLine(int16_t x, int16_t y, int16_t height, uint16_t ink) override {
                for (int i = std::max<int>(0, y); i < std::min<int>(kHeight, y + height); ++i)
                    writePixelPreclipped(x, i, ink);
            }
            void writeFillRectPreclipped(int16_t x, int16_t y, int16_t width, int16_t height,
                                         uint16_t ink) override {
                for (int i = std::max<int>(0, y); i < std::min<int>(kHeight, y + height); ++i)
                    writeFastHLine(x, i, width, ink);
            }
        private:
            std::array<uint8_t, kWidth * kHeight / 4> pixels_{};
        };

        void arrow(ui::Context& ui, ui::Rect rect, bool down, bool enabled) {
            const bool pressed = enabled && ui.pressedIn(rect);
            const uint8_t alpha = !enabled ? 36 : pressed ? 255 : 208;
            if (ui.redraw(rect, ui::Context::combine(alpha, down)))
                ui.paint(rect, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(ui::themes::Background));
                    const int16_t cx = r.x + r.w / 2, cy = r.y + r.h / 2;
                    const int16_t tip = cy + (down ? 5 : -5), tail = cy + (down ? -5 : 5);
                    for (int offset = 0; offset < 2; ++offset) {
                        output.drawLine(cx - 12, tail + offset, cx, tip + offset,
                                        ui.blend(ui::themes::Accent, alpha));
                        output.drawLine(cx, tip + offset, cx + 12, tail + offset,
                                        ui.blend(ui::themes::Accent, alpha));
                    }
                });
        }
    } // namespace

    void TubeList::reset(size_t index) {
        selected_ = index;
        position_ = static_cast<int64_t>(index) * kPixelsPerItem;
        active_ = moved_ = vertical_ = settling_ = false;
    }

    void TubeList::sync(size_t count) {
        if (count != count_) {
            active_ = moved_ = settling_ = false;
            count_ = count;
            reset(count == 0 ? 0 : std::min(selected_, count - 1));
        }
    }

    float TubeList::angle(size_t index) const {
        const auto delta = static_cast<int64_t>(index) - static_cast<int64_t>(selected_);
        const auto offset = static_cast<int64_t>(selected_) * kPixelsPerItem - position_;
        return (static_cast<float>(delta) + static_cast<float>(offset) / kPixelsPerItem) * kStepAngle;
    }

    TubeRow TubeList::row(size_t index, ui::Rect view) const {
        const float center = angle(index);
        const float cosine = std::cos(center);
        // Back-facing polygon sides are hidden rather than bent around a circle.
        if (cosine <= 1.0f / kCamera)
            return {};
        const float top = projectedY(center, -kHalfFace, view);
        const float bottom = projectedY(center, kHalfFace, view);
        const float scale = perspective(center);
        const int16_t width = static_cast<int16_t>((view.w - 12) * scale);
        const int16_t y = static_cast<int16_t>(std::ceil(top));
        const int16_t height = static_cast<int16_t>(std::floor(bottom) - y + 1);
        if (height < 6 || bottom <= view.y || top >= view.y + view.h)
            return {};
        return {{static_cast<int16_t>(view.x + (view.w - width) / 2), y, width, height}, center, scale,
                static_cast<uint8_t>(130 + 125 * cosine),
                static_cast<int16_t>((view.w - 12) * perspective(center, -kHalfFace)),
                static_cast<int16_t>((view.w - 12) * perspective(center, kHalfFace))};
    }

    void TubeList::settle(uint32_t nowMs) {
        settleOrigin_ = position_;
        settleStarted_ = nowMs;
        settling_ = position_ != static_cast<int64_t>(selected_) * kPixelsPerItem;
    }

    void TubeList::animate(uint32_t nowMs) {
        if (!settling_)
            return;
        const auto target = static_cast<int64_t>(selected_) * kPixelsPerItem;
        const uint32_t elapsed = nowMs - settleStarted_;
        if (elapsed >= kSnapMs) {
            position_ = target;
            settling_ = false;
        } else {
            const float remaining = 1.0f - static_cast<float>(elapsed) / kSnapMs;
            position_ = target + static_cast<int64_t>(std::lround((settleOrigin_ - target)
                                                               * remaining * remaining * remaining));
        }
    }

    void TubeList::step(int direction, size_t count, uint32_t nowMs) {
        sync(count);
        animate(nowMs);
        if (direction < 0 && selected_ > 0)
            --selected_;
        else if (direction > 0 && selected_ + 1 < count)
            ++selected_;
        else
            return;
        active_ = false;
        settle(nowMs);
    }

    std::optional<size_t> TubeList::update(const ui::Touch* touch, ui::Rect view, size_t count, uint32_t nowMs) {
        sync(count);
        animate(nowMs);
        if (!touch || count == 0)
            return {};
        if (ui::hasTouch(*touch, ui::TouchStart)) {
            active_ = ui::contains(view, touch->x, touch->y);
            if (active_) {
                settling_ = false;
                dragOrigin_ = position_;
                startX_ = touch->x;
                startY_ = touch->y;
                moved_ = vertical_ = false;
            }
        }
        if (!active_)
            return {};
        const int dx = static_cast<int>(touch->x) - startX_;
        const int dy = static_cast<int>(touch->y) - startY_;
        if (!moved_ && std::max(std::abs(dx), std::abs(dy)) >= 8) {
            moved_ = true;
            vertical_ = std::abs(dy) > std::abs(dx);
        }
        if (moved_ && vertical_) {
            position_ = std::clamp<int64_t>(dragOrigin_ - dy, 0, static_cast<int64_t>(count - 1) * kPixelsPerItem);
            selected_ = static_cast<size_t>((position_ + kPixelsPerItem / 2) / kPixelsPerItem);
        }
        if (!ui::hasTouch(*touch, ui::TouchRelease))
            return {};
        active_ = false;
        if (!moved_ && ui::hasTouch(*touch, ui::TouchTap) && ui::contains(view, touch->x, touch->y)) {
            const size_t first = selected_ > 3 ? selected_ - 3 : 0;
            const size_t last = std::min(count, selected_ + 4);
            for (size_t index = first; index < last; ++index) {
                const auto geometry = row(index, view);
                if (ui::contains(geometry.bounds, touch->x, touch->y)) {
                    reset(index);
                    return index;
                }
            }
        }
        settle(nowMs);
        return {};
    }

    ui::Rect TubeList::viewport(ui::Context& ui) {
        auto view = content(ui);
        view.w -= 56;
        return view;
    }

    std::optional<size_t> TubeList::draw(ui::Context& ui, size_t count,
                                        const std::function<TubeItem(size_t)>& itemAt, uint32_t nowMs) {
        const auto view = viewport(ui);
        auto activated = update(ui.touch(), view, count, nowMs);
        const ui::Rect up{static_cast<int16_t>(ui.width() - 56), view.y, 44,
                          static_cast<int16_t>((view.h - 4) / 2)};
        const ui::Rect down{up.x, static_cast<int16_t>(up.y + up.h + 4), up.w, up.h};
        const bool previous = ui.tap(up, selected_ > 0);
        const bool next = ui.tap(down, selected_ + 1 < count);
        if (previous)
            step(-1, count, nowMs);
        else if (next)
            step(1, count, nowMs);
        arrow(ui, up, false, selected_ > 0);
        arrow(ui, down, true, selected_ + 1 < count);
        ui.label({180, 0, 94, 42}, std::to_string(count == 0 ? 0 : selected_ + 1) + " / " + std::to_string(count),
                 1, ui::themes::Muted, ui::TextAlign::Right);

        struct Row {
            size_t index;
            TubeItem item;
            TubeRow geometry;
            ui::TextLayout title, detail, number, trailing;
        };
        std::vector<Row> rows;
        rows.reserve(5);
        uint32_t signature = ui::Context::combine(static_cast<uint32_t>(position_), static_cast<uint32_t>(count));
        signature = ui::Context::combine(signature, static_cast<uint32_t>(selected_));
        const size_t first = selected_ > 3 ? selected_ - 3 : 0;
        const size_t last = std::min(count, selected_ + 4);
        for (size_t index = first; index < last; ++index) {
            auto geometry = row(index, view);
            if (geometry.bounds.h <= 0)
                continue;
            auto item = itemAt(index);
            signature = ui::Context::signature(item.title, signature);
            signature = ui::Context::signature(item.detail, signature);
            signature = ui::Context::signature(item.trailing, signature);
            signature = ui::Context::combine(signature, item.current);
            rows.push_back({index, std::move(item), geometry, {}, {}, {}, {}});
        }
        if (!ui.redraw(view, signature, true))
            return activated;
        for (auto& row : rows) {
            const bool showDetail = row.index == selected_ && !row.item.detail.empty();
            row.title = ui.prepareText({64, 0, kWidth - 144, static_cast<int16_t>(showDetail ? 34 : kHeight)},
                                      row.item.title, 2);
            row.detail = ui.prepareText({64, 35, kWidth - 144, 20}, showDetail ? row.item.detail : std::string_view{}, 1);
            row.number = ui.prepareText({12, 0, 44, kHeight}, std::to_string(row.index + 1), 1);
            row.trailing = ui.prepareText({kWidth - 72, 0, 60, kHeight}, row.item.trailing, 1, ui::TextAlign::Right);
        }
        static TubeTexture texture;
        ui.paint(view, [&](Arduino_GFX& output, ui::Rect translated) {
            const int dx = translated.x - view.x, dy = translated.y - view.y;
            output.fillRect(translated.x, translated.y, translated.w, translated.h, ui.color(ui::themes::Background));
            for (const auto& row : rows) {
                texture.clear();
                ui.drawText(texture, row.title, 1);
                ui.drawText(texture, row.detail, 2);
                if (row.index == selected_) {
                    ui.drawText(texture, row.number, row.item.current ? 3 : 2);
                    ui.drawText(texture, row.trailing, 3);
                }
                const auto bounds = row.geometry.bounds;
                for (int y = std::max<int>(view.y, bounds.y); y < std::min<int>(view.y + view.h, bounds.y + bounds.h); ++y) {
                    const auto scan = faceLine(row.geometry.angle, y, view);
                    if (scan.sourceY < 0 || scan.sourceY >= kHeight)
                        continue;
                    const int width = static_cast<int>((view.w - 12) * scan.scale);
                    const int left = view.x + (view.w - width) / 2;
                    const auto alpha = row.geometry.alpha;
                    const uint16_t surface = ui.blend(row.index == selected_ ? ui::themes::SurfaceActive
                                                                           : ui::themes::Surface, alpha);
                    const auto outline = ui.blend(ui::themes::Outline, alpha);
                    output.drawFastHLine(left + dx, y + dy, width, surface);
                    output.drawPixel(left + dx, y + dy,
                                     row.index == selected_ || row.item.current ? ui.color(ui::themes::Accent) : outline);
                    output.drawPixel(left + width - 1 + dx, y + dy, outline);
                    if (y == bounds.y || y == bounds.y + bounds.h - 1)
                        output.drawFastHLine(left + dx, y + dy, width, outline);
                    const std::array<uint16_t, 4> palette{surface, ui.blend(ui::themes::Foreground, alpha),
                        ui.blend(ui::themes::Muted, alpha), ui.blend(ui::themes::Accent, alpha)};
                    int sourceY = scan.sourceY;
                    if (row.index != selected_) {
                        // Keep neighboring titles readable while retaining their
                        // perspective compression and narrower letter spacing.
                        const float boost = 1 + 0.6f * (1 - std::cos(row.geometry.angle));
                        sourceY = static_cast<int>((sourceY - kHeight * 0.5f) / boost + kHeight * 0.5f);
                    }
                    for (int x = 0; x < width;) {
                        const uint8_t ink = texture.pixel(x * kWidth / width, sourceY);
                        const int start = x++;
                        while (x < width && texture.pixel(x * kWidth / width, sourceY) == ink)
                            ++x;
                        if (ink != 0)
                            output.drawFastHLine(left + start + dx, y + dy, x - start, palette[ink]);
                    }
                }
            }
        });
        return activated;
    }
} // namespace screens::rapid
