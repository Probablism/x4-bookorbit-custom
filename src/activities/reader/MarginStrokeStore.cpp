#include "MarginStrokeStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>

namespace {
constexpr char FILE_NAME[] = "notebook_strokes.bin";
constexpr uint8_t VERSION = 1;
constexpr char MAGIC[] = "CIMS";

struct FileHeader {
  char magic[4];
  uint8_t version;
  uint8_t reserved;
  uint16_t count;
  uint32_t nextId;
};

constexpr size_t RECORD_SIZE = sizeof(uint32_t) + sizeof(MarginStrokeKey) + sizeof(uint8_t) +
                               sizeof(MarginStrokePoint) * MARGIN_STROKE_MAX_POINTS;

std::string pathFor(const std::string& stateDir) { return stateDir + "/" + FILE_NAME; }

bool sameKey(const MarginStrokeKey& a, const MarginStrokeKey& b) {
  return a.spineIndex == b.spineIndex && a.pageIndex == b.pageIndex && a.layoutSignature == b.layoutSignature;
}

bool validStroke(const MarginStroke& stroke) {
  if (stroke.id == 0 || stroke.pointCount < 2 || stroke.pointCount > MARGIN_STROKE_MAX_POINTS) return false;
  for (uint8_t i = 0; i < stroke.pointCount; ++i) {
    if (stroke.points[i].x > 1000 || stroke.points[i].y > 1000) return false;
  }
  return true;
}

bool readHeader(FsFile& file, FileHeader& header) {
  return serialization::tryReadPod(file, header) && std::memcmp(header.magic, MAGIC, sizeof(header.magic)) == 0 &&
         header.version == VERSION && header.count <= MARGIN_STROKE_MAX_PER_BOOK;
}

bool readStroke(FsFile& file, MarginStroke& stroke) {
  return serialization::tryReadPod(file, stroke.id) && serialization::tryReadPod(file, stroke.key) &&
         serialization::tryReadPod(file, stroke.pointCount) &&
         file.read(reinterpret_cast<uint8_t*>(stroke.points.data()), sizeof(stroke.points)) == sizeof(stroke.points) &&
         validStroke(stroke);
}

bool writeStroke(FsFile& file, const MarginStroke& stroke) {
  return serialization::tryWritePod(file, stroke.id) && serialization::tryWritePod(file, stroke.key) &&
         serialization::tryWritePod(file, stroke.pointCount) &&
         file.write(reinterpret_cast<const uint8_t*>(stroke.points.data()), sizeof(stroke.points)) == sizeof(stroke.points);
}

bool recoverBackup(const std::string& path) {
  const std::string backup = path + ".bak";
  if (!Storage.exists(path.c_str()) && Storage.exists(backup.c_str())) {
    if (!Storage.rename(backup.c_str(), path.c_str())) {
      LOG_ERR("MInk", "Failed to recover margin ink backup");
      return false;
    }
  }
  return true;
}
}  // namespace

bool MarginStrokeStore::loadPage(const std::string& stateDir, const MarginStrokeKey& key,
                                 std::array<MarginStroke, MARGIN_STROKE_MAX_PER_PAGE>& out, uint8_t& outCount) {
  outCount = 0;
  if (stateDir.empty()) return false;
  const std::string path = pathFor(stateDir);
  if (!recoverBackup(path)) return false;
  if (!Storage.exists(path.c_str())) return true;
  FsFile file;
  if (!Storage.openFileForRead("MInk", path, file)) return false;
  FileHeader header{};
  if (!readHeader(file, header)) {
    file.close();
    LOG_ERR("MInk", "Invalid margin ink header");
    return false;
  }
  for (uint16_t i = 0; i < header.count; ++i) {
    MarginStroke stroke{};
    if (!readStroke(file, stroke)) {
      file.close();
      LOG_ERR("MInk", "Invalid margin ink record %u", i);
      return false;
    }
    if (sameKey(stroke.key, key) && outCount < out.size()) out[outCount++] = stroke;
  }
  file.close();
  return true;
}

bool MarginStrokeStore::append(const std::string& stateDir, const MarginStroke& stroke) {
  if (!validStroke(stroke)) return false;
  return rewrite(stateDir, &stroke, nullptr, false);
}

bool MarginStrokeStore::eraseLatest(const std::string& stateDir, const MarginStrokeKey& key) {
  return rewrite(stateDir, nullptr, &key, true);
}

bool MarginStrokeStore::clearPage(const std::string& stateDir, const MarginStrokeKey& key) {
  return rewrite(stateDir, nullptr, &key, false);
}

bool MarginStrokeStore::rewrite(const std::string& stateDir, const MarginStroke* appendStroke,
                                const MarginStrokeKey* removeKey, const bool eraseLatestOnly) {
  if (stateDir.empty()) return false;
  const std::string path = pathFor(stateDir);
  const std::string tmp = path + ".tmp";
  const std::string backup = path + ".bak";
  if (!recoverBackup(path)) return false;
  FsFile source;
  FileHeader oldHeader{};
  const bool hasSource = Storage.exists(path.c_str());
  if (hasSource) {
    if (!Storage.openFileForRead("MInk", path, source) || !readHeader(source, oldHeader)) {
      if (source) source.close();
      LOG_ERR("MInk", "Cannot read existing margin ink");
      return false;
    }
  }
  uint32_t latestId = 0;
  if (eraseLatestOnly && hasSource) {
    for (uint16_t i = 0; i < oldHeader.count; ++i) {
      MarginStroke stroke{};
      if (!readStroke(source, stroke)) { source.close(); return false; }
      if (sameKey(stroke.key, *removeKey) && stroke.id > latestId) latestId = stroke.id;
    }
    source.close();
    if (latestId == 0) return true;
    if (!Storage.openFileForRead("MInk", path, source) || !readHeader(source, oldHeader)) {
      if (source) source.close();
      return false;
    }
  }
  if (Storage.exists(tmp.c_str())) Storage.remove(tmp.c_str());
  FsFile dest = Storage.open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  if (!dest) { if (source) source.close(); return false; }
  FileHeader newHeader{{'C','I','M','S'}, VERSION, 0, 0, hasSource ? oldHeader.nextId : 1};
  uint8_t appendPageCount = 0;
  if (!serialization::tryWritePod(dest, newHeader)) { dest.close(); if (source) source.close(); Storage.remove(tmp.c_str()); return false; }
  if (hasSource) {
    for (uint16_t i = 0; i < oldHeader.count; ++i) {
      MarginStroke stroke{};
      if (!readStroke(source, stroke)) { dest.close(); source.close(); Storage.remove(tmp.c_str()); return false; }
      const bool remove = removeKey && sameKey(stroke.key, *removeKey) && (!eraseLatestOnly || stroke.id == latestId);
      if (!remove) {
        if (newHeader.count >= MARGIN_STROKE_MAX_PER_BOOK || !writeStroke(dest, stroke)) {
          dest.close(); source.close(); Storage.remove(tmp.c_str()); return false;
        }
        ++newHeader.count;
        if (appendStroke && sameKey(stroke.key, appendStroke->key)) ++appendPageCount;
      }
    }
    source.close();
  }
  if (appendStroke) {
    // IDs are allocated by the durable header rather than millis(), which is
    // not a stable clock across sleep/reboot on every device.
    MarginStroke stored = *appendStroke;
    stored.id = newHeader.nextId == 0 ? 1 : newHeader.nextId;
    if (newHeader.count >= MARGIN_STROKE_MAX_PER_BOOK || appendPageCount >= MARGIN_STROKE_MAX_PER_PAGE ||
        !writeStroke(dest, stored)) {
      dest.close(); Storage.remove(tmp.c_str()); return false;
    }
    ++newHeader.count;
    newHeader.nextId = stored.id + 1;
    if (newHeader.nextId == 0) newHeader.nextId = 1;
  }
  if (!dest.seekSet(0) || !serialization::tryWritePod(dest, newHeader) || !dest.sync()) {
    dest.close(); Storage.remove(tmp.c_str()); return false;
  }
  dest.close();
  if (Storage.exists(backup.c_str())) Storage.remove(backup.c_str());
  const bool replacing = Storage.exists(path.c_str());
  if (replacing && !Storage.rename(path.c_str(), backup.c_str())) { Storage.remove(tmp.c_str()); return false; }
  if (!Storage.rename(tmp.c_str(), path.c_str())) {
    Storage.remove(tmp.c_str());
    if (replacing) Storage.rename(backup.c_str(), path.c_str());
    return false;
  }
  if (replacing && Storage.exists(backup.c_str())) Storage.remove(backup.c_str());
  return true;
}
