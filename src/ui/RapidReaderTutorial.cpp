#include "ui/RapidReaderTutorial.h"

#include <array>
#include "product/RapidReaderText.h"
#include "ui/RapidReaderBrand.h"
#include "ui/RapidReaderUi.h"

namespace screens {
    product::TutorialAction rapidReaderTutorial(ui::Context& ui, const product::Tutorial& tutorial) {
        using product::Text;
        using product::TutorialAction;
        using namespace ui::themes;
        constexpr std::array<std::array<Text, 3>, product::Tutorial::kStepCount> steps{{
            {Text::TutorialBooks, Text::TutorialBooksLine1, Text::TutorialBooksLine2},
            {Text::TutorialControls, Text::TutorialControlsLine1, Text::TutorialControlsLine2},
            {Text::TutorialGestures, Text::TutorialGesturesLine1, Text::TutorialGesturesLine2},
            {Text::TutorialDisplay, Text::TutorialDisplayLine1, Text::TutorialDisplayLine2},
        }};
        const auto step = std::min<size_t>(tutorial.step(), steps.size() - 1);
        const auto message = [&](Text text) { return product::text(text, ui.locale()); };
        ui.beginFrame(0xfc);
        ui.label({24, 0, 440, 38}, "RAPIDREADER / " + rapid::uppercase(message(Text::Tutorial)), 1, Foreground);
        ui.label({500, 0, 116, 38}, "0" + std::to_string(step + 1) + " / 04", 1, Accent, ui::TextAlign::Right);
        const ui::Rect rule{24, 39, 592, 1};
        if (ui.redraw(rule, 1))
            ui.paint(rule, [&](Arduino_GFX& output, ui::Rect r) {
                output.fillRect(r.x, r.y, r.w, r.h, ui.color(Outline));
            });
        rapidReaderLogo(ui, {24, 53, 48, 48});
        ui.label({92, 45, 524, 26}, rapid::uppercase(message(steps[step][0])), 1, Accent);
        ui.label({92, 73, 524, 20}, message(steps[step][1]), 1, Foreground);
        ui.label({92, 96, 524, 20}, message(steps[step][2]), 1, Muted);
        for (size_t index = 0; index < steps.size(); ++index) {
            const ui::Rect segment{static_cast<int16_t>(24 + index * 150), 118, 142, 2};
            const bool reached = index <= step;
            if (ui.redraw(segment, reached))
                ui.paint(segment, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(reached ? Accent : Outline));
                });
        }
        TutorialAction action = TutorialAction::None;
        if (rapid::action(ui, {24, 128, 144, 36}, rapid::uppercase(message(Text::TutorialSkip))))
            action = TutorialAction::Skip;
        if (rapid::action(ui, {332, 128, 124, 36}, rapid::uppercase(message(Text::TutorialPrevious)),
                          {}, {}, false, step > 0))
            action = TutorialAction::Previous;
        if (rapid::action(ui, {468, 128, 148, 36},
                          rapid::uppercase(message(step + 1 == steps.size() ? Text::TutorialFinish : Text::TutorialNext)),
                          {}, {}, true))
            action = TutorialAction::Next;
        ui.endFrame();
        return action;
    }
} // namespace screens
