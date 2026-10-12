#include "ChessGameStore.h"

#include <HalStorage.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr char PATH[] = "/.crosspoint/chess_games.bin";
constexpr char TEMP_PATH[] = "/.crosspoint/chess_games.bin.tmp";
constexpr char BACKUP_PATH[] = "/.crosspoint/chess_games.bin.bak";
constexpr char MAGIC[] = "CPCH";
constexpr uint8_t VERSION = 1;

struct DiskSlot {
  uint8_t occupied;
  char name[ChessGameStore::NAME_SIZE];
  chess::Position position;
};

struct DiskFile {
  char magic[4];
  uint8_t version;
  uint8_t reserved[3];
  DiskSlot slots[ChessGameStore::SLOT_COUNT];
};

void clearSlots(std::array<ChessGameStore::Slot, ChessGameStore::SLOT_COUNT>& slots) {
  for (uint8_t i = 0; i < ChessGameStore::SLOT_COUNT; ++i) {
    slots[i] = {};
    ChessGameStore::defaultName(i, slots[i].name);
  }
}

bool readFile(DiskFile& out) {
  if (!Storage.exists(PATH)) return false;
  FsFile file;
  if (!Storage.openFileForRead("CHESS", PATH, file)) return false;
  const bool ok = file.fileSize() == sizeof(out) && file.read(reinterpret_cast<uint8_t*>(&out), sizeof(out)) == sizeof(out);
  file.close();
  return ok && std::memcmp(out.magic, MAGIC, sizeof(out.magic)) == 0 && out.version == VERSION;
}

bool writeFile(const DiskFile& data) {
  if (Storage.exists(TEMP_PATH)) Storage.remove(TEMP_PATH);
  FsFile file;
  if (!Storage.openFileForWrite("CHESS", TEMP_PATH, file)) return false;
  const bool ok = file.write(reinterpret_cast<const uint8_t*>(&data), sizeof(data)) == sizeof(data) && file.sync() && file.close();
  if (!ok) {
    if (file) file.close();
    Storage.remove(TEMP_PATH);
    return false;
  }
  if (Storage.exists(BACKUP_PATH)) Storage.remove(BACKUP_PATH);
  const bool hadOriginal = Storage.exists(PATH);
  if (hadOriginal && !Storage.rename(PATH, BACKUP_PATH)) {
    Storage.remove(TEMP_PATH);
    return false;
  }
  if (!Storage.rename(TEMP_PATH, PATH)) {
    if (hadOriginal && Storage.exists(BACKUP_PATH)) Storage.rename(BACKUP_PATH, PATH);
    Storage.remove(TEMP_PATH);
    return false;
  }
  return true;
}
}  // namespace

void ChessGameStore::defaultName(const uint8_t slot, std::array<char, NAME_SIZE + 1>& out) {
  std::snprintf(out.data(), out.size(), "Game %u", static_cast<unsigned>(slot + 1));
}

bool ChessGameStore::loadAll(std::array<Slot, SLOT_COUNT>& slots) {
  clearSlots(slots);
  DiskFile disk{};
  if (!readFile(disk)) return false;
  for (uint8_t i = 0; i < SLOT_COUNT; ++i) {
    slots[i].occupied = disk.slots[i].occupied != 0;
    std::memcpy(slots[i].name.data(), disk.slots[i].name, NAME_SIZE);
    slots[i].name[NAME_SIZE] = '\0';
    if (slots[i].name[0] == '\0') defaultName(i, slots[i].name);
    slots[i].position = disk.slots[i].position;
  }
  return true;
}

bool ChessGameStore::save(const uint8_t slot, const chess::Position& position) {
  if (slot >= SLOT_COUNT) return false;
  std::array<Slot, SLOT_COUNT> slots{};
  loadAll(slots);
  slots[slot].occupied = true;
  slots[slot].position = position;

  DiskFile disk{};
  std::memcpy(disk.magic, MAGIC, sizeof(disk.magic));
  disk.version = VERSION;
  for (uint8_t i = 0; i < SLOT_COUNT; ++i) {
    disk.slots[i].occupied = slots[i].occupied ? 1 : 0;
    std::memcpy(disk.slots[i].name, slots[i].name.data(), NAME_SIZE);
    disk.slots[i].position = slots[i].position;
  }
  return writeFile(disk);
}
