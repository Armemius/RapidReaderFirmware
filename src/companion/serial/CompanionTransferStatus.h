#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace companion::serial {
    enum class TransferActivity { Connected, Receiving, Sending, Processing };

    struct TransferStatus {
        TransferActivity activity = TransferActivity::Connected;
        uint64_t completedBytes = 0;
        uint64_t totalBytes = 0;
        std::string_view name;

        int percentage() const {
            if (totalBytes == 0)
                return -1;
            const auto done = std::min(completedBytes, totalBytes);
            // Conversion before multiplication also handles very large counters.
            return std::clamp(static_cast<int>(100.0 * static_cast<double>(done) / totalBytes), 0, 100);
        }
    };
} // namespace companion::serial
