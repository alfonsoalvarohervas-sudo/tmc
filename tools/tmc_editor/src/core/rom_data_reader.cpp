#include "rom_data_reader.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace tmc {

RomDataReader::RomDataReader() {
    updateRegionOffsets();
}

RomDataReader::~RomDataReader() = default;

void RomDataReader::updateRegionOffsets() {
    // Check game code for region (USA = BZME, EU = BZMP, JP = BZMJ)
    if (m_romBytes.size() >= 0xBC && m_romBytes[0xAC] == 'B' && m_romBytes[0xAD] == 'Z' && m_romBytes[0xAE] == 'M' && m_romBytes[0xAF] == 'P') {
        // EU Offsets
        m_offsets.mapHeaderBase = 0x11D95C;
        m_offsets.areaMetadataBase = 0x0D4828;
        m_offsets.tileOffset = 0x5A23D0;
        m_offsets.paletteSetTableLoc = 0x0FED88;
        m_offsets.globalTileSetTableLoc = 0x101BC8;
        m_offsets.gfxSourceBase = 0x323FEC;
        m_offsets.globalMetaTileSetTableLoc = 0x1027F8;
        m_offsets.globalTileDataTableLoc = 0x1070E4;
        m_offsets.warpInformationTableLoc = 0x139EDC;
    } else {
        // USA Offsets (default)
        m_offsets.mapHeaderBase = 0x11E214;
        m_offsets.areaMetadataBase = 0x0D50FC;
        m_offsets.tileOffset = 0x5A2E80;
        m_offsets.paletteSetTableLoc = 0x0FF850;
        m_offsets.globalTileSetTableLoc = 0x10246C;
        m_offsets.gfxSourceBase = 0x324AE4;
        m_offsets.globalMetaTileSetTableLoc = 0x10309C;
        m_offsets.globalTileDataTableLoc = 0x107988;
        m_offsets.warpInformationTableLoc = 0x13A7F0;
    }
}

bool RomDataReader::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[RomDataReader] Failed to open ROM at: " << path << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    if (size < 0x800000) {
        std::cerr << "[RomDataReader] File too small to be TMC ROM: " << size << " bytes" << std::endl;
        return false;
    }

    m_romBytes.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(m_romBytes.data()), size)) {
        m_romBytes.clear();
        return false;
    }

    m_romPath = path;
    updateRegionOffsets();
    std::cout << "[RomDataReader] Loaded ROM (" << size / (1024 * 1024) << " MB): " << path << std::endl;
    return true;
}

u8 RomDataReader::read8(u32 romOffset) const {
    if (romOffset < m_romBytes.size()) {
        return m_romBytes[romOffset];
    }
    return 0;
}

u16 RomDataReader::read16(u32 romOffset) const {
    if (romOffset + 1 < m_romBytes.size()) {
        return static_cast<u16>(m_romBytes[romOffset]) |
               (static_cast<u16>(m_romBytes[romOffset + 1]) << 8);
    }
    return 0;
}

u32 RomDataReader::read32(u32 romOffset) const {
    if (romOffset + 3 < m_romBytes.size()) {
        return static_cast<u32>(m_romBytes[romOffset]) |
               (static_cast<u32>(m_romBytes[romOffset + 1]) << 8) |
               (static_cast<u32>(m_romBytes[romOffset + 2]) << 16) |
               (static_cast<u32>(m_romBytes[romOffset + 3]) << 24);
    }
    return 0;
}

u32 RomDataReader::gbaPtrToOffset(u32 gbaPtr) const {
    if ((gbaPtr & 0xFF000000) == 0x08000000) {
        return gbaPtr & 0x01FFFFFF;
    }
    return 0;
}

u32 RomDataReader::readAddr(u32 romOffset) const {
    u32 ptr = read32(romOffset);
    return gbaPtrToOffset(ptr);
}

ColorRGBA RomDataReader::bgr555ToRgba(u16 bgr) {
    u8 r = (bgr & 0x1F);
    u8 g = ((bgr >> 5) & 0x1F);
    u8 b = ((bgr >> 10) & 0x1F);

    r = static_cast<u8>((r * 527 + 23) >> 6);
    g = static_cast<u8>((g * 527 + 23) >> 6);
    b = static_cast<u8>((b * 527 + 23) >> 6);

    return ColorRGBA{ r, g, b, 255 };
}

// Standard GBA BIOS LZ77 Decompressor (Type 0x10)
std::vector<u8> RomDataReader::decompressLZ77(const u8* src, size_t srcMaxLen) {
    std::vector<u8> out;
    if (!src || srcMaxLen < 4) return out;

    u8 header = src[0];
    if (header != 0x10) {
        return out;
    }

    size_t inIdx = 4;
    u32 decompressedSize = static_cast<u32>(src[1]) |
                          (static_cast<u32>(src[2]) << 8) |
                          (static_cast<u32>(src[3]) << 16);

    if (decompressedSize == 0) {
        if (srcMaxLen < 8) return out;
        decompressedSize = static_cast<u32>(src[4]) |
                          (static_cast<u32>(src[5]) << 8) |
                          (static_cast<u32>(src[6]) << 16) |
                          (static_cast<u32>(src[7]) << 24);
        inIdx = 8;
    }

    if (decompressedSize == 0 || decompressedSize > 0x200000) {
        return out;
    }

    out.resize(decompressedSize);
    size_t outIdx = 0;

    while (outIdx < decompressedSize && inIdx < srcMaxLen) {
        u8 flag = src[inIdx++];
        for (int b = 7; b >= 0 && outIdx < decompressedSize; --b) {
            if ((flag & (1 << b)) == 0) {
                if (inIdx >= srcMaxLen) break;
                out[outIdx++] = src[inIdx++];
            } else {
                if (inIdx + 1 >= srcMaxLen) break;
                u8 b1 = src[inIdx++];
                u8 b2 = src[inIdx++];

                size_t length = ((b1 >> 4) & 0x0F) + 3;
                size_t disp = (((b1 & 0x0F) << 8) | b2) + 1;

                if (disp > outIdx) {
                    break;
                }

                size_t srcPos = outIdx - disp;
                for (size_t i = 0; i < length && outIdx < decompressedSize; ++i) {
                    out[outIdx++] = out[srcPos + i];
                }
            }
        }
    }

    return out;
}

std::vector<u8> RomDataReader::compressLZ77(const u8* src, size_t srcLen) {
    std::vector<u8> out;
    if (!src || srcLen == 0) return out;

    // Header 0x10 followed by 24-bit decompressed size
    out.push_back(0x10);
    out.push_back(static_cast<u8>(srcLen & 0xFF));
    out.push_back(static_cast<u8>((srcLen >> 8) & 0xFF));
    out.push_back(static_cast<u8>((srcLen >> 16) & 0xFF));

    size_t inPos = 0;
    while (inPos < srcLen) {
        size_t flagPos = out.size();
        out.push_back(0); // placeholder for flag byte
        u8 flagByte = 0;

        for (int block = 7; block >= 0 && inPos < srcLen; --block) {
            size_t bestLen = 0;
            size_t bestDisp = 0;

            // Search back window up to 0x1000 (4096 bytes)
            size_t maxDisp = std::min(inPos, static_cast<size_t>(0x1000));
            size_t maxMatch = std::min(srcLen - inPos, static_cast<size_t>(18)); // 3 to 18 bytes

            if (maxMatch >= 3) {
                for (size_t disp = 1; disp <= maxDisp; ++disp) {
                    size_t matchLen = 0;
                    while (matchLen < maxMatch && src[inPos - disp + matchLen] == src[inPos + matchLen]) {
                        matchLen++;
                    }
                    if (matchLen > bestLen && matchLen >= 3) {
                        bestLen = matchLen;
                        bestDisp = disp;
                        if (bestLen == 18) break;
                    }
                }
            }

            if (bestLen >= 3) {
                flagByte |= (1 << block);
                u8 b1 = static_cast<u8>(((bestLen - 3) << 4) | (((bestDisp - 1) >> 8) & 0x0F));
                u8 b2 = static_cast<u8>((bestDisp - 1) & 0xFF);
                out.push_back(b1);
                out.push_back(b2);
                inPos += bestLen;
            } else {
                out.push_back(src[inPos++]);
            }
        }

        out[flagPos] = flagByte;
    }

    // Pad to 4 bytes boundary
    while ((out.size() % 4) != 0) {
        out.push_back(0);
    }

    return out;
}

bool RomDataReader::loadPaletteSet(int pnum, FullPalette& outPalette) {
    if (!isLoaded() || pnum < 0) return false;

    // Initialize with transparent black
    for (int p = 0; p < 16; ++p) {
        for (int i = 0; i < 16; ++i) {
            outPalette[p][i] = ColorRGBA{ 0, 0, 0, 0 };
        }
    }

    // Minish Maker default palette 0 (engine fallback)
    const u8 defPal0[16][3] = {
        { 0, 0, 0 },
        { 14 * 8, 3 * 8, 2 * 8 },
        { 20 * 8, 5 * 8, 3 * 8 },
        { 26 * 8, 8 * 8, 4 * 8 },
        { 31 * 8, 10 * 8, 2 * 8 },
        { 31 * 8, 23 * 8, 9 * 8 },
        { 31 * 8, 17 * 8, 2 * 8 },
        { 31 * 8, 23 * 8, 5 * 8 },
        { 31 * 8, 28 * 8, 7 * 8 },
        { 31 * 8, 31 * 8, 12 * 8 },
        { 14 * 8, 12 * 8, 12 * 8 },
        { 19 * 8, 17 * 8, 17 * 8 },
        { 24 * 8, 22 * 8, 22 * 8 },
        { 29 * 8, 27 * 8, 27 * 8 },
        { 31 * 8, 31 * 31 / 31 * 8, 31 * 8 },
        { 0, 0, 0 }
    };
    for (int i = 0; i < 16; ++i) {
        outPalette[0][i] = ColorRGBA{ defPal0[i][0], defPal0[i][1], defPal0[i][2], (i == 0 ? (u8)0 : (u8)255) };
    }

    // Minish Maker default palette 1 (water/environment fallback)
    const u8 defPal1[16][3] = {
        { 0, 0, 0 },
        { 3 * 8, 6 * 8, 18 * 8 },
        { 4 * 8, 13 * 8, 25 * 8 },
        { 5 * 8, 20 * 8, 30 * 8 },
        { 9 * 8, 26 * 8, 31 * 8 },
        { 21 * 8, 29 * 8, 31 * 8 },
        { 13 * 8, 9 * 8, 5 * 8 },
        { 21 * 8, 12 * 8, 4 * 8 },
        { 28 * 8, 18 * 8, 5 * 8 },
        { 31 * 8, 26 * 8, 4 * 8 },
        { 10 * 8, 12 * 8, 13 * 8 },
        { 14 * 8, 17 * 8, 18 * 8 },
        { 20 * 8, 22 * 8, 24 * 8 },
        { 26 * 8, 27 * 8, 29 * 8 },
        { 31 * 8, 31 * 8, 31 * 8 },
        { 0, 0, 0 }
    };
    for (int i = 0; i < 16; ++i) {
        outPalette[1][i] = ColorRGBA{ defPal1[i][0], defPal1[i][1], defPal1[i][2], (i == 0 ? (u8)0 : (u8)255) };
    }

    u32 addr = readAddr(m_offsets.paletteSetTableLoc + (pnum * 4));
    if (addr == 0 || addr >= m_romBytes.size()) return false;

    u16 palIndex = read16(addr);
    u8 pstart = read8(addr + 2);
    u8 pamount = read8(addr + 3);

    u32 palAddr = m_offsets.tileOffset + (static_cast<u32>(palIndex) << 5);
    if (palAddr + pamount * 32 > m_romBytes.size()) return false;

    for (int p = pstart; p < (pstart + pamount) && p < 16; ++p) {
        u32 offset = palAddr + (p - pstart) * 32;
        for (int i = 0; i < 16; ++i) {
            u16 bgr = read16(offset + i * 2);
            u8 r = (bgr & 0x1F);
            u8 g = ((bgr >> 5) & 0x1F);
            u8 b = ((bgr >> 10) & 0x1F);
            r = static_cast<u8>((r * 255) / 31);
            g = static_cast<u8>((g * 255) / 31);
            b = static_cast<u8>((b * 255) / 31);
            outPalette[p][i] = ColorRGBA{ r, g, b, (i == 0 ? (u8)0 : (u8)255) };
        }
    }

    return true;
}

bool RomDataReader::loadRoom(u8 area, u8 room, RoomModel& outRoom, TileSetData& outTileset) {
    if (!isLoaded() || area >= TOTAL_AREAS || room >= MAX_ROOMS_PER_AREA) {
        return false;
    }

    outRoom.areaIndex = area;
    outRoom.roomIndex = room;
    outRoom.name = "Area " + std::to_string(area) + " - Room " + std::to_string(room);

    // 1. Read Room Header from MapHeaderBase
    u32 areaRMDTableLoc = readAddr(m_offsets.mapHeaderBase + (area * 4));
    if (areaRMDTableLoc == 0) return false;

    u32 roomMetaDataTableLoc = areaRMDTableLoc + (room * 10);
    outRoom.header.map_x = read16(roomMetaDataTableLoc) >> 4;
    outRoom.header.map_y = read16(roomMetaDataTableLoc + 2) >> 4;
    outRoom.header.pixel_width = read16(roomMetaDataTableLoc + 4);
    outRoom.header.pixel_height = read16(roomMetaDataTableLoc + 6);
    outRoom.header.tileSet_id = read16(roomMetaDataTableLoc + 8);

    if (outRoom.header.pixel_width == 0 || outRoom.header.pixel_height == 0) {
        outRoom.header.pixel_width = 256;
        outRoom.header.pixel_height = 256;
    }

    // 2. Load Tileset Graphics & Palette (Set1)
    outTileset.id = outRoom.header.tileSet_id;
    outTileset.vramGfx.assign(0x10000, 0); // 64KB VRAM buffer

    int paletteSetId = 0;
    u32 areaTileSetTableLoc = readAddr(m_offsets.globalTileSetTableLoc + (area * 4));
    if (areaTileSetTableLoc != 0) {
        u32 roomTileSetLoc = readAddr(areaTileSetTableLoc + (outRoom.header.tileSet_id * 4));
        if (roomTileSetLoc != 0) {
            u32 cur = roomTileSetLoc;
            bool cont = true;
            while (cont && cur + 12 <= m_romBytes.size()) {
                u32 data1 = read32(cur);
                u32 data2 = read32(cur + 4);
                u32 data3 = read32(cur + 8);

                if (data2 == 0) {
                    paletteSetId = static_cast<int>(data1 & 0x7FFFFFFF);
                } else if ((data2 & 0x0F000000) == 0x06000000) { // VRAM destination
                    u32 source = (data1 & 0x7FFFFFFF) + m_offsets.gfxSourceBase;
                    u32 dest = data2 & 0x00FFFFFF;
                    bool compressed = (data3 & 0x80000000) != 0;
                    u32 size = data3 & 0x7FFFFFFF;

                    if (source < m_romBytes.size()) {
                        const u8* srcPtr = &m_romBytes[source];
                        size_t srcMax = m_romBytes.size() - source;

                        if (compressed || srcPtr[0] == 0x10) {
                            std::vector<u8> decomp = decompressLZ77(srcPtr, srcMax);
                            if (!decomp.empty()) {
                                size_t copyLen = std::min(decomp.size(), outTileset.vramGfx.size() - dest);
                                std::memcpy(outTileset.vramGfx.data() + dest, decomp.data(), copyLen);
                            }
                        } else {
                            size_t copyLen = std::min<size_t>(static_cast<size_t>(size) << 2, outTileset.vramGfx.size() - dest);
                            std::memcpy(outTileset.vramGfx.data() + dest, srcPtr, copyLen);
                        }
                    }
                }

                cont = (data1 & 0x80000000) != 0;
                cur += 12;
            }
        }
    }

    // Load Palettes
    loadPaletteSet(paletteSetId, outTileset.palette);

    // 3. Load Metatiles Definitions (Set2)
    u32 metaTileSetsLoc = readAddr(m_offsets.globalMetaTileSetTableLoc + (area * 4));
    if (metaTileSetsLoc != 0) {
        u32 cur = metaTileSetsLoc;
        bool cont = true;
        while (cont && cur + 12 <= m_romBytes.size()) {
            u32 data1 = read32(cur);
            u32 data2 = read32(cur + 4);
            u32 data3 = read32(cur + 8);

            u32 source = (data1 & 0x7FFFFFFF) + m_offsets.gfxSourceBase;
            u32 dest = data2 & 0xFFFFFFFF;
            bool compressed = (data3 & 0x80000000) != 0;
            u32 size = data3 & 0x7FFFFFFF;

            if (source < m_romBytes.size()) {
                const u8* srcPtr = &m_romBytes[source];
                size_t srcMax = m_romBytes.size() - source;
                std::vector<u8> decomp;
                const u8* dataPtr = srcPtr;
                size_t dataLen = static_cast<size_t>(size) << 2;

                if (compressed || srcPtr[0] == 0x10) {
                    decomp = decompressLZ77(srcPtr, srcMax);
                    if (!decomp.empty()) {
                        dataPtr = decomp.data();
                        dataLen = decomp.size();
                    }
                }

                if (dest == 0x0202CEB4) {
                    outTileset.bg2MetaTiles.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x02012654) {
                    outTileset.bg1MetaTiles.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x0202AEB4) {
                    outTileset.bg2MetaTypes.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x02010654) {
                    outTileset.bg1MetaTypes.assign(dataPtr, dataPtr + dataLen);
                }
            }

            cont = (data1 & 0x80000000) != 0;
            cur += 12;
        }
    }

    // Parse Metatiles into structured arrays
    outTileset.metaTilesBottom.clear();
    size_t numBg2Metas = outTileset.bg2MetaTiles.size() / 8;
    outTileset.metaTilesBottom.resize(numBg2Metas);
    for (size_t m = 0; m < numBg2Metas; ++m) {
        for (size_t s = 0; s < 4; ++s) {
            u16 st = static_cast<u16>(outTileset.bg2MetaTiles[m * 8 + s * 2]) |
                     (static_cast<u16>(outTileset.bg2MetaTiles[m * 8 + s * 2 + 1]) << 8);
            outTileset.metaTilesBottom[m].subTiles[s].raw = st;
        }
    }

    outTileset.metaTilesTop.clear();
    size_t numBg1Metas = outTileset.bg1MetaTiles.size() / 8;
    outTileset.metaTilesTop.resize(numBg1Metas);
    for (size_t m = 0; m < numBg1Metas; ++m) {
        for (size_t s = 0; s < 4; ++s) {
            u16 st = static_cast<u16>(outTileset.bg1MetaTiles[m * 8 + s * 2]) |
                     (static_cast<u16>(outTileset.bg1MetaTiles[m * 8 + s * 2 + 1]) << 8);
            outTileset.metaTilesTop[m].subTiles[s].raw = st;
        }
    }

    // 4. Load Room Tilemap Data (Set3)
    outRoom.bg2Exists = false;
    outRoom.bg1Exists = false;
    outRoom.layerBottom.assign(MAX_MAP_TILES, 0);
    outRoom.layerTop.assign(MAX_MAP_TILES, 0);
    outRoom.collision.assign(MAX_MAP_TILES, 0);

    int tWidth = outRoom.header.tileWidth();
    int tHeight = outRoom.header.tileHeight();

    u32 areaTileDataTableLoc = readAddr(m_offsets.globalTileDataTableLoc + (area * 4));
    if (areaTileDataTableLoc != 0) {
        u32 tileDataLoc = readAddr(areaTileDataTableLoc + (room * 4));
        if (tileDataLoc != 0) {
            u32 cur = tileDataLoc;
            bool cont = true;
            while (cont && cur + 12 <= m_romBytes.size()) {
                u32 data1 = read32(cur);
                u32 data2 = read32(cur + 4);
                u32 data3 = read32(cur + 8);

                u32 source = (data1 & 0x7FFFFFFF) + m_offsets.gfxSourceBase;
                u32 dest = data2 & 0xFFFFFFFF;
                bool compressed = (data3 & 0x80000000) != 0;
                u32 size = data3 & 0x7FFFFFFF;

                if (source < m_romBytes.size()) {
                    const u8* srcPtr = &m_romBytes[source];
                    size_t srcMax = m_romBytes.size() - source;
                    std::vector<u8> decomp;
                    const u8* dataPtr = srcPtr;
                    size_t dataLen = static_cast<size_t>(size) << 2;

                    if (compressed || srcPtr[0] == 0x10) {
                        decomp = decompressLZ77(srcPtr, srcMax);
                        if (!decomp.empty()) {
                            dataPtr = decomp.data();
                            dataLen = decomp.size();
                        }
                    }

                    // BG2 (Bottom Ground Layer)
                    if (dest == 0x02025EB4) {
                        outRoom.bg2Exists = true;
                        size_t count = dataLen / 2;
                        for (int y = 0; y < tHeight; ++y) {
                            for (int x = 0; x < tWidth; ++x) {
                                size_t roomPos = y * tWidth + x;
                                size_t mapPos = y * MAX_MAP_WIDTH + x;
                                if (roomPos < count) {
                                    u16 mt = static_cast<u16>(dataPtr[roomPos * 2]) |
                                             (static_cast<u16>(dataPtr[roomPos * 2 + 1]) << 8);
                                    outRoom.layerBottom[mapPos] = mt;

                                    // Populate default collision from metatile type
                                    if (mt * 2 < outTileset.bg2MetaTypes.size()) {
                                        outRoom.collision[mapPos] = outTileset.bg2MetaTypes[mt * 2];
                                    }
                                }
                            }
                        }
                    }
                    // BG1 (Top Overhead Layer)
                    else if (dest == 0x0200B654 || dest == 0x02002F00) {
                        outRoom.bg1Exists = true;
                        size_t count = dataLen / 2;
                        for (int y = 0; y < tHeight; ++y) {
                            for (int x = 0; x < tWidth; ++x) {
                                size_t roomPos = y * tWidth + x;
                                size_t mapPos = y * MAX_MAP_WIDTH + x;
                                if (roomPos < count) {
                                    u16 mt = static_cast<u16>(dataPtr[roomPos * 2]) |
                                             (static_cast<u16>(dataPtr[roomPos * 2 + 1]) << 8);
                                    outRoom.layerTop[mapPos] = mt;
                                }
                            }
                        }
                    }
                }

                cont = (data1 & 0x80000000) != 0;
                cur += 12;
            }
        }
    }

    // 5. Load Entities & TileEntities
    u32 areaEntityTableAddr = readAddr(m_offsets.areaMetadataBase + (area * 4));
    if (areaEntityTableAddr != 0) {
        u32 roomEntityTableAddr = readAddr(areaEntityTableAddr + (room * 4));
        if (roomEntityTableAddr != 0) {
            // Prop 0 (List 1)
            u32 prop0Offset = readAddr(roomEntityTableAddr + 0);
            outRoom.entitiesMain.clear();
            if (prop0Offset != 0) {
                u32 cur = prop0Offset;
                for (int i = 0; i < 256; ++i) {
                    u8 kind = read8(cur);
                    if (kind == 0xFF) break;
                    EntityData ent;
                    ent.kind = kind;
                    ent.flags = read8(cur + 1);
                    ent.id = read8(cur + 2);
                    ent.type = read8(cur + 3);
                    ent.type2 = read32(cur + 4);
                    ent.xPos = read16(cur + 8);
                    ent.yPos = read16(cur + 10);
                    ent.spritePtr = read32(cur + 12);
                    outRoom.entitiesMain.push_back(ent);
                    cur += 16;
                }
            }

            // Prop 1 (List 2)
            u32 prop1Offset = readAddr(roomEntityTableAddr + 4);
            outRoom.entitiesSecondary.clear();
            if (prop1Offset != 0) {
                u32 cur = prop1Offset;
                for (int i = 0; i < 256; ++i) {
                    u8 kind = read8(cur);
                    if (kind == 0xFF) break;
                    EntityData ent;
                    ent.kind = kind;
                    ent.flags = read8(cur + 1);
                    ent.id = read8(cur + 2);
                    ent.type = read8(cur + 3);
                    ent.type2 = read32(cur + 4);
                    ent.xPos = read16(cur + 8);
                    ent.yPos = read16(cur + 10);
                    ent.spritePtr = read32(cur + 12);
                    outRoom.entitiesSecondary.push_back(ent);
                    cur += 16;
                }
            }

            // Prop 2 (List 3)
            u32 prop2Offset = readAddr(roomEntityTableAddr + 8);
            outRoom.entitiesRespawn.clear();
            if (prop2Offset != 0) {
                u32 cur = prop2Offset;
                for (int i = 0; i < 256; ++i) {
                    u8 kind = read8(cur);
                    if (kind == 0xFF) break;
                    EntityData ent;
                    ent.kind = kind;
                    ent.flags = read8(cur + 1);
                    ent.id = read8(cur + 2);
                    ent.type = read8(cur + 3);
                    ent.type2 = read32(cur + 4);
                    ent.xPos = read16(cur + 8);
                    ent.yPos = read16(cur + 10);
                    ent.spritePtr = read32(cur + 12);
                    outRoom.entitiesRespawn.push_back(ent);
                    cur += 16;
                }
            }

            // Prop 3 (Chests / TileEntities)
            u32 prop3Offset = readAddr(roomEntityTableAddr + 12);
            outRoom.tileEntities.clear();
            if (prop3Offset != 0) {
                u32 cur = prop3Offset;
                for (int i = 0; i < 256; ++i) {
                    u8 type = read8(cur);
                    if (type == 0) break;
                    TileEntity te;
                    te.type = type;
                    te.localFlag = read8(cur + 1);
                    te.param2 = read8(cur + 2);
                    te.param3 = read8(cur + 3);
                    te.tilePos = read16(cur + 4);
                    te.param6 = read8(cur + 6);
                    te.param7 = read8(cur + 7);
                    outRoom.tileEntities.push_back(te);
                    cur += 8;
                }
            }
        }
    }

    // 6. Load Warps & Transitions
    u32 areaWarpTableAddr = readAddr(m_offsets.warpInformationTableLoc + (area * 4));
    if (areaWarpTableAddr != 0) {
        u32 roomWarpTableAddr = readAddr(areaWarpTableAddr + (room * 4));
        outRoom.transitions.clear();
        if (roomWarpTableAddr != 0) {
            u32 cur = roomWarpTableAddr;
            for (int i = 0; i < 64; ++i) {
                u16 wType = read16(cur);
                if (wType == 0xFFFF) break;
                Transition tr;
                tr.warp_type = wType;
                tr.startX = read16(cur + 2);
                tr.startY = read16(cur + 4);
                tr.endX = read16(cur + 6);
                tr.endY = read16(cur + 8);
                tr.shape = read8(cur + 10);
                tr.destArea = read8(cur + 11);
                tr.destRoom = read8(cur + 12);
                tr.playerLayer = read8(cur + 13);
                tr.transition_type = read8(cur + 14);
                tr.facing_direction = read8(cur + 15);
                tr.transitionSFX = read16(cur + 16);
                outRoom.transitions.push_back(tr);
                cur += 20;
            }
        }
    }

    return true;
}

u32 RomDataReader::getRoomMetadataOffset(u8 area, u8 room) const {
    if (!isLoaded()) return 0;
    u32 areaEntityTableAddr = readAddr(m_offsets.areaMetadataBase + (area * 4));
    if (areaEntityTableAddr == 0) return 0;
    return readAddr(areaEntityTableAddr + (room * 4));
}

u32 RomDataReader::getRoomTileDataOffset(u8 area, u8 room) const {
    if (!isLoaded()) return 0;
    u32 areaTileDataTableLoc = readAddr(m_offsets.globalTileDataTableLoc + (area * 4));
    if (areaTileDataTableLoc == 0) return 0;
    return readAddr(areaTileDataTableLoc + (room * 4));
}

u32 RomDataReader::getRoomHeaderOffset(u8 area, u8 room) const {
    if (!isLoaded()) return 0;
    u32 headerTableAddr = readAddr(m_offsets.mapHeaderBase + (area * 4));
    if (headerTableAddr == 0) return 0;
    return headerTableAddr + (room * 10);
}

int RomDataReader::getRoomCount(u8 area) const {
    if (!isLoaded() || area >= TOTAL_AREAS) return 0;
    u32 headerTableAddr = readAddr(m_offsets.mapHeaderBase + (area * 4));
    if (headerTableAddr == 0 || headerTableAddr >= m_romBytes.size()) return 0;

    int count = 0;
    u32 cur = headerTableAddr;
    while (cur + 2 <= m_romBytes.size() && count < static_cast<int>(MAX_ROOMS_PER_AREA)) {
        u16 termCheck = read16(cur);
        if (termCheck == 0xFFFF) {
            break;
        }
        if (cur + 10 > m_romBytes.size()) {
            break;
        }
        count++;
        cur += 10;
    }
    return count;
}

bool RomDataReader::loadTilesetOnly(u8 area, u16 tilesetId, TileSetData& outTileset) {
    if (!isLoaded() || area >= TOTAL_AREAS) {
        return false;
    }

    outTileset.id = tilesetId;
    outTileset.vramGfx.assign(0x10000, 0); // 64KB VRAM buffer

    int paletteSetId = 0;
    u32 areaTileSetTableLoc = readAddr(m_offsets.globalTileSetTableLoc + (area * 4));
    if (areaTileSetTableLoc != 0) {
        u32 roomTileSetLoc = readAddr(areaTileSetTableLoc + (tilesetId * 4));
        if (roomTileSetLoc != 0) {
            u32 cur = roomTileSetLoc;
            bool cont = true;
            while (cont && cur + 12 <= m_romBytes.size()) {
                u32 data1 = read32(cur);
                u32 data2 = read32(cur + 4);
                u32 data3 = read32(cur + 8);

                if (data2 == 0) {
                    paletteSetId = static_cast<int>(data1 & 0x7FFFFFFF);
                } else if ((data2 & 0x0F000000) == 0x06000000) { // VRAM destination
                    u32 source = (data1 & 0x7FFFFFFF) + m_offsets.gfxSourceBase;
                    u32 dest = data2 & 0x00FFFFFF;
                    bool compressed = (data3 & 0x80000000) != 0;
                    u32 size = data3 & 0x7FFFFFFF;

                    if (source < m_romBytes.size()) {
                        const u8* srcPtr = &m_romBytes[source];
                        size_t srcMax = m_romBytes.size() - source;

                        if (compressed || srcPtr[0] == 0x10) {
                            std::vector<u8> decomp = decompressLZ77(srcPtr, srcMax);
                            if (!decomp.empty()) {
                                size_t copyLen = std::min(decomp.size(), outTileset.vramGfx.size() - dest);
                                std::memcpy(outTileset.vramGfx.data() + dest, decomp.data(), copyLen);
                            }
                        } else {
                            size_t copyLen = std::min<size_t>(static_cast<size_t>(size) << 2, outTileset.vramGfx.size() - dest);
                            std::memcpy(outTileset.vramGfx.data() + dest, srcPtr, copyLen);
                        }
                    }
                }

                cont = (data1 & 0x80000000) != 0;
                cur += 12;
            }
        }
    }

    // Load Palettes
    loadPaletteSet(paletteSetId, outTileset.palette);

    // Load Metatiles Definitions (Set2)
    u32 metaTileSetsLoc = readAddr(m_offsets.globalMetaTileSetTableLoc + (area * 4));
    if (metaTileSetsLoc != 0) {
        u32 cur = metaTileSetsLoc;
        bool cont = true;
        while (cont && cur + 12 <= m_romBytes.size()) {
            u32 data1 = read32(cur);
            u32 data2 = read32(cur + 4);
            u32 data3 = read32(cur + 8);

            u32 source = (data1 & 0x7FFFFFFF) + m_offsets.gfxSourceBase;
            u32 dest = data2 & 0xFFFFFFFF;
            bool compressed = (data3 & 0x80000000) != 0;
            u32 size = data3 & 0x7FFFFFFF;

            if (source < m_romBytes.size()) {
                const u8* srcPtr = &m_romBytes[source];
                size_t srcMax = m_romBytes.size() - source;
                std::vector<u8> decomp;
                const u8* dataPtr = srcPtr;
                size_t dataLen = static_cast<size_t>(size) << 2;

                if (compressed || srcPtr[0] == 0x10) {
                    decomp = decompressLZ77(srcPtr, srcMax);
                    if (!decomp.empty()) {
                        dataPtr = decomp.data();
                        dataLen = decomp.size();
                    }
                }

                if (dest == 0x0202CEB4) {
                    outTileset.bg2MetaTiles.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x02012654) {
                    outTileset.bg1MetaTiles.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x0202AEB4) {
                    outTileset.bg2MetaTypes.assign(dataPtr, dataPtr + dataLen);
                } else if (dest == 0x02010654) {
                    outTileset.bg1MetaTypes.assign(dataPtr, dataPtr + dataLen);
                }
            }

            cont = (data1 & 0x80000000) != 0;
            cur += 12;
        }
    }

    // Parse Metatiles into structured arrays
    outTileset.metaTilesBottom.clear();
    size_t numBg2Metas = outTileset.bg2MetaTiles.size() / 8;
    outTileset.metaTilesBottom.resize(numBg2Metas);
    for (size_t m = 0; m < numBg2Metas; ++m) {
        for (size_t s = 0; s < 4; ++s) {
            u16 st = static_cast<u16>(outTileset.bg2MetaTiles[m * 8 + s * 2]) |
                     (static_cast<u16>(outTileset.bg2MetaTiles[m * 8 + s * 2 + 1]) << 8);
            outTileset.metaTilesBottom[m].subTiles[s].raw = st;
        }
    }

    outTileset.metaTilesTop.clear();
    size_t numBg1Metas = outTileset.bg1MetaTiles.size() / 8;
    outTileset.metaTilesTop.resize(numBg1Metas);
    for (size_t m = 0; m < numBg1Metas; ++m) {
        for (size_t s = 0; s < 4; ++s) {
            u16 st = static_cast<u16>(outTileset.bg1MetaTiles[m * 8 + s * 2]) |
                     (static_cast<u16>(outTileset.bg1MetaTiles[m * 8 + s * 2 + 1]) << 8);
            outTileset.metaTilesTop[m].subTiles[s].raw = st;
        }
    }

    return true;
}

} // namespace tmc
