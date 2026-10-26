#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace tmc {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;

constexpr size_t MAX_MAP_WIDTH = 64;
constexpr size_t MAX_MAP_HEIGHT = 64;
constexpr size_t MAX_MAP_TILES = MAX_MAP_WIDTH * MAX_MAP_HEIGHT;
constexpr size_t MAX_ROOMS_PER_AREA = 64;
constexpr size_t TOTAL_AREAS = 0x90;

// Subtile in GBA: 8x8 pixels, 4bpp.
struct SubTile {
    u16 raw = 0;

    u16 tileIndex() const { return raw & 0x3FF; }
    void setTileIndex(u16 idx) { raw = (raw & ~0x3FF) | (idx & 0x3FF); }

    bool hFlip() const { return (raw & 0x400) != 0; }
    void setHFlip(bool flip) { if (flip) raw |= 0x400; else raw &= ~0x400; }

    bool vFlip() const { return (raw & 0x800) != 0; }
    void setVFlip(bool flip) { if (flip) raw |= 0x800; else raw &= ~0x800; }

    u8 palette() const { return (raw >> 12) & 0x0F; }
    void setPalette(u8 pal) { raw = (raw & 0x0FFF) | ((pal & 0x0F) << 12); }
};

// Metatile: 16x16 pixels, composed of 4 SubTiles (TL, TR, BL, BR)
struct MetaTile {
    std::array<SubTile, 4> subTiles;
    u16 tileType = 0;
};

// Collision flags
enum CollisionType : u8 {
    COL_NONE = 0x00,
    COL_SOLID = 0x01,
    COL_WATER_DEEP = 0x02,
    COL_WATER_SHALLOW = 0x03,
    COL_HOLE = 0x04,
    COL_LEDGE_DOWN = 0x05,
    COL_LEDGE_UP = 0x06,
    COL_LEDGE_LEFT = 0x07,
    COL_LEDGE_RIGHT = 0x08,
    COL_STAIRS = 0x09,
    COL_SLOPE_NW = 0x0A,
    COL_SLOPE_NE = 0x0B,
    COL_SLOPE_SW = 0x0C,
    COL_SLOPE_SE = 0x0D,
    COL_MINISH_ONLY = 0x0E,
    COL_ICE = 0x0F,
    COL_CUSTOM = 0x10,
};

// Room Header
struct RoomHeader {
    u16 map_x = 0;
    u16 map_y = 0;
    u16 pixel_width = 256;
    u16 pixel_height = 256;
    u16 tileSet_id = 0;

    int tileWidth() const { return std::max(1, static_cast<int>(pixel_width) / 16); }
    int tileHeight() const { return std::max(1, static_cast<int>(pixel_height) / 16); }
};

// Entity Data (Prop 0, 1, 2)
struct EntityData {
    u8 kind = 0;
    u8 flags = 0;
    u8 id = 0;
    u8 type = 0;
    u32 type2 = 0;
    u16 xPos = 0;
    u16 yPos = 0;
    u32 spritePtr = 0;
};

// TileEntity (Prop 3)
enum TileEntityType : u8 {
    TE_NONE = 0,
    TE_ROOM_VISIT_MARKER = 1,
    TE_SMALL_CHEST = 2,
    TE_BIG_CHEST = 3,
    TE_BOMBABLE_WALL = 4,
    TE_SIGN = 5,
    TE_TILE_EZLO_HINT = 6,
    TE_MUSIC_SETTER = 7,
    TE_TILE_ENTITY_8 = 8,
    TE_DARKNESS = 9,
    TE_DESTRUCTIBLE_TILE = 10,
    TE_GRASS_DROP_CHANGER = 11,
    TE_LOCATION_CHANGER = 12,
    TE_TILE_ENTITY_D = 13,
};

struct TileEntity {
    u8 type = 0;
    u8 localFlag = 0;
    u8 param2 = 0;
    u8 param3 = 0;
    u16 tilePos = 0;
    u8 param6 = 0;
    u8 param7 = 0;
};

// Minish Portals
struct PortalInfo {
    bool in_use = false;
    u8 type = 0;
    u16 x = 0;
    u16 y = 0;
    u8 exit_dir = 0;
    u8 mode = 0;
};

// Exits & Transitions
struct Transition {
    u16 warp_type = 0;
    u16 startX = 0;
    u16 startY = 0;
    u16 endX = 0;
    u16 endY = 0;
    u8 shape = 0;
    u8 destArea = 0;
    u8 destRoom = 0;
    u8 playerLayer = 1;
    u8 transition_type = 0;
    u8 facing_direction = 0;
    u16 transitionSFX = 0;
};

// Color RGBA
struct ColorRGBA {
    u8 r = 0, g = 0, b = 0, a = 255;
};

using SubPalette = std::array<ColorRGBA, 16>;
using FullPalette = std::array<SubPalette, 16>;

// Full Tileset
struct TileSetData {
    u16 id = 0;
    std::string name;

    std::vector<u8> vramGfx;       // 64KB raw GBA 4bpp tile data (0x10000 bytes = 2048 8x8 tiles)
    std::vector<u8> bg2MetaTiles;  // Raw Metatile definitions for Bottom Layer (8 bytes per metatile)
    std::vector<u8> bg1MetaTiles;  // Raw Metatile definitions for Top Layer (8 bytes per metatile)
    std::vector<u8> bg2MetaTypes;  // Metatile type data for BG2
    std::vector<u8> bg1MetaTypes;  // Metatile type data for BG1

    std::vector<MetaTile> metaTilesBottom; // Parsed Metatiles (BG2)
    std::vector<MetaTile> metaTilesTop;    // Parsed Metatiles (BG1)

    FullPalette palette;

    TileSetData() {
        vramGfx.resize(0x10000, 0);
    }
};

// Complete Room Definition
struct RoomModel {
    u8 areaIndex = 0;
    u8 roomIndex = 0;
    std::string name;

    RoomHeader header;
    PortalInfo portal;

    bool bg2Exists = false;
    bool bg1Exists = false;

    // Room tile grids (16-bit metatile index per cell: tileWidth x tileHeight)
    std::vector<u16> layerBottom; // BG2 (Ground)
    std::vector<u16> layerTop;    // BG1 (Overhead)
    std::vector<u8>  collision;   // Collision flags
    std::vector<u8>  actTiles;

    // Entities & Props
    std::vector<EntityData> entitiesMain;
    std::vector<EntityData> entitiesSecondary;
    std::vector<EntityData> entitiesRespawn;
    std::vector<TileEntity> tileEntities;
    std::vector<Transition> transitions;

    RoomModel() {
        layerBottom.resize(MAX_MAP_TILES, 0);
        layerTop.resize(MAX_MAP_TILES, 0);
        collision.resize(MAX_MAP_TILES, 0);
        actTiles.resize(MAX_MAP_TILES, 0);
    }
};

} // namespace tmc
