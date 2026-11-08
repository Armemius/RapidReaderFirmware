#pragma once

#include <cstdint>

namespace product {
    enum class TutorialAction : uint8_t { None, Previous, Next, Skip };

    class Tutorial {
    public:
        // Keep this independent of SD cards, book files and imported settings.
        static constexpr char kPreferenceKey[] = "rrIntro";
        static constexpr uint8_t kVersion = 1;
        static constexpr uint8_t kStepCount = 4;

        void begin(uint8_t savedVersion) {
            step_ = 0;
            active_ = savedVersion < kVersion;
        }
        bool active() const { return active_; }
        uint8_t step() const { return step_; }
        // True only when this action completes/skips an active tutorial.
        bool apply(TutorialAction action) {
            if (!active_)
                return false;
            switch (action) {
            case TutorialAction::Previous:
                if (step_ > 0)
                    --step_;
                break;
            case TutorialAction::Next:
                if (step_ + 1 < kStepCount) {
                    ++step_;
                    break;
                }
                [[fallthrough]];
            case TutorialAction::Skip:
                active_ = false;
                return true;
            case TutorialAction::None:
                break;
            }
            return false;
        }

    private:
        uint8_t step_ = 0;
        bool active_ = false;
    };
} // namespace product
