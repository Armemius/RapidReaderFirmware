#pragma once

#include "ui/screens/Screens.h"

namespace screens {
    void rapidReaderLogo(ui::Context& ui, ui::Rect rect);
    void rapidReaderBoot(ui::Context& ui, std::string_view phase = {});
} // namespace screens
