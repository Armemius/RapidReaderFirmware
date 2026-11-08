#pragma once

#include "ui/screens/Screens.h"

namespace screens::rapid {
    std::string uppercase(std::string_view text);
    ui::Rect content(ui::Context& ui);
    Action navigation(ui::Context& ui, Screen active, Screen& screen);
    bool action(ui::Context& ui, ui::Rect rect, std::string_view title, std::string_view detail = {},
                std::string_view trailing = {}, bool selected = false, bool enabled = true);
    bool choice(ui::Context& ui, ui::Rect rect, std::string_view label, std::string_view value);
    Action read(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress, Screen& screen);
    bool display(ui::Context& ui, settings::InterfaceSettings& config, ThemeStore& themes,
                 std::span<const uint32_t> durations, void (*brightness)(uint8_t));
    // A list page has two large rows and dedicated previous/next touch areas.
    ui::Rect listRow(ui::Context& ui, size_t row);
    bool listPaging(ui::Context& ui, size_t count, size_t& page, ui::CarouselGesture& gesture);
} // namespace screens::rapid
