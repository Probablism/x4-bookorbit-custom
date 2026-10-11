#include "BookOrbitCoverCache.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "JpegToBmpConverter.h"
#include "network/BookOrbitCatalogClient.h"

namespace {
constexpr char CACHE_DIR[] = "/.crosspoint/bookorbit_covers";
constexpr size_t MAX_JPEG_BYTES = 4 * 1024 * 1024;

uint32_t coverVersion(const std::string& value) {
  // Fixed FNV-1a, rather than std::hash, so cache names survive firmware restarts.
  uint32_t hash = 2166136261u;
  for (const unsigned char c : value) hash = (hash ^ c) * 16777619u;
  return hash;
}

bool isReasonableJpeg(const std::string& path) {
  FsFile file;
  if (!Storage.openFileForRead("BookOrbit", path, file)) return false;
  const size_t size = file.fileSize();
  uint8_t signature[2] = {};
  const int read = file.read(signature, sizeof(signature));
  file.close();
  return size >= 4 && size <= MAX_JPEG_BYTES && read == static_cast<int>(sizeof(signature)) && signature[0] == 0xff &&
         signature[1] == 0xd8;
}
}  // namespace

namespace BookOrbitCoverCache {

std::string pathFor(const int64_t bookId, const std::string& updatedAt) {
  char name[96];
  // c2 changes the cache namespace from the original 72x108 previews so an
  // upgrade cannot keep displaying undersized thumbnails from an old cache.
  snprintf(name, sizeof(name), "%s/%lld-%08lx-c2.bmp", CACHE_DIR, static_cast<long long>(bookId),
           static_cast<unsigned long>(coverVersion(updatedAt)));
  return name;
}

bool isAvailable(const int64_t bookId, const std::string& updatedAt) {
  return Storage.exists(pathFor(bookId, updatedAt).c_str());
}

bool ensure(const int64_t bookId, const std::string& updatedAt) {
#if !defined(FREEINK_DEVICE_X4PRO) || !FREEINK_DEVICE_X4PRO
  (void)bookId;
  (void)updatedAt;
  return false;
#else
  const std::string bmpPath = pathFor(bookId, updatedAt);
  if (Storage.exists(bmpPath.c_str())) return true;
  Storage.mkdir("/.crosspoint");
  if (!Storage.exists(CACHE_DIR) && !Storage.mkdir(CACHE_DIR)) return false;

  const std::string jpegPath = bmpPath + ".jpg";
  const std::string tmpBmpPath = bmpPath + ".part";
  Storage.remove(jpegPath.c_str());
  Storage.remove((jpegPath + ".part").c_str());
  Storage.remove(tmpBmpPath.c_str());

  HttpDownloader::DownloadOptions options;
  options.validate = isReasonableJpeg;
  const auto result = BookOrbitCatalogClient::downloadCover(bookId, jpegPath, options);
  if (result != HttpDownloader::OK) {
    LOG_ERR("BookOrbit", "Cover download failed for %lld (err=%d)", static_cast<long long>(bookId), static_cast<int>(result));
    Storage.remove(jpegPath.c_str());
    return false;
  }

  FsFile jpeg;
  FsFile bmp;
  const bool opened = Storage.openFileForRead("BookOrbit", jpegPath, jpeg) &&
                      Storage.openFileForWrite("BookOrbit", tmpBmpPath, bmp);
  bool converted = false;
  if (opened) {
    converted = JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(jpeg, bmp, THUMB_WIDTH, THUMB_HEIGHT, true);
  }
  jpeg.close();
  bmp.close();
  Storage.remove(jpegPath.c_str());
  if (!converted) {
    LOG_ERR("BookOrbit", "Cover conversion failed for %lld", static_cast<long long>(bookId));
    Storage.remove(tmpBmpPath.c_str());
    return false;
  }
  if (!Storage.rename(tmpBmpPath.c_str(), bmpPath.c_str())) {
    Storage.remove(tmpBmpPath.c_str());
    return false;
  }
  return true;
#endif
}

}  // namespace BookOrbitCoverCache
