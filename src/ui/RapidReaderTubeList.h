#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "ui/Ui.h"

namespace screens::rapid {
    struct TubeItem {
        std::string_view title;
        std::string_view detail;
        std::string trailing;
        bool current = false;
    };

    struct TubeRow {
        ui::Rect bounds;
        float angle = 0;
        float scale = 1;
        uint8_t alpha = 255;
        int16_t topWidth = 0;
        int16_t bottomWidth = 0;
    };

    // Shared vertical hexagonal-prism picker for books and chapters. Positions are kept
    // in integer drag pixels, so neither list length nor sample rate affects motion.
    class TubeList {
    public:
        static constexpr int kPixelsPerItem = 40;
        void reset(size_t index = 0);
        size_t selected() const { return selected_; }
        bool dragging() const { return active_; }
        TubeRow row(size_t index, ui::Rect viewport) const;
        std::optional<size_t> update(const ui::Touch* touch, ui::Rect viewport, size_t count, uint32_t nowMs);
        void step(int direction, size_t count, uint32_t nowMs);
        std::optional<size_t> draw(ui::Context& ui, size_t count,
                                   const std::function<TubeItem(size_t)>& itemAt, uint32_t nowMs);
        static ui::Rect viewport(ui::Context& ui);

    private:
        void sync(size_t count);
        void settle(uint32_t nowMs);
        void animate(uint32_t nowMs);
        float angle(size_t index) const;
        size_t selected_ = 0;
        size_t count_ = 0;
        int64_t position_ = 0;
        int64_t dragOrigin_ = 0;
        int64_t settleOrigin_ = 0;
        uint32_t settleStarted_ = 0;
        uint16_t startX_ = 0, startY_ = 0;
        bool active_ = false;
        bool moved_ = false;
        bool vertical_ = false;
        bool settling_ = false;
    };
} // namespace screens::rapid
