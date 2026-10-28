#include "editor_window.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace tmc {

EditorWindow::EditorWindow() = default;
EditorWindow::~EditorWindow() = default;

bool EditorWindow::init(SDL_Renderer* renderer) {
    m_renderer = renderer;
    if (!m_tileRenderer.init(renderer)) {
        return false;
    }
    m_entityStudio.init(renderer, &m_romReader);

    // Auto-probe common ROM paths
    const std::vector<std::string> romCandidates = {
        "baserom.gba",
        "baserom_eu.gba",
        "../../baserom.gba",
        "dist/USA/baserom.gba",
        "build/pc/baserom.gba"
    };

    for (const auto& path : romCandidates) {
        if (fs::exists(path) && m_romReader.loadRom(path)) {
            m_statusMessage = "Loaded ROM: " + path;
            m_entityStudio.setRomReader(&m_romReader);
            
            // Check if an autosaved project exists to restore previous session
            if (fs::exists("autosave.tmcproj") && m_projectManager.loadProject("autosave.tmcproj")) {
                m_selectedArea = m_projectManager.getActiveRoom().areaIndex;
                m_selectedRoom = m_projectManager.getActiveRoom().roomIndex;
                // Only load the tileset data so we don't wipe out the user's restored room edits!
                m_romReader.loadTilesetOnly(m_selectedArea, m_projectManager.getActiveRoom().header.tileSet_id, m_projectManager.getActiveTileset());
                m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
                m_statusMessage = "Restored previous session (Area " + std::to_string(m_selectedArea) +
                                  " Room " + std::to_string(m_selectedRoom) + ").";
            } else {
                // Load initial room (Area 0, Room 0 = Minish Woods Entrance)
                m_romReader.loadRoom(0, 0, m_projectManager.getActiveRoom(), m_projectManager.getActiveTileset());
                m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
            }
            break;
        }
    }

    return true;
}

void EditorWindow::updateAndRender() {
    renderMainMenuBar();

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float menuBarHeight = ImGui::GetFrameHeight();
    float topBarHeight = 44.0f;
    float contentY = menuBarHeight + topBarHeight;
    float contentH = displaySize.y - contentY;

    // 1. Top Quick Action Ribbon Toolbar
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, topBarHeight));
    ImGui::Begin("TopToolRibbon", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    // Tool buttons with active styling
    auto ToolButton = [this](const char* label, EditorTool tool, ImVec2 size = ImVec2(130, 28)) {
        bool isActive = (m_currentTool == tool);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
        }
        if (ImGui::Button(label, size)) {
            m_currentTool = tool;
        }
        if (isActive) {
            ImGui::PopStyleColor(2);
        }
    };

    // Combined Tilemap button
    bool isTilemapActive = (m_currentTool == EditorTool::PAINT_TILE_BOTTOM || m_currentTool == EditorTool::PAINT_TILE_TOP);
    if (isTilemapActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    }
    if (ImGui::Button("Tilemap", ImVec2(130, 28))) {
        if (!isTilemapActive) {
            m_currentTool = EditorTool::PAINT_TILE_BOTTOM;
        }
    }
    if (isTilemapActive) {
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine();

    ToolButton("Collision", EditorTool::PAINT_COLLISION);
    ImGui::SameLine();
    
    // Entity button isActive if current tool is either SELECT_ENTITY or PLACE_ENTITY
    bool isEntityActive = (m_currentTool == EditorTool::SELECT_ENTITY || m_currentTool == EditorTool::PLACE_ENTITY);
    if (isEntityActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    }
    if (ImGui::Button("Entity (NPC/Enemy)", ImVec2(130, 28))) {
        m_currentTool = EditorTool::SELECT_ENTITY;
    }
    if (isEntityActive) {
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine();

    ToolButton("Chest / Item", EditorTool::PLACE_TILE_ENTITY);
    ImGui::SameLine();
    ToolButton("Warp / Exit", EditorTool::PLACE_TRANSITION);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Zoom Controls
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(24, 26)) && m_renderOptions.zoom > 0.5f) {
        m_renderOptions.zoom = std::max(0.5f, m_renderOptions.zoom - 0.5f);
    }
    ImGui::SameLine();
    ImGui::Text("%.1fx", m_renderOptions.zoom);
    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(24, 26)) && m_renderOptions.zoom < 5.0f) {
        m_renderOptions.zoom = std::min(5.0f, m_renderOptions.zoom + 0.5f);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Entity Studio Quick Access Button
    if (m_showEntityStudio) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 0.9f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.35f, 0.70f, 0.9f));
    }
    if (ImGui::Button("Entity Studio (F6)", ImVec2(140, 28))) {
        m_showEntityStudio = !m_showEntityStudio;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(displaySize.x - 280);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.25f, 0.9f));
    if (ImGui::Button("Playtest in tmc_pc (F5)", ImVec2(260, 28))) {
        m_projectManager.exportMod(m_modExportName, "mods", &m_romReader);
        std::string exePath = fs::exists("build/pc/tmc_pc.exe") ? "build/pc/tmc_pc.exe" :
                              fs::exists("dist/USA/tmc_pc.exe") ? "dist/USA/tmc_pc.exe" :
                              fs::exists("build/pc/tmc_pc") ? "build/pc/tmc_pc" : "./dist/USA/tmc_pc";
        m_projectManager.launchGamePlaytest(m_modExportName, exePath);
        m_statusMessage = "Launching tmc_pc with mod: " + m_modExportName;
    }
    ImGui::PopStyleColor();

    ImGui::End();

    // 2. Left Panel: Room Selector & Project Manager (320px)
    float leftWidth = 320.0f;
    float rightWidth = 390.0f;
    float centerWidth = displaySize.x - leftWidth - rightWidth;

    ImGui::SetNextWindowPos(ImVec2(0, contentY));
    ImGui::SetNextWindowSize(ImVec2(leftWidth, contentH));
    renderToolbox();

    // 3. Center Panel: Map Canvas
    ImGui::SetNextWindowPos(ImVec2(leftWidth, contentY));
    ImGui::SetNextWindowSize(ImVec2(centerWidth, contentH));
    renderCanvas();

    // 4. Right Panel: Active Mode Inspector
    ImGui::SetNextWindowPos(ImVec2(leftWidth + centerWidth, contentY));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, contentH));
    ImGui::Begin("Inspector & Palette", nullptr, ImGuiWindowFlags_NoCollapse);

    switch (m_currentTool) {
        case EditorTool::PAINT_TILE_BOTTOM:
        case EditorTool::PAINT_TILE_TOP:
            renderTilesetPicker();
            break;
        case EditorTool::PAINT_COLLISION:
            renderCollisionPalette();
            break;
        case EditorTool::SELECT_ENTITY:
        case EditorTool::PLACE_ENTITY:
            renderEntityInspector();
            break;
        case EditorTool::PLACE_TILE_ENTITY:
            renderTileEntityInspector();
            break;
        case EditorTool::PLACE_TRANSITION:
            renderTransitionsInspector();
            break;
        default:
            renderTilesetPicker();
            break;
    }

    ImGui::End();

    // Render Entity Studio (Dedicated Custom Entity & Visual Behavior Designer Window)
    if (m_showEntityStudio) {
        m_entityStudio.render(&m_showEntityStudio);
    }
}

void EditorWindow::renderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Level Project", "Ctrl+N")) {
                m_projectManager.newProject(m_selectedArea, m_selectedRoom);
                m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
                m_statusMessage = "Created new level project.";
            }
            if (ImGui::MenuItem("Save Project (.tmcproj)", "Ctrl+S")) {
                m_projectManager.saveProject("custom_level.tmcproj");
                m_statusMessage = "Project saved to custom_level.tmcproj";
            }
            if (ImGui::MenuItem("Open Project (.tmcproj)", "Ctrl+O")) {
                if (m_projectManager.loadProject("custom_level.tmcproj")) {
                    m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
                    m_statusMessage = "Project loaded from custom_level.tmcproj";
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Mod (Project Picori)")) {
                m_projectManager.exportMod(m_modExportName, "mods", &m_romReader);
                m_statusMessage = "Mod exported to mods/" + m_modExportName;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_projectManager.canUndo())) {
                m_projectManager.undo();
                m_statusMessage = "Undo action performed.";
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_projectManager.canRedo())) {
                m_projectManager.redo();
                m_statusMessage = "Redo action performed.";
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Ground Layer (BG2)", nullptr, &m_renderOptions.showLayerBottom);
            ImGui::MenuItem("Show Overhead Layer (BG1)", nullptr, &m_renderOptions.showLayerTop);
            ImGui::MenuItem("Show Collision Overlay", nullptr, &m_renderOptions.showCollision);
            ImGui::MenuItem("Show 16x16 Grid", nullptr, &m_renderOptions.showGrid16);
            ImGui::MenuItem("Show Entities (Enemies/NPCs)", nullptr, &m_renderOptions.showEntities);
            ImGui::MenuItem("Show Tile Entities (Chests)", nullptr, &m_renderOptions.showTileEntities);
            ImGui::MenuItem("Show Transitions / Exits", nullptr, &m_renderOptions.showTransitions);
            ImGui::Separator();
            ImGui::SliderFloat("Zoom Level", &m_renderOptions.zoom, 0.5f, 5.0f, "%.1fx");
            ImGui::SliderFloat("Collision Alpha", &m_renderOptions.collisionOpacity, 0.1f, 1.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("TMC Entity Studio (Custom Entities & Behavior)", "F6", m_showEntityStudio)) {
                m_showEntityStudio = !m_showEntityStudio;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Playtest")) {
            if (ImGui::MenuItem("Launch in tmc_pc", "F5")) {
                m_projectManager.exportMod(m_modExportName, "mods", &m_romReader);
                std::string exePath = fs::exists("build/pc/tmc_pc.exe") ? "build/pc/tmc_pc.exe" :
                                      fs::exists("dist/USA/tmc_pc.exe") ? "dist/USA/tmc_pc.exe" :
                                      fs::exists("build/pc/tmc_pc") ? "build/pc/tmc_pc" : "./dist/USA/tmc_pc";
                m_projectManager.launchGamePlaytest(m_modExportName, exePath);
                m_statusMessage = "Launching tmc_pc with mod: " + m_modExportName;
            }
            ImGui::EndMenu();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 360);
        ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.20f, 1.0f), "Status: %s", m_statusMessage.c_str());

        ImGui::EndMainMenuBar();
    }
}

void EditorWindow::renderToolbox() {
    ImGui::Begin("Level & Project Manager", nullptr, ImGuiWindowFlags_NoCollapse);

    // 1. Room Selector
    renderRoomSelector();

    ImGui::Separator();
    ImGui::Text("Active Mod Name:");
    char modBuf[64];
    std::snprintf(modBuf, sizeof(modBuf), "%s", m_modExportName.c_str());
    if (ImGui::InputText("##ModName", modBuf, sizeof(modBuf))) {
        m_modExportName = modBuf;
    }

    if (ImGui::Button("Export Mod Files", ImVec2(-1, 28))) {
        m_projectManager.exportMod(m_modExportName, "mods", &m_romReader);
        m_statusMessage = "Exported mod to mods/" + m_modExportName;
    }

    ImGui::Separator();
    const auto& room = m_projectManager.getActiveRoom();
    ImGui::Text("Room Dimensions: %d x %d px (%d x %d tiles)",
                room.header.pixel_width, room.header.pixel_height,
                room.header.tileWidth(), room.header.tileHeight());
    ImGui::Text("Entities: %zu | Chests: %zu | Warps: %zu",
                room.entitiesMain.size(), room.tileEntities.size(), room.transitions.size());

    ImGui::End();
}

void EditorWindow::renderTilesetPicker() {
    bool isTop = (m_currentTool == EditorTool::PAINT_TILE_TOP);
    SDL_Texture* atlas = isTop ? m_tileRenderer.getAtlasTopTexture() : m_tileRenderer.getAtlasBottomTexture();
    if (!atlas) {
        ImGui::Text("Tileset texture not initialized.");
        return;
    }

    int cols = m_tileRenderer.getAtlasColumns();
    int rows = m_tileRenderer.getAtlasRows();
    int totalW = m_tileRenderer.getAtlasWidth();
    int totalH = m_tileRenderer.getAtlasHeight();
    const auto& tileset = m_projectManager.getActiveTileset();

    size_t totalMetas = isTop ? tileset.metaTilesTop.size() : tileset.metaTilesBottom.size();

    ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.2f, 1.0f), "Tileset: #%u (%s)", tileset.id, tileset.name.c_str());

    // Layer Selector Radio Buttons (Active Edit Layer)
    ImGui::Text("Active Layer:");
    if (ImGui::RadioButton("Ground (BG2)", !isTop)) {
        m_currentTool = EditorTool::PAINT_TILE_BOTTOM;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Overhead (BG1)", isTop)) {
        m_currentTool = EditorTool::PAINT_TILE_TOP;
    }

    // Layer Visibility Toggles
    ImGui::Text("Visibility:");
    ImGui::Checkbox("Show Ground (BG2)", &m_renderOptions.showLayerBottom);
    ImGui::SameLine();
    ImGui::Checkbox("Show Overhead (BG1)", &m_renderOptions.showLayerTop);

    ImGui::Separator();

    // Selected Metatile Swatch
    int sx = (m_selectedTileIndex % cols) * 16;
    int sy = (m_selectedTileIndex / cols) * 16;
    ImVec2 uv0(static_cast<float>(sx) / totalW, static_cast<float>(sy) / totalH);
    ImVec2 uv1(static_cast<float>(sx + 16) / totalW, static_cast<float>(sy + 16) / totalH);

    ImGui::Image(reinterpret_cast<ImTextureID>(atlas), ImVec2(48, 48), uv0, uv1);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Selected Metatile: #%u (0x%03X)", m_selectedTileIndex, m_selectedTileIndex);
    ImGui::Text("Layer: %s", isTop ? "BG1 (Overhead)" : "BG2 (Ground)");
    ImGui::EndGroup();

    ImGui::Separator();
    ImGui::Text("Metatiles Palette (%zu tiles):", totalMetas);

    ImGui::BeginChild("TilesetScrollArea", ImVec2(-1, -1), true);

    const int displayCols = 8;
    float tileSize = 36.0f;

    for (size_t i = 0; i < totalMetas && i < static_cast<size_t>(cols * rows); ++i) {
        int tsx = (static_cast<int>(i) % cols) * 16;
        int tsy = (static_cast<int>(i) / cols) * 16;

        ImVec2 tuv0(static_cast<float>(tsx) / totalW, static_cast<float>(tsy) / totalH);
        ImVec2 tuv1(static_cast<float>(tsx + 16) / totalW, static_cast<float>(tsy + 16) / totalH);

        ImGui::PushID(static_cast<int>(i));
        bool isSelected = (m_selectedTileIndex == static_cast<u16>(i));
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.84f, 0.0f, 1.0f)); // Gold border
        }

        if (ImGui::ImageButton("##tile_btn", reinterpret_cast<ImTextureID>(atlas), ImVec2(tileSize, tileSize), tuv0, tuv1)) {
            m_selectedTileIndex = static_cast<u16>(i);
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Metatile ID: #%zu (0x%03zX)", i, i);
        }

        ImGui::PopID();

        if ((i + 1) % displayCols != 0) {
            ImGui::SameLine();
        }
    }

    ImGui::EndChild();
}

void EditorWindow::renderRoomSelector() {
    const auto& areas = TmcDatabase::getAreas();
    std::string currentAreaName = TmcDatabase::getAreaName(m_selectedArea);

    if (ImGui::BeginCombo("Area", currentAreaName.c_str())) {
        for (const auto& area : areas) {
            bool isSelected = (m_selectedArea == area.id);
            char areaLabel[128];
            std::snprintf(areaLabel, sizeof(areaLabel), "[0x%02X] %s (%d room%s)",
                          area.id, area.name.c_str(), area.roomCount, area.roomCount == 1 ? "" : "s");
            if (ImGui::Selectable(areaLabel, isSelected)) {
                m_selectedArea = area.id;
                int maxRooms = m_romReader.isLoaded() ? m_romReader.getRoomCount(m_selectedArea) : TmcDatabase::getAreaRoomCount(m_selectedArea);
                if (maxRooms <= 0) maxRooms = 1;
                if (m_selectedRoom >= maxRooms) {
                    m_selectedRoom = 0;
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    int totalRooms = m_romReader.isLoaded() ? m_romReader.getRoomCount(m_selectedArea) : TmcDatabase::getAreaRoomCount(m_selectedArea);
    if (totalRooms <= 0) totalRooms = 1;

    if (m_selectedRoom >= totalRooms) {
        m_selectedRoom = 0;
    }

    // Room Selector constrained to existing rooms
    if (totalRooms == 1) {
        ImGui::Text("Room: 0 (Only 1 room in this area)");
        m_selectedRoom = 0;
    } else {
        char sliderFmt[64];
        std::snprintf(sliderFmt, sizeof(sliderFmt), "Room %%d of %d (0..%d)", totalRooms, totalRooms - 1);
        ImGui::SliderInt("Room Index", &m_selectedRoom, 0, totalRooms - 1, sliderFmt);
    }

    if (ImGui::Button("Load Room from ROM", ImVec2(-1, 28))) {
        if (m_romReader.isLoaded()) {
            m_projectManager.pushUndo("Load Room from ROM");
            if (m_romReader.loadRoom(m_selectedArea, m_selectedRoom,
                                     m_projectManager.getActiveRoom(),
                                     m_projectManager.getActiveTileset())) {
                m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
                m_statusMessage = "Loaded Area " + std::to_string(m_selectedArea) +
                                  " Room " + std::to_string(m_selectedRoom) + " (" + currentAreaName + ")";
            }
        } else {
            m_statusMessage = "Error: ROM not loaded.";
        }
    }
}

void EditorWindow::renderCollisionPalette() {
    const u8 collisionTypes[] = {
        COL_NONE, COL_SOLID, COL_WATER_DEEP, COL_WATER_SHALLOW,
        COL_HOLE, COL_LEDGE_DOWN, COL_LEDGE_UP, COL_LEDGE_LEFT,
        COL_LEDGE_RIGHT, COL_STAIRS, COL_SLOPE_NW, COL_SLOPE_NE,
        COL_SLOPE_SW, COL_SLOPE_SE, COL_MINISH_ONLY, COL_ICE
    };

    for (u8 col : collisionTypes) {
        ColorRGBA c = TileRenderer::getCollisionColor(col);
        ImVec4 colVec(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);

        ImGui::ColorButton("##col_btn", colVec, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
        ImGui::SameLine();

        bool isSelected = (m_selectedCollisionType == col);
        if (ImGui::Selectable(TileRenderer::getCollisionName(col), isSelected)) {
            m_selectedCollisionType = col;
            m_currentTool = EditorTool::PAINT_COLLISION;
        }
    }
}

void EditorWindow::renderCanvas() {
    ImGui::Begin("Map Viewport", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    m_renderOptions.selectedEntityList = m_selectedEntityList;
    m_renderOptions.selectedEntityIndex = m_selectedEntityIndex;
    m_renderOptions.selectedTransitionIndex = m_selectedTransitionIndex;
    if (m_currentTool == EditorTool::PLACE_TRANSITION) {
        m_renderOptions.showTransitions = true;
    }

    SDL_Texture* tex = m_tileRenderer.renderRoomToTexture(
        m_projectManager.getActiveRoom(),
        m_projectManager.getActiveTileset(),
        m_renderOptions
    );

    if (tex) {
        auto& room = m_projectManager.getActiveRoom();
        float viewW = static_cast<float>(room.header.pixel_width) * m_renderOptions.zoom;
        float viewH = static_cast<float>(room.header.pixel_height) * m_renderOptions.zoom;

        // Auto-center viewport if requested (e.g. double click on entity or transition in inspector)
        if (m_hasPendingFocus) {
            float targetScrollX = (m_pendingFocusPos.x * m_renderOptions.zoom) - (ImGui::GetWindowWidth() * 0.5f);
            float targetScrollY = (m_pendingFocusPos.y * m_renderOptions.zoom) - (ImGui::GetWindowHeight() * 0.5f);
            ImGui::SetScrollX(std::max(0.0f, targetScrollX));
            ImGui::SetScrollY(std::max(0.0f, targetScrollY));
            m_hasPendingFocus = false;
        }

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(static_cast<float>(room.header.pixel_width) / static_cast<float>(m_tileRenderer.getCanvasWidth()),
                   static_cast<float>(room.header.pixel_height) / static_cast<float>(m_tileRenderer.getCanvasHeight()));

        ImGui::Image(reinterpret_cast<ImTextureID>(tex), ImVec2(viewW, viewH), uv0, uv1);

        // Determine tool modes
        bool isEntityMode = (m_currentTool == EditorTool::PLACE_ENTITY) || (m_currentTool == EditorTool::SELECT_ENTITY);
        bool isTransitionMode = (m_currentTool == EditorTool::PLACE_TRANSITION);

        // Handle Canvas Clicking, Dragging & Painting
        if (ImGui::IsItemHovered()) {
            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - canvasPos.x) / m_renderOptions.zoom;
            float relY = (mousePos.y - canvasPos.y) / m_renderOptions.zoom;
            int tileX = static_cast<int>(relX) / 16;
            int tileY = static_cast<int>(relY) / 16;

            size_t tileIdx = tileY * MAX_MAP_WIDTH + tileX;
            u16 currentBottomMeta = (tileIdx < room.layerBottom.size()) ? room.layerBottom[tileIdx] : 0;
            u16 currentTopMeta = (tileIdx < room.layerTop.size()) ? room.layerTop[tileIdx] : 0;

            if (isTransitionMode) {
                // Find hovered transition
                int hoveredTransIdx = -1;
                static const int sShapeDims[4][2] = { { 6, 6 }, { 6, 14 }, { 14, 6 }, { 22, 6 } };

                for (int i = static_cast<int>(room.transitions.size()) - 1; i >= 0; --i) {
                    const auto& tr = room.transitions[i];
                    bool isBorder = (tr.warp_type == 0 || tr.warp_type == 2);
                    if (isBorder) {
                        int borderThickness = 16;
                        int roomW = room.header.pixel_width;
                        int roomH = room.header.pixel_height;
                        int midX = roomW / 2;
                        int midY = roomH / 2;

                        bool inN = (tr.shape & 0x01 && relX < midX && relY <= borderThickness) ||
                                   (tr.shape & 0x02 && relX >= midX && relY <= borderThickness);
                        bool inE = (tr.shape & 0x04 && relX >= roomW - borderThickness && relY < midY) ||
                                   (tr.shape & 0x08 && relX >= roomW - borderThickness && relY >= midY);
                        bool inS = (tr.shape & 0x10 && relX < midX && relY >= roomH - borderThickness) ||
                                   (tr.shape & 0x20 && relX >= midX && relY >= roomH - borderThickness);
                        bool inW = (tr.shape & 0x40 && relX <= borderThickness && relY < midY) ||
                                   (tr.shape & 0x80 && relX <= borderThickness && relY >= midY);

                        if (inN || inE || inS || inW) {
                            hoveredTransIdx = i;
                            break;
                        }
                    } else {
                        int shapeIdx = tr.shape & 3;
                        int halfW = sShapeDims[shapeIdx][0] + 4; // Slight padding for easier clicking
                        int halfH = sShapeDims[shapeIdx][1] + 4;
                        if (std::abs(static_cast<int>(tr.startX) - static_cast<int>(relX)) <= halfW &&
                            std::abs(static_cast<int>(tr.startY) - static_cast<int>(relY)) <= halfH) {
                            hoveredTransIdx = i;
                            break;
                        }
                    }
                }

                if (hoveredTransIdx >= 0) {
                    const auto& tr = room.transitions[hoveredTransIdx];
                    bool isBorder = (tr.warp_type == 0 || tr.warp_type == 2);
                    std::string warpKind = isBorder ? "Border Exit" : "Area Warp";
                    std::string targetAreaName = TmcDatabase::getAreaName(tr.destArea);
                    ImGui::SetTooltip("[%s #%d] -> %s (Area 0x%02X, Room %u)\nPos: (%d, %d) | Target Link Spawn: (%d, %d)\n%s",
                                      warpKind.c_str(), hoveredTransIdx, targetAreaName.c_str(),
                                      tr.destArea, tr.destRoom, tr.startX, tr.startY, tr.endX, tr.endY,
                                      (m_selectedTransitionIndex == hoveredTransIdx ? "(SELECTED - Drag to move trigger)" : "(Click to select)"));
                } else {
                    ImGui::SetTooltip("Warp / Exit Mode | Pos: (%d, %d)\n(Click a warp to select/move, or click 'Add New Warp' in Inspector)",
                                      static_cast<int>(relX), static_cast<int>(relY));
                }

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (hoveredTransIdx >= 0) {
                        bool wasAlreadySelected = (m_selectedTransitionIndex == hoveredTransIdx);
                        m_selectedTransitionIndex = hoveredTransIdx;
                        m_isDraggingTransition = wasAlreadySelected;
                    } else {
                        m_selectedTransitionIndex = -1;
                        m_isDraggingTransition = false;
                    }
                }

            } else if (isEntityMode) {
                // In Entity mode: show coordinates and hovered entity info across all 3 entity lists
                int hoveredListIdx = -1; // 0=Main, 1=Secondary, 2=Respawn
                int hoveredEntityIndex = -1;

                auto checkList = [&](const std::vector<EntityData>& list, int listId) {
                    for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i) {
                        const auto& ent = list[i];
                        if (std::abs(static_cast<int>(ent.xPos) - static_cast<int>(relX)) <= 10 &&
                            std::abs(static_cast<int>(ent.yPos) - static_cast<int>(relY)) <= 10) {
                            hoveredListIdx = listId;
                            hoveredEntityIndex = i;
                            return true;
                        }
                    }
                    return false;
                };

                // Check active list first, then remaining lists
                if (m_selectedEntityList == 0) {
                    if (!checkList(room.entitiesMain, 0)) {
                        if (!checkList(room.entitiesRespawn, 2)) {
                            checkList(room.entitiesSecondary, 1);
                        }
                    }
                } else if (m_selectedEntityList == 1) {
                    if (!checkList(room.entitiesSecondary, 1)) {
                        if (!checkList(room.entitiesRespawn, 2)) {
                            checkList(room.entitiesMain, 0);
                        }
                    }
                } else {
                    if (!checkList(room.entitiesRespawn, 2)) {
                        if (!checkList(room.entitiesMain, 0)) {
                            checkList(room.entitiesSecondary, 1);
                        }
                    }
                }

                if (hoveredEntityIndex >= 0) {
                    const EntityData* pEnt = nullptr;
                    const char* listTag = "Main";
                    if (hoveredListIdx == 0) { pEnt = &room.entitiesMain[hoveredEntityIndex]; listTag = "Main (Prop 0)"; }
                    else if (hoveredListIdx == 1) { pEnt = &room.entitiesSecondary[hoveredEntityIndex]; listTag = "Secondary (Prop 1)"; }
                    else if (hoveredListIdx == 2) { pEnt = &room.entitiesRespawn[hoveredEntityIndex]; listTag = "Enemy/Respawn (Prop 2)"; }

                    if (pEnt) {
                        bool isAlreadySelected = (m_selectedEntityList == hoveredListIdx && m_selectedEntityIndex == hoveredEntityIndex);
                        if (isAlreadySelected) {
                            ImGui::SetTooltip("[%s #%d] %s (SELECTED)\nPos: (%d, %d)\n(Drag to move)",
                                              listTag, hoveredEntityIndex, TmcDatabase::getEntityName(pEnt->kind, pEnt->id).c_str(),
                                              pEnt->xPos, pEnt->yPos);
                        } else {
                            ImGui::SetTooltip("[%s #%d] %s\nPos: (%d, %d)\n(Click to select)",
                                              listTag, hoveredEntityIndex, TmcDatabase::getEntityName(pEnt->kind, pEnt->id).c_str(),
                                              pEnt->xPos, pEnt->yPos);
                        }
                    }
                } else {
                    ImGui::SetTooltip("Entity Mode | Pos: (%d, %d) | Tile: [%d, %d]\n(Tile painting disabled)",
                                      static_cast<int>(relX), static_cast<int>(relY), tileX, tileY);
                }

                // Left click press on entity -> select or start dragging if already selected
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (hoveredEntityIndex >= 0) {
                        bool wasAlreadySelected = (m_selectedEntityList == hoveredListIdx && m_selectedEntityIndex == hoveredEntityIndex);
                        m_selectedEntityList = hoveredListIdx;
                        m_selectedEntityIndex = hoveredEntityIndex;

                        if (wasAlreadySelected) {
                            // If it was already selected, allow dragging on subsequent mouse press/drag
                            m_isDraggingEntity = true;
                        } else {
                            // First click only selects the entity, does NOT move it
                            m_isDraggingEntity = false;
                        }
                        
                        const EntityData& ent = (m_selectedEntityList == 0) ? room.entitiesMain[hoveredEntityIndex] :
                                                (m_selectedEntityList == 1) ? room.entitiesSecondary[hoveredEntityIndex] :
                                                                              room.entitiesRespawn[hoveredEntityIndex];
                        m_statusMessage = "Selected entity #" + std::to_string(hoveredEntityIndex) + ": " +
                                          TmcDatabase::getEntityName(ent.kind, ent.id) + (wasAlreadySelected ? " (Dragging)" : "");
                    } else if (m_currentTool == EditorTool::PLACE_ENTITY) {
                        // Place a new entity in currently active entity list
                        EntityData ent;
                        ent.kind = static_cast<u8>(m_selectedEntityKind);
                        ent.id = static_cast<u8>(m_selectedEntityId);
                        ent.xPos = static_cast<u16>(std::clamp(static_cast<int>(relX), 0, static_cast<int>(room.header.pixel_width)));
                        ent.yPos = static_cast<u16>(std::clamp(static_cast<int>(relY), 0, static_cast<int>(room.header.pixel_height)));
                        
                        std::vector<EntityData>& targetList = (m_selectedEntityList == 0) ? room.entitiesMain :
                                                              (m_selectedEntityList == 1) ? room.entitiesSecondary :
                                                                                            room.entitiesRespawn;
                        targetList.push_back(ent);
                        m_selectedEntityIndex = static_cast<int>(targetList.size()) - 1;
                        m_isDraggingEntity = false;
                        m_projectManager.markModified();
                        m_statusMessage = "Placed new entity: " + TmcDatabase::getEntityName(ent.kind, ent.id);
                    } else {
                        // Clicked empty space in entity mode -> deselect
                        m_selectedEntityIndex = -1;
                        m_isDraggingEntity = false;
                    }
                }
            } else {
                // Non-Entity Mode: regular tile/collision painting
                ImGui::SetTooltip("Pos: (%d, %d) | Tile: [%d, %d] | Bottom: #%u | Top: #%u",
                                  static_cast<int>(relX), static_cast<int>(relY),
                                  tileX, tileY, currentBottomMeta, currentTopMeta);

                // Left Click -> Paint Tiles / Collision
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    if (m_currentTool == EditorTool::PAINT_COLLISION) {
                        m_projectManager.setCollision(tileX, tileY, m_selectedCollisionType);
                    } else if (m_currentTool == EditorTool::PAINT_TILE_BOTTOM) {
                        m_projectManager.setTile(0, tileX, tileY, m_selectedTileIndex);
                    } else if (m_currentTool == EditorTool::PAINT_TILE_TOP) {
                        m_projectManager.setTile(1, tileX, tileY, m_selectedTileIndex);
                    } else if (m_currentTool == EditorTool::PLACE_TILE_ENTITY && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        TileEntity te;
                        te.type = static_cast<u8>(m_selectedTileEntityType);
                        te.tilePos = static_cast<u16>(tileY * MAX_MAP_WIDTH + tileX);
                        room.tileEntities.push_back(te);
                        m_projectManager.markModified();
                    }
                }

                // Right Click -> Eyedropper / Pick Tile from Map
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    if (m_currentTool == EditorTool::PAINT_TILE_TOP && currentTopMeta != 0) {
                        m_selectedTileIndex = currentTopMeta;
                        m_statusMessage = "Picked Top Tile #" + std::to_string(currentTopMeta);
                    } else {
                        m_selectedTileIndex = currentBottomMeta;
                        m_statusMessage = "Picked Bottom Tile #" + std::to_string(currentBottomMeta);
                    }
                }
            }
        }

        // Live dragging update for Entity
        if (m_isDraggingEntity) {
            std::vector<EntityData>& targetList = (m_selectedEntityList == 0) ? room.entitiesMain :
                                                  (m_selectedEntityList == 1) ? room.entitiesSecondary :
                                                                                room.entitiesRespawn;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(targetList.size())) {
                ImVec2 mousePos = ImGui::GetMousePos();
                float relX = (mousePos.x - canvasPos.x) / m_renderOptions.zoom;
                float relY = (mousePos.y - canvasPos.y) / m_renderOptions.zoom;
                auto& ent = targetList[m_selectedEntityIndex];
                ent.xPos = static_cast<u16>(std::clamp(static_cast<int>(relX), 0, static_cast<int>(room.header.pixel_width)));
                ent.yPos = static_cast<u16>(std::clamp(static_cast<int>(relY), 0, static_cast<int>(room.header.pixel_height)));
                m_projectManager.markModified();
            } else {
                m_isDraggingEntity = false;
            }
        }

        // Live dragging update for Area Transition Trigger
        if (m_isDraggingTransition) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                m_selectedTransitionIndex >= 0 && m_selectedTransitionIndex < static_cast<int>(room.transitions.size())) {
                ImVec2 mousePos = ImGui::GetMousePos();
                float relX = (mousePos.x - canvasPos.x) / m_renderOptions.zoom;
                float relY = (mousePos.y - canvasPos.y) / m_renderOptions.zoom;
                auto& tr = room.transitions[m_selectedTransitionIndex];
                if (tr.warp_type == 1 || tr.warp_type == 3) { // Only area transitions have a local (X, Y) trigger position
                    tr.startX = static_cast<u16>(std::clamp(static_cast<int>(relX), 0, static_cast<int>(room.header.pixel_width)));
                    tr.startY = static_cast<u16>(std::clamp(static_cast<int>(relY), 0, static_cast<int>(room.header.pixel_height)));
                    m_projectManager.markModified();
                }
            } else {
                m_isDraggingTransition = false;
            }
        }
    }

    ImGui::End();
}

void EditorWindow::renderEntityInspector() {
    auto& room = m_projectManager.getActiveRoom();

    // Entity List Group Selector
    const char* listNames[] = {
        "Main / Static (Prop 0)",
        "Secondary / Cutscene (Prop 1)",
        "Enemies & Respawn (Prop 2)"
    };
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Entity Property List:");
    int prevList = m_selectedEntityList;
    if (ImGui::Combo("##EntityListGroup", &m_selectedEntityList, listNames, IM_ARRAYSIZE(listNames))) {
        if (prevList != m_selectedEntityList) {
            m_selectedEntityIndex = -1;
        }
    }

    std::vector<EntityData>& currentList = (m_selectedEntityList == 0) ? room.entitiesMain :
                                           (m_selectedEntityList == 1) ? room.entitiesSecondary :
                                                                         room.entitiesRespawn;

    ImGui::TextDisabled("Count: %zu | Select below or click/drag in viewport.", currentList.size());

    // Filter/Search box for room entities
    static char filterBuf[64] = "";
    ImGui::InputTextWithHint("##FilterEntities", "Search entity...", filterBuf, sizeof(filterBuf));

    if (ImGui::BeginListBox("##EntitiesList", ImVec2(-1, 160))) {
        for (size_t i = 0; i < currentList.size(); ++i) {
            const auto& ent = currentList[i];
            std::string name = TmcDatabase::getEntityName(ent.kind, ent.id);
            if (filterBuf[0] != '\0') {
                std::string lowerName = name;
                std::string lowerFilter = filterBuf;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                if (lowerName.find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }

            std::string label = "[" + std::to_string(i) + "] " + name +
                                " (" + std::to_string(ent.xPos) + ", " + std::to_string(ent.yPos) + ")";
            bool isSelected = (m_selectedEntityIndex == static_cast<int>(i));
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_selectedEntityIndex = static_cast<int>(i);
                m_selectedEntityKind = ent.kind;
                m_selectedEntityId = ent.id;
            }

            // Double click entity in list to center viewport on it
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_selectedEntityIndex = static_cast<int>(i);
                m_selectedEntityKind = ent.kind;
                m_selectedEntityId = ent.id;
                m_pendingFocusPos = ImVec2(static_cast<float>(ent.xPos), static_cast<float>(ent.yPos));
                m_hasPendingFocus = true;
                m_statusMessage = "Centered view on " + name + " (" + std::to_string(ent.xPos) + ", " + std::to_string(ent.yPos) + ")";
            }
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();

    // Spawn New Entity Section
    if (ImGui::CollapsingHeader("Spawn / Place New Entity", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* kindNames[] = {
            "Player (1)", "Enemy (3)", "Projectile (4)", "Object (6)", "NPC (7)", "Player Item (8)", "Manager (9)"
        };
        const u8 kindValues[] = { 1, 3, 4, 6, 7, 8, 9 };

        int currentKindIdx = 1; // Default to Enemy (3)
        for (int i = 0; i < 7; ++i) {
            if (kindValues[i] == m_selectedEntityKind) {
                currentKindIdx = i;
                break;
            }
        }

        if (ImGui::Combo("Spawn Kind", &currentKindIdx, kindNames, IM_ARRAYSIZE(kindNames))) {
            m_selectedEntityKind = kindValues[currentKindIdx];
            m_selectedEntityId = 0;
        }

        // Entity ID selector with full names for the selected Kind
        std::vector<const EntityMeta*> matchingEntities;
        for (const auto& meta : TmcDatabase::getEntityRegistry()) {
            if (meta.kind == m_selectedEntityKind) {
                matchingEntities.push_back(&meta);
            }
        }

        std::string currentSelectedName = TmcDatabase::getEntityName(m_selectedEntityKind, m_selectedEntityId);
        if (ImGui::BeginCombo("Spawn Entity", currentSelectedName.c_str())) {
            for (const auto* meta : matchingEntities) {
                bool isSel = (meta->id == m_selectedEntityId);
                char label[128];
                std::snprintf(label, sizeof(label), "0x%02X - %s", meta->id, meta->name.c_str());
                if (ImGui::Selectable(label, isSel)) {
                    m_selectedEntityId = meta->id;
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Activate Entity Placement Tool", ImVec2(-1, 26))) {
            m_currentTool = EditorTool::PLACE_ENTITY;
            m_statusMessage = "Click on Map Viewport to place " + currentSelectedName;
        }
    }

    // Selected Entity Properties
    if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(currentList.size())) {
        auto& ent = currentList[m_selectedEntityIndex];
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Selected Entity #%d: %s",
                           m_selectedEntityIndex, TmcDatabase::getEntityName(ent.kind, ent.id).c_str());

        // Kind Selector
        const char* kindNames[] = {
            "Player (1)", "Enemy (3)", "Projectile (4)", "Object (6)", "NPC (7)", "Player Item (8)", "Manager (9)"
        };
        const u8 kindValues[] = { 1, 3, 4, 6, 7, 8, 9 };

        int currentKindIdx = 0;
        for (int i = 0; i < 7; ++i) {
            if (kindValues[i] == ent.kind) {
                currentKindIdx = i;
                break;
            }
        }

        if (ImGui::Combo("Kind##Edit", &currentKindIdx, kindNames, IM_ARRAYSIZE(kindNames))) {
            ent.kind = kindValues[currentKindIdx];
            m_projectManager.markModified();
        }

        // Entity ID Selector with Registry Names
        std::vector<const EntityMeta*> matchingEntities;
        for (const auto& meta : TmcDatabase::getEntityRegistry()) {
            if (meta.kind == ent.kind) {
                matchingEntities.push_back(&meta);
            }
        }

        std::string currentEntName = TmcDatabase::getEntityName(ent.kind, ent.id);
        if (ImGui::BeginCombo("Entity ID##EditCombo", currentEntName.c_str())) {
            for (const auto* meta : matchingEntities) {
                bool isSel = (meta->id == ent.id);
                char label[128];
                std::snprintf(label, sizeof(label), "0x%02X - %s", meta->id, meta->name.c_str());
                if (ImGui::Selectable(label, isSel)) {
                    ent.id = meta->id;
                    m_projectManager.markModified();
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        int x = ent.xPos;
        int y = ent.yPos;
        if (ImGui::DragInt("Position X", &x, 1, 0, room.header.pixel_width)) { ent.xPos = static_cast<u16>(x); m_projectManager.markModified(); }
        if (ImGui::DragInt("Position Y", &y, 1, 0, room.header.pixel_height)) { ent.yPos = static_cast<u16>(y); m_projectManager.markModified(); }

        int type = ent.type;
        int type2 = ent.type2;
        int flags = ent.flags;
        if (ImGui::DragInt("Type / Variation", &type, 1, 0, 255)) { ent.type = static_cast<u8>(type); m_projectManager.markModified(); }
        if (ImGui::DragInt("Type 2 / Param", &type2, 1, 0, 255)) { ent.type2 = static_cast<u8>(type2); m_projectManager.markModified(); }
        if (ImGui::DragInt("Flags / Properties", &flags, 1, 0, 255)) { ent.flags = static_cast<u8>(flags); m_projectManager.markModified(); }

        ImGui::Spacing();
        if (ImGui::Button("Duplicate Entity", ImVec2(140, 24))) {
            EntityData copy = ent;
            copy.xPos = std::min<u16>(copy.xPos + 16, room.header.pixel_width);
            copy.yPos = std::min<u16>(copy.yPos + 16, room.header.pixel_height);
            currentList.push_back(copy);
            m_selectedEntityIndex = static_cast<int>(currentList.size()) - 1;
            m_projectManager.markModified();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Entity", ImVec2(140, 24))) {
            currentList.erase(currentList.begin() + m_selectedEntityIndex);
            m_selectedEntityIndex = -1;
            m_projectManager.markModified();
        }
    }
}

void EditorWindow::renderTileEntityInspector() {
    auto& room = m_projectManager.getActiveRoom();
    ImGui::Text("Tile Entities (%zu):", room.tileEntities.size());

    if (ImGui::BeginListBox("##TileEntsList", ImVec2(-1, 130))) {
        for (size_t i = 0; i < room.tileEntities.size(); ++i) {
            const auto& te = room.tileEntities[i];
            std::string typeName = (te.type == TE_SMALL_CHEST) ? "Small Chest" :
                                   (te.type == TE_BIG_CHEST) ? "Big Chest" :
                                   (te.type == TE_BOMBABLE_WALL) ? "Bombable Wall" :
                                   (te.type == TE_DARKNESS) ? "Darkness / Torch" : "Tile Entity";

            std::string label = "[" + std::to_string(i) + "] " + typeName + " (TilePos: " + std::to_string(te.tilePos) + ")";
            bool isSelected = (m_selectedTileEntityIndex == static_cast<int>(i));
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_selectedTileEntityIndex = static_cast<int>(i);
            }
        }
        ImGui::EndListBox();
    }

    if (m_selectedTileEntityIndex >= 0 && m_selectedTileEntityIndex < static_cast<int>(room.tileEntities.size())) {
        auto& te = room.tileEntities[m_selectedTileEntityIndex];
        ImGui::Separator();
        int tpos = te.tilePos;
        if (ImGui::DragInt("Tile Position Index", &tpos, 1, 0, 4095)) {
            te.tilePos = static_cast<u16>(tpos);
            m_projectManager.markModified();
        }
        int lflag = te.localFlag;
        if (ImGui::InputInt("Local Flag (Persistence)", &lflag)) {
            te.localFlag = static_cast<u8>(lflag);
            m_projectManager.markModified();
        }
        if (ImGui::Button("Delete Tile Entity", ImVec2(-1, 24))) {
            room.tileEntities.erase(room.tileEntities.begin() + m_selectedTileEntityIndex);
            m_selectedTileEntityIndex = -1;
            m_projectManager.markModified();
        }
    }
}

void EditorWindow::renderTransitionsInspector() {
    auto& room = m_projectManager.getActiveRoom();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "Room Warps & Exits (%zu):", room.transitions.size());
    ImGui::TextDisabled("Triggers transporting Link to another room or outdoor area.");

    if (ImGui::BeginListBox("##TransitionsList", ImVec2(-1, 150))) {
        for (size_t i = 0; i < room.transitions.size(); ++i) {
            const auto& tr = room.transitions[i];
            bool isBorder = (tr.warp_type == 0 || tr.warp_type == 2);
            std::string destName = TmcDatabase::getAreaName(tr.destArea);

            std::string label = "[" + std::to_string(i) + "] " +
                                (isBorder ? "Border -> " : "Warp -> ") +
                                destName + " (R" + std::to_string(tr.destRoom) + ")";

            bool isSelected = (m_selectedTransitionIndex == static_cast<int>(i));
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_selectedTransitionIndex = static_cast<int>(i);
            }

            // Double click to focus viewport
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_selectedTransitionIndex = static_cast<int>(i);
                m_hasPendingFocus = true;
                if (isBorder) {
                    m_pendingFocusPos = ImVec2(room.header.pixel_width * 0.5f, room.header.pixel_height * 0.5f);
                } else {
                    m_pendingFocusPos = ImVec2(tr.startX, tr.startY);
                }
            }
        }
        ImGui::EndListBox();
    }

    if (m_selectedTransitionIndex >= 0 && m_selectedTransitionIndex < static_cast<int>(room.transitions.size())) {
        auto& tr = room.transitions[m_selectedTransitionIndex];
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Transition #%d Properties:", m_selectedTransitionIndex);

        // 1. Warp Type Selector
        const char* warpTypes[] = {
            "0: WARP_TYPE_BORDER (Room Edge)",
            "1: WARP_TYPE_AREA (Door / Stairs / Portal)",
            "2: WARP_TYPE_BORDER2 (Secondary Border)",
            "3: WARP_TYPE_AREA2 (Secondary Area)"
        };
        int wType = tr.warp_type;
        if (wType > 3) wType = 1;
        if (ImGui::Combo("Warp Type", &wType, warpTypes, IM_ARRAYSIZE(warpTypes))) {
            tr.warp_type = static_cast<u16>(wType);
            m_projectManager.markModified();
        }

        bool isBorder = (tr.warp_type == 0 || tr.warp_type == 2);

        // 2. Shape Selector
        if (isBorder) {
            ImGui::Text("Border Direction / Quadrant:");
            struct BorderOption { u8 mask; const char* name; };
            static const BorderOption sBorderOpts[] = {
                { 0x01, "North-West (Top Left)" },
                { 0x02, "North-East (Top Right)" },
                { 0x03, "North (Whole Top Edge)" },
                { 0x04, "East-North (Right Top)" },
                { 0x08, "East-South (Right Bottom)" },
                { 0x0C, "East (Whole Right Edge)" },
                { 0x10, "South-West (Bottom Left)" },
                { 0x20, "South-East (Bottom Right)" },
                { 0x30, "South (Whole Bottom Edge)" },
                { 0x40, "West-North (Left Top)" },
                { 0x80, "West-South (Left Bottom)" },
                { 0xC0, "West (Whole Left Edge)" }
            };

            int currentBorderIdx = 0;
            for (int b = 0; b < 12; ++b) {
                if (tr.shape == sBorderOpts[b].mask) {
                    currentBorderIdx = b;
                    break;
                }
            }

            if (ImGui::BeginCombo("Border Edge", sBorderOpts[currentBorderIdx].name)) {
                for (int b = 0; b < 12; ++b) {
                    bool isSel = (currentBorderIdx == b);
                    if (ImGui::Selectable(sBorderOpts[b].name, isSel)) {
                        tr.shape = sBorderOpts[b].mask;
                        m_projectManager.markModified();
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            const char* areaShapes[] = {
                "0: 12x12 px (Small Entrance / Hole / Tile)",
                "1: 12x28 px (Vertical Door / Pathway)",
                "2: 28x12 px (Horizontal Wide Door / Cave Entrance)",
                "3: 44x12 px (Extra Wide Door / Passage)"
            };
            int shapeIdx = tr.shape & 3;
            if (ImGui::Combo("Trigger Box Shape", &shapeIdx, areaShapes, IM_ARRAYSIZE(areaShapes))) {
                tr.shape = static_cast<u8>(shapeIdx);
                m_projectManager.markModified();
            }

            int sx = tr.startX, sy = tr.startY;
            if (ImGui::DragInt("Trigger Pos X", &sx, 1, 0, room.header.pixel_width)) {
                tr.startX = static_cast<u16>(sx);
                m_projectManager.markModified();
            }
            if (ImGui::DragInt("Trigger Pos Y", &sy, 1, 0, room.header.pixel_height)) {
                tr.startY = static_cast<u16>(sy);
                m_projectManager.markModified();
            }
        }

        ImGui::Separator();
        ImGui::Text("Destination Link Spawn:");

        // 3. Destination Area Combo
        const auto& areas = TmcDatabase::getAreas();
        int curAreaListIdx = 0;
        for (size_t a = 0; a < areas.size(); ++a) {
            if (areas[a].id == tr.destArea) {
                curAreaListIdx = static_cast<int>(a);
                break;
            }
        }
        std::string currentAreaPreview = TmcDatabase::getAreaName(tr.destArea) + " (0x" +
            ([&]() { char b[8]; std::snprintf(b, sizeof(b), "%02X", tr.destArea); return std::string(b); })() + ")";

        if (ImGui::BeginCombo("Destination Area", currentAreaPreview.c_str())) {
            for (size_t a = 0; a < areas.size(); ++a) {
                bool isSel = (curAreaListIdx == static_cast<int>(a));
                char itemBuf[96];
                std::snprintf(itemBuf, sizeof(itemBuf), "0x%02X: %s (%d rooms)", areas[a].id, areas[a].name.c_str(), areas[a].roomCount);
                if (ImGui::Selectable(itemBuf, isSel)) {
                    tr.destArea = areas[a].id;
                    if (tr.destRoom >= areas[a].roomCount) {
                        tr.destRoom = 0;
                    }
                    m_projectManager.markModified();
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // 4. Destination Room Selector (bounded by actual room count)
        int maxRooms = TmcDatabase::getAreaRoomCount(tr.destArea);
        if (maxRooms <= 0) maxRooms = 1;
        int dRoom = tr.destRoom;
        if (ImGui::SliderInt("Destination Room", &dRoom, 0, maxRooms - 1, "Room %d")) {
            tr.destRoom = static_cast<u8>(dRoom);
            m_projectManager.markModified();
        }

        // Button to directly load and open the destination room
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.72f, 0.9f));
        if (ImGui::Button("Open Destination Room in Editor", ImVec2(-1, 26))) {
            if (m_romReader.isLoaded()) {
                m_selectedArea = tr.destArea;
                m_selectedRoom = tr.destRoom;
                m_projectManager.pushUndo("Navigate to Warp Destination Room");
                if (m_romReader.loadRoom(m_selectedArea, m_selectedRoom,
                                         m_projectManager.getActiveRoom(),
                                         m_projectManager.getActiveTileset())) {
                    m_tileRenderer.updateTilesetGfx(m_projectManager.getActiveTileset());
                    m_selectedTransitionIndex = -1;
                    m_selectedEntityIndex = -1;
                    m_selectedTileEntityIndex = -1;
                    m_statusMessage = "Opened destination: " + TmcDatabase::getAreaName(m_selectedArea) +
                                      " (Room " + std::to_string(m_selectedRoom) + ")";
                }
            } else {
                m_statusMessage = "Error: ROM not loaded.";
            }
        }
        ImGui::PopStyleColor();

        // 5. Target Spawn Coordinates in Destination Room (endX, endY)
        bool keepCoordX = (tr.endX > 0x3FF);
        bool keepCoordY = (tr.endY > 0x3FF);

        int ex = keepCoordX ? 0 : tr.endX;
        int ey = keepCoordY ? 0 : tr.endY;

        if (ImGui::Checkbox("Preserve Link's X position across transition", &keepCoordX)) {
            tr.endX = keepCoordX ? 0x0FFF : 0x0080;
            m_projectManager.markModified();
        }
        if (!keepCoordX) {
            if (ImGui::DragInt("Spawn X in Target Room", &ex, 1, 0, 2048)) {
                tr.endX = static_cast<u16>(ex);
                m_projectManager.markModified();
            }
        }

        if (ImGui::Checkbox("Preserve Link's Y position across transition", &keepCoordY)) {
            tr.endY = keepCoordY ? 0x0FFF : 0x0080;
            m_projectManager.markModified();
        }
        if (!keepCoordY) {
            if (ImGui::DragInt("Spawn Y in Target Room", &ey, 1, 0, 2048)) {
                tr.endY = static_cast<u16>(ey);
                m_projectManager.markModified();
            }
        }

        // 6. Spawn Animation / Transition Type
        const char* transTypes[] = {
            "0: NORMAL (Walk In)",
            "1: INSTANT_MINISH",
            "2: DROP_IN (Fall from ceiling)",
            "3: INSTANT (Direct cut)",
            "4: STEP_FORWARD",
            "5: UNK_5",
            "6: DROP_IN_MINISH",
            "7: STAIR_EXIT",
            "8: UNK_8",
            "9: UNK_9",
            "10: HOP_IN_FORWARD",
            "11: HOP_IN",
            "12: FLY_IN"
        };
        int tType = tr.transition_type;
        if (tType >= IM_ARRAYSIZE(transTypes)) tType = 0;
        if (ImGui::Combo("Transition Animation", &tType, transTypes, IM_ARRAYSIZE(transTypes))) {
            tr.transition_type = static_cast<u8>(tType);
            m_projectManager.markModified();
        }

        // 7. Facing Direction
        const char* facingDirs[] = {
            "0: Up (North)",
            "1: Right (East)",
            "2: Down (South)",
            "3: Left (West)",
            "4: Preserve Direction"
        };
        int dir = tr.facing_direction;
        if (dir >= IM_ARRAYSIZE(facingDirs)) dir = 4;
        if (ImGui::Combo("Spawn Facing Direction", &dir, facingDirs, IM_ARRAYSIZE(facingDirs))) {
            tr.facing_direction = static_cast<u8>(dir);
            m_projectManager.markModified();
        }

        // 8. Player Layer
        int layer = tr.playerLayer;
        if (ImGui::RadioButton("Layer 1 (Overhead / Normal)", layer == 1)) {
            tr.playerLayer = 1;
            m_projectManager.markModified();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Layer 2 (Lower / Under)", layer == 2)) {
            tr.playerLayer = 2;
            m_projectManager.markModified();
        }

        // Actions: Focus Viewport, Delete
        ImGui::Separator();
        if (ImGui::Button("Center View on Warp (or Double-Click List)", ImVec2(-1, 24))) {
            m_hasPendingFocus = true;
            if (isBorder) {
                m_pendingFocusPos = ImVec2(room.header.pixel_width * 0.5f, room.header.pixel_height * 0.5f);
            } else {
                m_pendingFocusPos = ImVec2(tr.startX, tr.startY);
            }
        }

        if (ImGui::Button("Delete This Warp / Exit", ImVec2(-1, 24))) {
            room.transitions.erase(room.transitions.begin() + m_selectedTransitionIndex);
            m_selectedTransitionIndex = -1;
            m_projectManager.markModified();
        }
    } else {
        ImGui::Separator();
        if (ImGui::Button("Add New Area Warp (Door / Cave / Hole)", ImVec2(-1, 28))) {
            Transition tr;
            tr.warp_type = 1; // WARP_TYPE_AREA
            tr.destArea = static_cast<u8>(m_selectedArea);
            tr.destRoom = 0;
            tr.startX = static_cast<u16>(room.header.pixel_width / 2);
            tr.startY = static_cast<u16>(room.header.pixel_height / 2);
            tr.endX = 0x0080;
            tr.endY = 0x0080;
            tr.shape = 0; // 12x12
            tr.playerLayer = 1;
            tr.transition_type = 0;
            tr.facing_direction = 2; // Face down
            room.transitions.push_back(tr);
            m_selectedTransitionIndex = static_cast<int>(room.transitions.size()) - 1;
            m_projectManager.markModified();
            m_statusMessage = "Added new Area Warp.";
        }

        if (ImGui::Button("Add New Border Exit (Room Edge)", ImVec2(-1, 28))) {
            Transition tr;
            tr.warp_type = 0; // WARP_TYPE_BORDER
            tr.destArea = static_cast<u8>(m_selectedArea);
            tr.destRoom = 0;
            tr.startX = 0;
            tr.startY = 0;
            tr.endX = 0x0FFF; // Keep X
            tr.endY = 0x0080;
            tr.shape = 0x30; // South border
            tr.playerLayer = 1;
            tr.transition_type = 0;
            tr.facing_direction = 2; // Face down
            room.transitions.push_back(tr);
            m_selectedTransitionIndex = static_cast<int>(room.transitions.size()) - 1;
            m_projectManager.markModified();
            m_statusMessage = "Added new Border Exit.";
        }
    }
}

void EditorWindow::renderRoomHeaderInspector() {
    auto& header = m_projectManager.getActiveRoom().header;

    int w = header.pixel_width;
    int h = header.pixel_height;
    if (ImGui::DragInt("Pixel Width", &w, 16, 256, 1024)) { header.pixel_width = static_cast<u16>(w); m_projectManager.markModified(); }
    if (ImGui::DragInt("Pixel Height", &h, 16, 256, 1024)) { header.pixel_height = static_cast<u16>(h); m_projectManager.markModified(); }

    int mx = header.map_x;
    int my = header.map_y;
    if (ImGui::DragInt("Area Map X", &mx, 16, 0, 2048)) { header.map_x = static_cast<u16>(mx); m_projectManager.markModified(); }
    if (ImGui::DragInt("Area Map Y", &my, 16, 0, 2048)) { header.map_y = static_cast<u16>(my); m_projectManager.markModified(); }

    int ts = header.tileSet_id;
    if (ImGui::InputInt("Tileset ID", &ts)) { header.tileSet_id = static_cast<u16>(ts); m_projectManager.markModified(); }
}

} // namespace tmc
