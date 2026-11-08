#pragma once

#include <string_view>

namespace product {
    inline constexpr std::string_view kName = "RapidReader";
    inline constexpr std::string_view kVersion = "rapid-reader-0.1.0";
    inline constexpr std::string_view kDefaultLocale = "ru";

    constexpr bool builtInLocale(std::string_view locale) {
        return locale == "ru" || locale == "en";
    }
} // namespace product
