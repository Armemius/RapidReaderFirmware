#pragma once

#include "product/RapidReader.h"
#include "settings/SettingsModel.h"

namespace product {
    // Apply on load and on companion edits, so hidden preferences cannot leave
    // an upgraded device with an inaccessible non-default configuration.
    inline void applyDefaults(settings::DeviceSettings& value) {
        const auto speed = value.reading.wpm;
        value.reading = settings::ReadingSettings{};
        value.reading.wpm = speed;
        value.network = settings::NetworkSettings{};
        value.updates = settings::UpdateSettings{};
        if (!builtInLocale(value.interface.locale))
            value.interface.locale = kDefaultLocale;
    }
} // namespace product
