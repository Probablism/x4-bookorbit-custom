#pragma once

#include <cstdint>

// Display style is local reader metadata. BookOrbit's annotation wire format
// remains a text/xpointer highlight, so another client can still render it.
enum class ClippingAnnotationStyle : uint8_t {
  Highlight = 0,
  Underline = 1,
};

inline constexpr bool isValidClippingAnnotationStyle(const uint8_t value) {
  return value <= static_cast<uint8_t>(ClippingAnnotationStyle::Underline);
}
