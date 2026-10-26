#pragma once

#include "core/tmc_types.hpp"
#include <SDL3/SDL.h>
#include <vector>
#include <memory>

namespace tmc {

struct RenderOptions {
    bool showLayerBottom = true;
    bool showLayerTop = true;
    bool showCollision = false;
    bool showActTiles = false;
    bool showGrid16 = false;
    bool showGrid8 = false;
    bool showEntities = true;
    bool showTileEntities = true;
    bool showTransitions = false;
    float collisionOpacity = 0.55f;
    float zoom = 2.0f;
    int selectedEntityList = -1;  // 0=Main, 1=Secondary, 2=Respawn
    int selectedEntityIndex = -1;
    int selectedTransitionIndex = -1;
};

class TileRenderer {
public:
    TileRenderer();
    ~TileRenderer();

    bool init(SDL_Renderer* renderer);
    void destroy();

    // Rebuild metatile atlas textures for BG2 (bottom) and BG1 (top)
    void updateTilesetGfx(const TileSetData& tileset);

    // Render full room to target texture
    SDL_Texture* renderRoomToTexture(const RoomModel& room, const TileSetData& tileset, const RenderOptions& opts);

    // Atlases for UI Picker
    SDL_Texture* getCanvasTexture() const { return m_canvasTexture; }
    SDL_Texture* getAtlasBottomTexture() const { return m_atlasBottomTexture; }
    SDL_Texture* getAtlasTopTexture() const { return m_atlasTopTexture; }
    int getAtlasColumns() const { return m_atlasCols; }
    int getAtlasRows() const { return m_atlasRows; }
    int getAtlasWidth() const { return m_atlasCols * 16; }
    int getAtlasHeight() const { return m_atlasRows * 16; }
    int getCanvasWidth() const { return m_canvasWidth; }
    int getCanvasHeight() const { return m_canvasHeight; }

    // Color helpers for collision overlay
    static ColorRGBA getCollisionColor(u8 colType);
    static const char* getCollisionName(u8 colType);

private:
    SDL_Renderer* m_sdlRenderer = nullptr;
    SDL_Texture*  m_canvasTexture = nullptr;
    SDL_Texture*  m_atlasBottomTexture = nullptr;
    SDL_Texture*  m_atlasTopTexture = nullptr;

    std::vector<u8> m_canvasPixels;
    std::vector<u8> m_atlasBottomPixels;
    std::vector<u8> m_atlasTopPixels;

    int m_canvasWidth = 1024;
    int m_canvasHeight = 1024;

    int m_atlasCols = 16;
    int m_atlasRows = 64; // up to 1024 metatiles

    u16 m_cachedTilesetId = 0xFFFF;
    size_t m_cachedGfxHash = 0;

    void buildAtlas(SDL_Texture* targetAtlas, std::vector<u8>& pixelBuffer,
                    const std::vector<u8>& metaTileBytes, const std::vector<u8>& vramGfx,
                    const FullPalette& palette, bool isBg1);
};

} // namespace tmc
