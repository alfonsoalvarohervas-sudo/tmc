#pragma once

#include "tmc_types.hpp"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>
#include <initializer_list>

namespace tmc {

class RomDataReader;

struct SpritePartInfo {
    u16 spriteIndex = 0;
    u16 paletteIndex = 0;
    u16 gfxIndex = 0;
    u8 frameIndex = 0;
    s8 xOffset = 0;
    s8 yOffset = 0;
};

struct SpriteInfo {
    u16 spriteIndex = 0;
    u16 paletteIndex = 0;
    u16 gfxIndex = 0;
    u8 frameIndex = 0;
    u8 partCount = 1;
    SpritePartInfo parts[4];

    SpriteInfo() = default;

    SpriteInfo(u16 sp, u16 pal, u16 gfx, u8 frame = 0, s8 xo = 0, s8 yo = 0)
        : spriteIndex(sp), paletteIndex(pal), gfxIndex(gfx), frameIndex(frame), partCount(1)
    {
        parts[0] = { sp, pal, gfx, frame, xo, yo };
    }

    SpriteInfo(std::initializer_list<SpritePartInfo> list) {
        partCount = 0;
        for (const auto& p : list) {
            if (partCount < 4) {
                parts[partCount++] = p;
            }
        }
        if (partCount > 0) {
            spriteIndex = parts[0].spriteIndex;
            paletteIndex = parts[0].paletteIndex;
            gfxIndex = parts[0].gfxIndex;
            frameIndex = parts[0].frameIndex;
        }
    }
};

class EntitySpriteProvider {
public:
    static EntitySpriteProvider& get();

    // Resolves vanilla (kind, id) to sprite, palette & gfx IDs (supports multi-part)
    SpriteInfo getVanillaSpriteInfo(u8 kind, u8 id) const;

    // Decodes sprite frame into a 64x64 RGBA pixel buffer
    bool decodeSpriteRgba(
        const RomDataReader& romReader,
        u16 spriteIndex,
        u16 paletteIndex,
        u16 gfxIndex,
        std::vector<u32>& outPixels,
        int& outWidth,
        int& outHeight,
        u8 frameIndex = 0,
        s8 xOffset = 0,
        s8 yOffset = 0,
        bool clearPixels = true
    );

    // Decodes composite sprite (all parts, e.g. body + head)
    bool decodeCompositeSpriteRgba(
        const RomDataReader& romReader,
        const SpriteInfo& info,
        std::vector<u32>& outPixels,
        int& outWidth,
        int& outHeight
    );

    // Convenience helper to get or create cached SDL_Texture for an entity
    SDL_Texture* getEntityTexture(
        SDL_Renderer* renderer,
        const RomDataReader& romReader,
        u8 kind,
        u8 id,
        u8 frameIndex = 0
    );

    void clearCache();

private:
    EntitySpriteProvider();
    ~EntitySpriteProvider();

    // Texture cache keyed by (kind << 16) | (id << 8) | frameIndex
    std::unordered_map<u32, SDL_Texture*> m_textureCache;
};

} // namespace tmc
