#pragma once

#include "tmc_types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace tmc {

struct RomOffsets {
    u32 mapHeaderBase = 0x11E214;
    u32 areaMetadataBase = 0x0D50FC;
    u32 tileOffset = 0x5A2E80;
    u32 paletteSetTableLoc = 0x0FF850;
    u32 globalTileSetTableLoc = 0x10246C;
    u32 gfxSourceBase = 0x324AE4;
    u32 globalMetaTileSetTableLoc = 0x10309C;
    u32 globalTileDataTableLoc = 0x107988;
    u32 warpInformationTableLoc = 0x13A7F0;
};

class RomDataReader {
public:
    RomDataReader();
    ~RomDataReader();

    bool loadRom(const std::string& path);
    bool isLoaded() const { return !m_romBytes.empty(); }

    size_t getRomSize() const { return m_romBytes.size(); }
    const std::string& getRomPath() const { return m_romPath; }
    const RomOffsets& getOffsets() const { return m_offsets; }

    // Area & Room extraction
    bool loadRoom(u8 area, u8 room, RoomModel& outRoom, TileSetData& outTileset);
    bool loadTilesetOnly(u8 area, u16 tilesetId, TileSetData& outTileset);
    int  getRoomCount(u8 area) const;
    u32  getRoomMetadataOffset(u8 area, u8 room) const;
    u32  getRoomTileDataOffset(u8 area, u8 room) const;
    u32  getRoomHeaderOffset(u8 area, u8 room) const;

    // GBA LZ77 Decompressor & Compressor
    static std::vector<u8> decompressLZ77(const u8* src, size_t srcMaxLen);
    static std::vector<u8> compressLZ77(const u8* src, size_t srcLen);

    // Color conversion (BGR555 15-bit -> RGBA8888 32-bit)
    static ColorRGBA bgr555ToRgba(u16 bgr);

    // Palette loading
    bool loadPaletteSet(int paletteSetId, FullPalette& outPalette);

private:
    std::string m_romPath;
    std::vector<u8> m_romBytes;
    RomOffsets  m_offsets;

    // Helper read functions
    u8  read8(u32 romOffset) const;
    u16 read16(u32 romOffset) const;
    u32 read32(u32 romOffset) const;
    u32 readAddr(u32 romOffset) const;
    u32 gbaPtrToOffset(u32 gbaPtr) const;

    void updateRegionOffsets();
};

} // namespace tmc
