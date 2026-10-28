#pragma once

#include "core/tmc_types.hpp"
#include "core/tmc_database.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>

namespace tmc {

// Block Types for Visual Entity Behavior Scripting
enum class ScriptBlockType {
    EVENT_ON_SPAWN,
    EVENT_ON_TALK,
    EVENT_ON_DAMAGE,
    EVENT_ON_DEATH,
    CONDITION_HAS_ITEM,
    CONDITION_CHECK_FLAG,
    CONDITION_PLAYER_NEAR,
    ACTION_SHOW_MESSAGE,
    ACTION_GIVE_ITEM,
    ACTION_SET_FLAG,
    ACTION_MOVE_DIR,
    ACTION_PLAY_SOUND,
    ACTION_CHANGE_ANIM,
};

struct ScriptBlock {
    ScriptBlockType type;
    std::string     label;
    std::string     argString;
    int             argInt = 0;
    int             argInt2 = 0;
};

struct CustomEntityDefinition {
    std::string id = "custom_actor_1";
    std::string name = "Custom Actor";
    u8          baseKind = 3; // Enemy by default
    u8          baseId = 1;   // Chuchu
    u8          defaultType = 0;
    u8          defaultCollisionLayer = 1;

    // Visual attributes
    int         health = 4;
    int         damage = 2;
    float       moveSpeed = 1.0f;
    std::string spriteSheetName = "default";
    int         previewPalette = 0;

    // Visual Behavior Script (Block sequence)
    std::vector<ScriptBlock> scriptBlocks;
};

// Information recorded when user flags an entity as having an error/issue
struct DebugMarkedEntity {
    u8 kind = 0;
    u8 id = 0;
    std::string name;
    std::string category;
    std::string notes;
    std::string timestamp;
};

class RomDataReader;

class EntityStudioWindow {
public:
    EntityStudioWindow();
    ~EntityStudioWindow();

    bool init(SDL_Renderer* renderer, const RomDataReader* romReader = nullptr);
    void setRomReader(const RomDataReader* romReader) { m_romReader = romReader; updatePreviewTexture(); }
    void render(bool* pOpen);

    // Get list of custom user entities for level placement
    const std::vector<CustomEntityDefinition>& getCustomEntities() const { return m_customEntities; }

    // Debug Marked Entities (Issue tracker)
    void loadMarkedEntities();
    void saveMarkedEntities();
    bool isEntityMarked(u8 kind, u8 id) const;
    void markEntity(u8 kind, u8 id, const std::string& name, const std::string& category, const std::string& notes);
    void unmarkEntity(u8 kind, u8 id);
    std::string getEntityMarkNote(u8 kind, u8 id) const;
    void setEntityMarkNote(u8 kind, u8 id, const std::string& notes);
    const std::unordered_map<u32, DebugMarkedEntity>& getMarkedEntities() const { return m_markedEntities; }

private:
    SDL_Renderer* m_renderer = nullptr;
    const RomDataReader* m_romReader = nullptr;
    bool m_ownsPreviewTexture = true;

    // Active project custom entities
    std::vector<CustomEntityDefinition> m_customEntities;
    int  m_selectedEntityIndex = 0;

    // Vanilla Entity Browser State
    int  m_selectedSidebarTab = 0; // 0 = Custom Mod Entities, 1 = Vanilla Entities Registry
    int  m_selectedVanillaKind = 3; // Enemy by default
    int  m_selectedVanillaId = 0;
    char m_vanillaFilterBuf[64] = "";
    int  m_selectedCategoryFilter = -1; // -1 = All, 0 = Enemy, 1 = Boss, 2 = NPC, 3 = Object, 4 = Projectile, 5 = Manager
    bool m_showOnlyMarked = false;

    // Debug tracking registry: key = (kind << 8) | id
    std::unordered_map<u32, DebugMarkedEntity> m_markedEntities;

    // Viewport preview animation state
    SDL_Texture* m_previewTexture = nullptr;
    float m_previewZoom = 3.0f;
    int   m_currentAnimFrame = 0;
    float m_animTimer = 0.0f;
    bool  m_isPlayingAnim = true;
    float m_animSpeed = 1.0f;
    int   m_selectedAnimIndex = 0;

    // Sub-panel renderers
    void renderEntitySidebar();
    void renderPreviewViewport();
    void renderPropertiesInspector();
    void renderVisualScriptEditor();

    // Helper functions
    void updatePreviewTexture();
    void createNewEntity();
};

} // namespace tmc
