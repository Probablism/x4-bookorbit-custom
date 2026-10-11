#pragma once

#include <cstdint>
#include <string>

/**
 * A deliberately small, X4 Pro-only cache for BookOrbit catalog covers. The
 * cache contains display-ready 1-bit BMPs, never source artwork or EPUB files.
 */
namespace BookOrbitCoverCache {

// These are intentionally close to the X4 Pro catalog tile dimensions. The
// renderer does not upscale 1-bit BMPs, so a tiny home-screen thumbnail would
// otherwise occupy only a fraction of a cover-grid cell.
constexpr int THUMB_WIDTH = 144;
constexpr int THUMB_HEIGHT = 216;

std::string pathFor(int64_t bookId, const std::string& updatedAt);
bool isAvailable(int64_t bookId, const std::string& updatedAt);

// Synchronous by design: call only from the catalog's existing loading path,
// never from render(). A failed request leaves no partial thumbnail behind.
bool ensure(int64_t bookId, const std::string& updatedAt);

}  // namespace BookOrbitCoverCache
