#include "tile_renderer.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace tmc {

TileRenderer::TileRenderer() = default;

TileRenderer::~TileRenderer() {
    destroy();
}

bool TileRenderer::init(SDL_Renderer* renderer) {
    m_sdlRenderer = renderer;
    if (!m_sdlRenderer) return false;

    m_canvasPixels.assign(m_canvasWidth * m_canvasHeight * 4, 0);

    int atlasW = m_atlasCols * 16;
    int atlasH = m_atlasRows * 16;
    m_atlasBottomPixels.assign(atlasW * atlasH * 4, 0);
    m_atlasTopPixels.assign(atlasW * atlasH * 4, 0);

    // 1024x1024 Streaming Canvas texture in RGBA32
    m_canvasTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_STREAMING, m_canvasWidth, m_canvasHeight);
    if (!m_canvasTexture) {
        std::cerr << "[TileRenderer] Failed to create canvas texture: " << SDL_GetError() << std::endl;
        return false;
    }

    m_atlasBottomTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING, atlasW, atlasH);
    m_atlasTopTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STREAMING, atlasW, atlasH);

    SDL_SetTextureBlendMode(m_canvasTexture, SDL_BLENDMODE_BLEND);
    if (m_atlasBottomTexture) SDL_SetTextureBlendMode(m_atlasBottomTexture, SDL_BLENDMODE_BLEND);
    if (m_atlasTopTexture) SDL_SetTextureBlendMode(m_atlasTopTexture, SDL_BLENDMODE_BLEND);

    return true;
}

void TileRenderer::destroy() {
    if (m_canvasTexture) {
        SDL_DestroyTexture(m_canvasTexture);
        m_canvasTexture = nullptr;
    }
    if (m_atlasBottomTexture) {
        SDL_DestroyTexture(m_atlasBottomTexture);
        m_atlasBottomTexture = nullptr;
    }
    if (m_atlasTopTexture) {
        SDL_DestroyTexture(m_atlasTopTexture);
        m_atlasTopTexture = nullptr;
    }
    m_canvasPixels.clear();
    m_atlasBottomPixels.clear();
    m_atlasTopPixels.clear();
}

ColorRGBA TileRenderer::getCollisionColor(u8 colType) {
    switch (colType) {
        case COL_NONE:           return ColorRGBA{ 0, 0, 0, 0 };
        case COL_SOLID:          return ColorRGBA{ 235, 50, 50, 180 };   // Red
        case COL_WATER_DEEP:     return ColorRGBA{ 30, 100, 240, 180 };  // Deep Blue
        case COL_WATER_SHALLOW:  return ColorRGBA{ 80, 180, 255, 180 };  // Light Blue
        case COL_HOLE:           return ColorRGBA{ 20, 20, 20, 220 };    // Pitch Black
        case COL_LEDGE_DOWN:     return ColorRGBA{ 255, 200, 40, 180 };  // Yellow Down
        case COL_LEDGE_UP:       return ColorRGBA{ 255, 150, 20, 180 };  // Orange Up
        case COL_LEDGE_LEFT:     return ColorRGBA{ 230, 120, 230, 180 }; // Purple Left
        case COL_LEDGE_RIGHT:    return ColorRGBA{ 180, 80, 220, 180 };  // Purple Right
        case COL_STAIRS:         return ColorRGBA{ 50, 220, 120, 180 };  // Green Stairs
        case COL_SLOPE_NW:       return ColorRGBA{ 200, 80, 80, 180 };
        case COL_SLOPE_NE:       return ColorRGBA{ 200, 80, 80, 180 };
        case COL_SLOPE_SW:       return ColorRGBA{ 200, 80, 80, 180 };
        case COL_SLOPE_SE:       return ColorRGBA{ 200, 80, 80, 180 };
        case COL_MINISH_ONLY:    return ColorRGBA{ 0, 255, 200, 180 };
        case COL_ICE:            return ColorRGBA{ 180, 240, 255, 180 };
        default:                 return ColorRGBA{ 128, 128, 128, 150 };
    }
}

const char* TileRenderer::getCollisionName(u8 colType) {
    switch (colType) {
        case COL_NONE:          return "Walkable (None)";
        case COL_SOLID:         return "Solid Wall / Obstacle";
        case COL_WATER_DEEP:    return "Deep Water (Swim/Drown)";
        case COL_WATER_SHALLOW: return "Shallow Water (Wade)";
        case COL_HOLE:          return "Pit / Hole (Fall)";
        case COL_LEDGE_DOWN:    return "Jump Ledge (South)";
        case COL_LEDGE_UP:      return "Jump Ledge (North)";
        case COL_LEDGE_LEFT:    return "Jump Ledge (West)";
        case COL_LEDGE_RIGHT:   return "Jump Ledge (East)";
        case COL_STAIRS:        return "Stairs";
        case COL_SLOPE_NW:      return "Slope North-West";
        case COL_SLOPE_NE:      return "Slope North-East";
        case COL_SLOPE_SW:      return "Slope South-West";
        case COL_SLOPE_SE:      return "Slope South-East";
        case COL_MINISH_ONLY:   return "Minish Hole Only";
        case COL_ICE:           return "Ice (Slippery)";
        default:                return "Custom Flag";
    }
}

static void drawSubTileFast(u8* targetPixels, int targetWidth, int targetHeight, int targetPitch,
                           int destX, int destY, int tnum, const FullPalette& palette, int pnum,
                           bool hflip, bool vflip, const std::vector<u8>& vramGfx, bool isBg1) {
    if (isBg1) {
        tnum += 0x200;
    }
    size_t tileOffset = static_cast<size_t>(tnum) * 32;
    if (tileOffset + 32 > vramGfx.size()) return;

    const u8* rawTile = &vramGfx[tileOffset];
    const auto& pal = palette[pnum];

    int dataPos = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; x += 2) {
            int posX = hflip ? (6 - x) : x;
            int posY = vflip ? (7 - y) : y;
            int px = destX + posX;
            int py = destY + posY;

            u8 colorData = rawTile[dataPos++];
            int data1 = hflip ? (colorData >> 4) : (colorData & 0x0F);
            int data2 = hflip ? (colorData & 0x0F) : (colorData >> 4);

            if (data1 != 0) {
                const ColorRGBA& c1 = pal[data1];
                if (c1.a > 0 && px >= 0 && px < targetWidth && py >= 0 && py < targetHeight) {
                    u8* p = targetPixels + (py * targetPitch) + (px * 4);
                    p[0] = c1.r;
                    p[1] = c1.g;
                    p[2] = c1.b;
                    p[3] = 255;
                }
            }

            if (data2 != 0) {
                const ColorRGBA& c2 = pal[data2];
                if (c2.a > 0 && (px + 1) >= 0 && (px + 1) < targetWidth && py >= 0 && py < targetHeight) {
                    u8* p = targetPixels + (py * targetPitch) + ((px + 1) * 4);
                    p[0] = c2.r;
                    p[1] = c2.g;
                    p[2] = c2.b;
                    p[3] = 255;
                }
            }
        }
    }
}

static void drawMetaTileFast(u8* targetPixels, int targetWidth, int targetHeight, int targetPitch,
                            int destX, int destY, const u8* metaTileData,
                            const FullPalette& palette, const std::vector<u8>& vramGfx, bool isBg1) {
    if (!metaTileData) return;
    const int subOffX[4] = { 0, 8, 0, 8 };
    const int subOffY[4] = { 0, 0, 8, 8 };

    for (int s = 0; s < 4; ++s) {
        u16 data = static_cast<u16>(metaTileData[s * 2]) |
                   (static_cast<u16>(metaTileData[s * 2 + 1]) << 8);

        int tnum = data & 0x3FF;
        bool hflip = ((data >> 10) & 1) != 0;
        bool vflip = ((data >> 11) & 1) != 0;
        int pnum = (data >> 12) & 0x0F;

        drawSubTileFast(targetPixels, targetWidth, targetHeight, targetPitch,
                        destX + subOffX[s], destY + subOffY[s],
                        tnum, palette, pnum, hflip, vflip, vramGfx, isBg1);
    }
}

void TileRenderer::buildAtlas(SDL_Texture* targetAtlas, std::vector<u8>& pixelBuffer,
                             const std::vector<u8>& metaTileBytes, const std::vector<u8>& vramGfx,
                             const FullPalette& palette, bool isBg1) {
    if (!targetAtlas) return;

    int atlasW = m_atlasCols * 16;
    int atlasH = m_atlasRows * 16;
    int pitch = atlasW * 4;

    std::memset(pixelBuffer.data(), 0, pixelBuffer.size());

    size_t numMetatiles = metaTileBytes.size() / 8;
    numMetatiles = std::min<size_t>(numMetatiles, static_cast<size_t>(m_atlasCols * m_atlasRows));

    for (size_t m = 0; m < numMetatiles; ++m) {
        int metaTileX = static_cast<int>(m % m_atlasCols) * 16;
        int metaTileY = static_cast<int>(m / m_atlasCols) * 16;

        const u8* metaPtr = &metaTileBytes[m * 8];
        drawMetaTileFast(pixelBuffer.data(), atlasW, atlasH, pitch,
                         metaTileX, metaTileY, metaPtr, palette, vramGfx, isBg1);
    }

    SDL_UpdateTexture(targetAtlas, nullptr, pixelBuffer.data(), pitch);
}

void TileRenderer::updateTilesetGfx(const TileSetData& tileset) {
    buildAtlas(m_atlasBottomTexture, m_atlasBottomPixels, tileset.bg2MetaTiles, tileset.vramGfx, tileset.palette, false);
    buildAtlas(m_atlasTopTexture, m_atlasTopPixels, tileset.bg1MetaTiles, tileset.vramGfx, tileset.palette, true);
    m_cachedTilesetId = tileset.id;
    m_cachedGfxHash = tileset.bg2MetaTiles.size() + tileset.bg1MetaTiles.size() + tileset.vramGfx.size();
}

SDL_Texture* TileRenderer::renderRoomToTexture(const RoomModel& room, const TileSetData& tileset, const RenderOptions& opts) {
    if (!m_canvasTexture) return nullptr;

    size_t currentHash = tileset.bg2MetaTiles.size() + tileset.bg1MetaTiles.size() + tileset.vramGfx.size();
    if (m_cachedTilesetId != tileset.id || m_cachedGfxHash != currentHash) {
        updateTilesetGfx(tileset);
    }

    int targetPitch = m_canvasWidth * 4;
    // Clear canvas to dark backdrop
    for (int y = 0; y < m_canvasHeight; ++y) {
        for (int x = 0; x < m_canvasWidth; ++x) {
            u8* p = m_canvasPixels.data() + (y * targetPitch) + (x * 4);
            p[0] = 18;
            p[1] = 18;
            p[2] = 24;
            p[3] = 255;
        }
    }

    int tWidth = room.header.tileWidth();
    int tHeight = room.header.tileHeight();

    // 1. Draw Bottom Ground Layer (BG2)
    if (opts.showLayerBottom && !tileset.bg2MetaTiles.empty()) {
        size_t totalMetas = tileset.bg2MetaTiles.size() / 8;
        for (int ty = 0; ty < tHeight; ++ty) {
            for (int tx = 0; tx < tWidth; ++tx) {
                size_t mapPos = ty * MAX_MAP_WIDTH + tx;
                u16 metaId = (mapPos < room.layerBottom.size()) ? room.layerBottom[mapPos] : 0;
                if (metaId == 0xFFFF || metaId >= totalMetas) continue;

                const u8* metaPtr = &tileset.bg2MetaTiles[metaId * 8];
                drawMetaTileFast(m_canvasPixels.data(), m_canvasWidth, m_canvasHeight, targetPitch,
                                tx * 16, ty * 16, metaPtr, tileset.palette, tileset.vramGfx, false);
            }
        }
    }

    // 2. Draw Top Overhead Layer (BG1)
    if (opts.showLayerTop && !tileset.bg1MetaTiles.empty()) {
        size_t totalMetas = tileset.bg1MetaTiles.size() / 8;
        for (int ty = 0; ty < tHeight; ++ty) {
            for (int tx = 0; tx < tWidth; ++tx) {
                size_t mapPos = ty * MAX_MAP_WIDTH + tx;
                u16 metaId = (mapPos < room.layerTop.size()) ? room.layerTop[mapPos] : 0;
                if (metaId == 0 || metaId == 0xFFFF || metaId >= totalMetas) continue;

                const u8* metaPtr = &tileset.bg1MetaTiles[metaId * 8];
                drawMetaTileFast(m_canvasPixels.data(), m_canvasWidth, m_canvasHeight, targetPitch,
                                tx * 16, ty * 16, metaPtr, tileset.palette, tileset.vramGfx, true);
            }
        }
    }

    // 3. Collision Overlay
    if (opts.showCollision) {
        for (int ty = 0; ty < tHeight; ++ty) {
            for (int tx = 0; tx < tWidth; ++tx) {
                size_t idx = ty * MAX_MAP_WIDTH + tx;
                if (idx < room.collision.size()) {
                    u8 col = room.collision[idx];
                    if (col != COL_NONE) {
                        ColorRGBA c = getCollisionColor(col);
                        u8 alpha = static_cast<u8>(c.a * opts.collisionOpacity);
                        for (int py = 0; py < 16; ++py) {
                            for (int px = 0; px < 16; ++px) {
                                int cx = tx * 16 + px;
                                int cy = ty * 16 + py;
                                if (cx < m_canvasWidth && cy < m_canvasHeight) {
                                    u8* p = m_canvasPixels.data() + (cy * targetPitch) + (cx * 4);
                                    p[0] = static_cast<u8>((p[0] * (255 - alpha) + c.r * alpha) / 255);
                                    p[1] = static_cast<u8>((p[1] * (255 - alpha) + c.g * alpha) / 255);
                                    p[2] = static_cast<u8>((p[2] * (255 - alpha) + c.b * alpha) / 255);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Grid Lines (16x16)
    if (opts.showGrid16) {
        int maxPxW = std::min<int>(room.header.pixel_width, m_canvasWidth);
        int maxPxH = std::min<int>(room.header.pixel_height, m_canvasHeight);
        for (int x = 0; x <= maxPxW; x += 16) {
            if (x >= m_canvasWidth) break;
            for (int y = 0; y < maxPxH; ++y) {
                u8* p = m_canvasPixels.data() + (y * targetPitch) + (x * 4);
                p[0] = static_cast<u8>((p[0] * 3 + 255) / 4);
                p[1] = static_cast<u8>((p[1] * 3 + 255) / 4);
                p[2] = static_cast<u8>((p[2] * 3 + 255) / 4);
            }
        }
        for (int y = 0; y <= maxPxH; y += 16) {
            if (y >= m_canvasHeight) break;
            for (int x = 0; x < maxPxW; ++x) {
                u8* p = m_canvasPixels.data() + (y * targetPitch) + (x * 4);
                p[0] = static_cast<u8>((p[0] * 3 + 255) / 4);
                p[1] = static_cast<u8>((p[1] * 3 + 255) / 4);
                p[2] = static_cast<u8>((p[2] * 3 + 255) / 4);
            }
        }
    }

    // 5. Entities (Props 0, 1, 2)
    if (opts.showEntities) {
        auto drawEntityDot = [this, targetPitch](const EntityData& ent, u8 cr, u8 cg, u8 cb, u8 outerR, u8 outerG, u8 outerB, bool isSelected) {
            int cx = ent.xPos;
            int cy = ent.yPos;
            int radius = isSelected ? 9 : 7;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < m_canvasWidth && py >= 0 && py < m_canvasHeight) {
                        u8* p = m_canvasPixels.data() + (py * targetPitch) + (px * 4);
                        if (isSelected && (std::abs(dx) == radius || std::abs(dy) == radius || std::abs(dx) == radius - 1 || std::abs(dy) == radius - 1)) {
                            p[0] = 255; p[1] = 255; p[2] = 255; // Thick white highlight frame for selected entity
                        } else if (std::abs(dx) == 7 || std::abs(dy) == 7) {
                            p[0] = outerR; p[1] = outerG; p[2] = outerB; // Crisp outline
                        } else if (std::abs(dx) == 6 || std::abs(dy) == 6) {
                            p[0] = 255; p[1] = 255; p[2] = 255; // White inner border
                        } else if (dx == 0 || dy == 0) {
                            p[0] = 255; p[1] = 255; p[2] = 255; // Center crosshair
                        } else {
                            p[0] = cr; p[1] = cg; p[2] = cb;
                        }
                    }
                }
            }
        };

        // Render Prop 0 (entitiesMain)
        for (size_t i = 0; i < room.entitiesMain.size(); ++i) {
            const auto& ent = room.entitiesMain[i];
            bool isSel = (opts.selectedEntityList == 0 && opts.selectedEntityIndex == static_cast<int>(i));
            u8 cr = 240, cg = 50, cb = 50;
            if (ent.kind == 1) { cr = 50; cg = 220; cb = 80; }       // Player
            else if (ent.kind == 3) { cr = 230; cg = 50; cb = 50; }  // Enemy
            else if (ent.kind == 4) { cr = 255; cg = 140; cb = 0; }  // Projectile
            else if (ent.kind == 6) { cr = 230; cg = 180; cb = 20; } // Object
            else if (ent.kind == 7) { cr = 40; cg = 180; cb = 240; } // NPC
            else if (ent.kind == 9) { cr = 180; cg = 60; cb = 240; } // Manager
            drawEntityDot(ent, cr, cg, cb, 20, 20, 20, isSel);
        }

        // Render Prop 1 (entitiesSecondary - Story / Secondary)
        for (size_t i = 0; i < room.entitiesSecondary.size(); ++i) {
            const auto& ent = room.entitiesSecondary[i];
            bool isSel = (opts.selectedEntityList == 1 && opts.selectedEntityIndex == static_cast<int>(i));
            drawEntityDot(ent, 50, 180, 220, 20, 20, 20, isSel);
        }

        // Render Prop 2 (entitiesRespawn - Enemies / Respawn)
        for (size_t i = 0; i < room.entitiesRespawn.size(); ++i) {
            const auto& ent = room.entitiesRespawn[i];
            bool isSel = (opts.selectedEntityList == 2 && opts.selectedEntityIndex == static_cast<int>(i));
            drawEntityDot(ent, 255, 40, 40, 20, 20, 20, isSel);
        }
    }

    // 6. TileEntities (Chests)
    if (opts.showTileEntities) {
        for (const auto& te : room.tileEntities) {
            int tx = te.tilePos % MAX_MAP_WIDTH;
            int ty = te.tilePos / MAX_MAP_WIDTH;
            int cx = tx * 16 + 8;
            int cy = ty * 16 + 8;
            for (int dy = -5; dy <= 5; ++dy) {
                for (int dx = -5; dx <= 5; ++dx) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < m_canvasWidth && py >= 0 && py < m_canvasHeight) {
                        u8* p = m_canvasPixels.data() + (py * targetPitch) + (px * 4);
                        if (std::abs(dx) == 5 || std::abs(dy) == 5) {
                            p[0] = 0; p[1] = 100; p[2] = 200;
                        } else {
                            p[0] = 50; p[1] = 200; p[2] = 255;
                        }
                    }
                }
            }
        }
    }

    // 7. Transitions (Exits) - Rendered with exact engine shape dimensions and border zones
    if (opts.showTransitions) {
        // GBA Engine transition shape dimensions: {half_w, half_h}
        // Shape 0: 12x12 (6, 6)
        // Shape 1: 12x28 (6, 14)
        // Shape 2: 28x12 (14, 6)
        // Shape 3: 44x12 (22, 6)
        static const int sShapeDims[4][2] = {
            { 6, 6 },
            { 6, 14 },
            { 14, 6 },
            { 22, 6 }
        };

        for (size_t i = 0; i < room.transitions.size(); ++i) {
            const auto& tr = room.transitions[i];
            bool isSelected = (opts.selectedTransitionIndex == static_cast<int>(i));

            bool isBorder = (tr.warp_type == 0 || tr.warp_type == 2); // WARP_TYPE_BORDER or WARP_TYPE_BORDER2

            if (isBorder) {
                // Border Transitions trigger along the edges of the room
                // Shape bitmask indicates which quadrant/half of which border:
                // North: 0x01 (NW), 0x02 (NE), 0x03 (Both)
                // East:  0x04 (EN), 0x08 (ES), 0x0C (Both)
                // South: 0x10 (SW), 0x20 (SE), 0x30 (Both)
                // West:  0x40 (WN), 0x80 (WS), 0xC0 (Both)
                int borderThickness = 12;
                int roomW = room.header.pixel_width;
                int roomH = room.header.pixel_height;
                int midX = roomW / 2;
                int midY = roomH / 2;

                auto drawBorderBox = [&](int x0, int y0, int x1, int y1) {
                    for (int py = y0; py <= y1; ++py) {
                        for (int px = x0; px <= x1; ++px) {
                            if (px >= 0 && px < m_canvasWidth && py >= 0 && py < m_canvasHeight) {
                                u8* p = m_canvasPixels.data() + (py * targetPitch) + (px * 4);
                                if (isSelected) {
                                    if (px == x0 || px == x1 || py == y0 || py == y1) {
                                        p[0] = 255; p[1] = 255; p[2] = 255; // White border
                                    } else {
                                        p[0] = static_cast<u8>(p[0] * 0.4f + 255 * 0.6f);
                                        p[1] = static_cast<u8>(p[1] * 0.4f + 200 * 0.6f);
                                        p[2] = static_cast<u8>(p[2] * 0.4f + 50 * 0.6f);
                                    }
                                } else {
                                    if (px == x0 || px == x1 || py == y0 || py == y1) {
                                        p[0] = 50; p[1] = 180; p[2] = 255; // Blue border
                                    } else {
                                        p[0] = static_cast<u8>(p[0] * 0.7f + 50 * 0.3f);
                                        p[1] = static_cast<u8>(p[1] * 0.7f + 140 * 0.3f);
                                        p[2] = static_cast<u8>(p[2] * 0.7f + 240 * 0.3f);
                                    }
                                }
                            }
                        }
                    }
                };

                if (tr.shape & 0x01) drawBorderBox(0, 0, midX, borderThickness);
                if (tr.shape & 0x02) drawBorderBox(midX, 0, roomW, borderThickness);
                if (tr.shape & 0x04) drawBorderBox(roomW - borderThickness, 0, roomW, midY);
                if (tr.shape & 0x08) drawBorderBox(roomW - borderThickness, midY, roomW, roomH);
                if (tr.shape & 0x10) drawBorderBox(0, roomH - borderThickness, midX, roomH);
                if (tr.shape & 0x20) drawBorderBox(midX, roomH - borderThickness, roomW, roomH);
                if (tr.shape & 0x40) drawBorderBox(0, 0, borderThickness, midY);
                if (tr.shape & 0x80) drawBorderBox(0, midY, borderThickness, roomH);

            } else {
                // Area Transitions: Trigger centered at (startX, startY) with shape half-extents
                int shapeIdx = tr.shape & 3;
                int halfW = sShapeDims[shapeIdx][0];
                int halfH = sShapeDims[shapeIdx][1];

                int sx = tr.startX - halfW;
                int sy = tr.startY - halfH;
                int ex = tr.startX + halfW;
                int ey = tr.startY + halfH;

                for (int py = sy; py <= ey; ++py) {
                    for (int px = sx; px <= ex; ++px) {
                        if (px >= 0 && px < m_canvasWidth && py >= 0 && py < m_canvasHeight) {
                            u8* p = m_canvasPixels.data() + (py * targetPitch) + (px * 4);
                            if (isSelected) {
                                if (px == sx || px == ex || py == sy || py == ey ||
                                    px == sx + 1 || px == ex - 1 || py == sy + 1 || py == ey - 1) {
                                    p[0] = 255; p[1] = 255; p[2] = 255; // Thick white border
                                } else if (px == tr.startX || py == tr.startY) {
                                    p[0] = 255; p[1] = 255; p[2] = 0; // Yellow center cross
                                } else {
                                    p[0] = static_cast<u8>(p[0] * 0.4f + 255 * 0.6f);
                                    p[1] = static_cast<u8>(p[1] * 0.4f + 215 * 0.6f);
                                    p[2] = static_cast<u8>(p[2] * 0.4f + 0 * 0.6f);
                                }
                            } else {
                                if (px == sx || px == ex || py == sy || py == ey) {
                                    p[0] = 50; p[1] = 255; p[2] = 100; // Crisp green border
                                } else if (px == tr.startX || py == tr.startY) {
                                    p[0] = 255; p[1] = 255; p[2] = 255; // White crosshair
                                } else {
                                    p[0] = static_cast<u8>(p[0] * 0.6f + 50 * 0.4f);
                                    p[1] = static_cast<u8>(p[1] * 0.6f + 220 * 0.4f);
                                    p[2] = static_cast<u8>(p[2] * 0.6f + 80 * 0.4f);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Upload CPU raster to GPU texture
    SDL_UpdateTexture(m_canvasTexture, nullptr, m_canvasPixels.data(), targetPitch);
    return m_canvasTexture;
}

} // namespace tmc
