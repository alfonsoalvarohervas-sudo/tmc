#pragma once

#include "core/tmc_types.hpp"
#include "core/tmc_database.hpp"
#include "core/rom_data_reader.hpp"
#include "core/project_manager.hpp"
#include "renderer/tile_renderer.hpp"
#include "ui/entity_studio_window.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>

namespace tmc {

enum class EditorTool {
    SELECT_ENTITY,
    PAINT_TILE_BOTTOM,
    PAINT_TILE_TOP,
    PAINT_COLLISION,
    PLACE_ENTITY,
    PLACE_TILE_ENTITY,
    PLACE_TRANSITION,
};

class EditorWindow {
public:
    EditorWindow();
    ~EditorWindow();

    bool init(SDL_Renderer* renderer);
    void updateAndRender();

private:
    SDL_Renderer*  m_renderer = nullptr;
    RomDataReader  m_romReader;
    ProjectManager m_projectManager;
    TileRenderer   m_tileRenderer;
    RenderOptions  m_renderOptions;

    EditorTool     m_currentTool = EditorTool::PAINT_TILE_BOTTOM;
    u8             m_selectedCollisionType = COL_SOLID;
    u16            m_selectedTileIndex = 0;
    int            m_selectedEntityKind = 3; // Enemy
    int            m_selectedEntityId = 1;   // Octorok
    int            m_selectedTileEntityType = 2; // Small Chest
    int            m_selectedChestItem = 0x1C;   // Green Rupee

    int            m_selectedArea = 0;
    int            m_selectedRoom = 0;
    int            m_selectedEntityList = 0; // 0=Main (Prop 0), 1=Secondary/Story (Prop 1), 2=Enemies/Respawn (Prop 2)
    int            m_selectedEntityIndex = -1;
    int            m_selectedTileEntityIndex = -1;
    int            m_selectedTransitionIndex = -1;

    std::string    m_statusMessage = "Ready. ROM loaded.";
    std::string    m_modExportName = "custom_map";

    // Dragging & interaction state
    bool           m_isDraggingEntity = false;
    bool           m_isDraggingTransition = false;
    bool           m_hasPendingFocus = false;
    ImVec2         m_pendingFocusPos = ImVec2(0, 0);

    // Entity Studio Window (Custom Entity & Visual Behavior Designer)
    bool               m_showEntityStudio = false;
    EntityStudioWindow m_entityStudio;


    // Sub-window renderers
    void renderMainMenuBar();
    void renderToolbox();
    void renderCanvas();
    void renderTilesetPicker();
    void renderRoomSelector();
    void renderCollisionPalette();
    void renderEntityInspector();
    void renderTileEntityInspector();
    void renderTransitionsInspector();
    void renderRoomHeaderInspector();
};

} // namespace tmc
