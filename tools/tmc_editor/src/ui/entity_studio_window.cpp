#include "ui/entity_studio_window.hpp"
#include "core/entity_sprite_provider.hpp"
#include "core/rom_data_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace tmc {

EntityStudioWindow::EntityStudioWindow() {
    // Populate with an initial template custom entity
    CustomEntityDefinition sample;
    sample.id = "green_chuchu_elite";
    sample.name = "Elite Spiny Chuchu";
    sample.baseKind = 3; // Enemy
    sample.baseId = 0x0B; // Spiny Chuchu
    sample.health = 8;
    sample.damage = 4;
    sample.moveSpeed = 1.8f;

    // Default visual script blocks
    sample.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_SPAWN, "On Spawn", "", 0, 0 });
    sample.scriptBlocks.push_back({ ScriptBlockType::ACTION_CHANGE_ANIM, "Play Animation", "Idle South", 0, 0 });
    sample.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_TALK, "On Player Interact", "", 0, 0 });
    sample.scriptBlocks.push_back({ ScriptBlockType::ACTION_SHOW_MESSAGE, "Show Dialogue Message", "Bzzt! Keep away!", 101, 0 });
    sample.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_DEATH, "On Defeated", "", 0, 0 });
    sample.scriptBlocks.push_back({ ScriptBlockType::ACTION_GIVE_ITEM, "Drop Item Reward", "Blue Rupee (5)", 0x1D, 1 });

    m_customEntities.push_back(sample);
}

EntityStudioWindow::~EntityStudioWindow() {
    if (m_previewTexture && m_ownsPreviewTexture) {
        SDL_DestroyTexture(m_previewTexture);
        m_previewTexture = nullptr;
    }
}

bool EntityStudioWindow::init(SDL_Renderer* renderer, const RomDataReader* romReader) {
    m_renderer = renderer;
    m_romReader = romReader;
    loadMarkedEntities();
    updatePreviewTexture();
    return true;
}

void EntityStudioWindow::updatePreviewTexture() {
    if (!m_renderer) return;

    if (m_previewTexture && m_ownsPreviewTexture) {
        SDL_DestroyTexture(m_previewTexture);
        m_previewTexture = nullptr;
    }

    u8 activeKind = 3;
    u8 activeId = 0;
    if (m_selectedSidebarTab == 0) {
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(m_customEntities.size())) {
            activeKind = m_customEntities[m_selectedEntityIndex].baseKind;
            activeId = m_customEntities[m_selectedEntityIndex].baseId;
        }
    } else {
        activeKind = static_cast<u8>(m_selectedVanillaKind);
        activeId = static_cast<u8>(m_selectedVanillaId);
    }

    // 1. Try decoding authentic sprite graphic directly from ROM
    if (m_romReader && m_romReader->isLoaded()) {
        SDL_Texture* spriteTex = EntitySpriteProvider::get().getEntityTexture(
            m_renderer,
            *m_romReader,
            activeKind,
            activeId,
            static_cast<u8>(m_currentAnimFrame)
        );

        if (spriteTex) {
            m_previewTexture = spriteTex;
            m_ownsPreviewTexture = false; // Texture managed by EntitySpriteProvider cache
            return;
        }
    }

    // 2. Fallback procedural representation if ROM is not currently mounted
    m_ownsPreviewTexture = true;
    const int texW = 64;
    const int texH = 64;
    std::vector<u32> pixels(texW * texH, 0);

    // Render checkered transparent background
    for (int y = 0; y < texH; ++y) {
        for (int x = 0; x < texW; ++x) {
            bool check = ((x / 8) + (y / 8)) % 2 == 0;
            pixels[y * texW + x] = check ? 0xFF282828 : 0xFF353535;
        }
    }

    // Render sprite silhouette based on baseKind/baseId
    u32 color = 0xFF50D050; // Green Chuchu by default
    if (activeKind == 3) color = 0xFF4040E0; // Enemy (Reddish/Orange)
    else if (activeKind == 7) color = 0xFFE08030; // NPC (Blue)
    else if (activeKind == 6) color = 0xFF20C0D0; // Object (Yellow)
    else if (activeKind == 9) color = 0xFFC040C0; // Manager (Purple)
    else if (activeKind == 1) color = 0xFF50E050; // Player (Green)

    int cx = 32;
    int cy = 34 + static_cast<int>(std::sin(m_animTimer * 4.0f) * 2.0f); // subtle idle bobbing
    int r = 14;

    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r * r) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < texW && py >= 0 && py < texH) {
                    if (dx * dx + dy * dy >= (r - 2) * (r - 2)) {
                        pixels[py * texW + px] = 0xFF101010; // Dark outline
                    } else if (dx == -4 && dy == -4) {
                        pixels[py * texW + px] = 0xFFFFFFFF; // Highlight
                    } else {
                        pixels[py * texW + px] = color;
                    }
                }
            }
        }
    }

    // Eyes
    pixels[(cy - 2) * texW + (cx - 4)] = 0xFF000000;
    pixels[(cy - 2) * texW + (cx + 4)] = 0xFF000000;
    pixels[(cy - 3) * texW + (cx - 4)] = 0xFFFFFFFF;
    pixels[(cy - 3) * texW + (cx + 4)] = 0xFFFFFFFF;

    m_previewTexture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STATIC,
        texW, texH
    );

    if (m_previewTexture) {
        SDL_SetTextureScaleMode(m_previewTexture, SDL_SCALEMODE_NEAREST);
        SDL_UpdateTexture(m_previewTexture, nullptr, pixels.data(), texW * 4);
    }
}

void EntityStudioWindow::createNewEntity() {
    CustomEntityDefinition newEnt;
    newEnt.id = "custom_actor_" + std::to_string(m_customEntities.size() + 1);
    newEnt.name = "New Custom Entity " + std::to_string(m_customEntities.size() + 1);
    newEnt.baseKind = 3; // Enemy
    newEnt.baseId = 0x01; // Chuchu

    newEnt.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_SPAWN, "On Spawn", "", 0, 0 });
    newEnt.scriptBlocks.push_back({ ScriptBlockType::ACTION_CHANGE_ANIM, "Play Animation", "Idle South", 0, 0 });

    m_customEntities.push_back(newEnt);
    m_selectedEntityIndex = static_cast<int>(m_customEntities.size()) - 1;
    updatePreviewTexture();
}

void EntityStudioWindow::render(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    // Tick animation timer
    if (m_isPlayingAnim) {
        m_animTimer += 0.016f * m_animSpeed;
        if (m_animTimer > 6.28318f) m_animTimer -= 6.28318f;
        updatePreviewTexture();
    }

    ImGui::SetNextWindowSize(ImVec2(1050, 680), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("TMC Entity Studio - Custom Entity & Behavior Designer", pOpen)) {
        ImGui::End();
        return;
    }

    // Top action bar
    if (ImGui::Button("+ New Custom Entity", ImVec2(160, 26))) {
        createNewEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate Entity", ImVec2(130, 26))) {
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(m_customEntities.size())) {
            CustomEntityDefinition copy = m_customEntities[m_selectedEntityIndex];
            copy.id += "_copy";
            copy.name += " (Copy)";
            m_customEntities.push_back(copy);
            m_selectedEntityIndex = static_cast<int>(m_customEntities.size()) - 1;
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.2f, 0.2f, 0.9f));
    if (ImGui::Button("Delete Entity", ImVec2(110, 26))) {
        if (m_customEntities.size() > 1 && m_selectedEntityIndex >= 0) {
            m_customEntities.erase(m_customEntities.begin() + m_selectedEntityIndex);
            m_selectedEntityIndex = std::clamp(m_selectedEntityIndex, 0, static_cast<int>(m_customEntities.size()) - 1);
            updatePreviewTexture();
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 0.9f));
    if (ImGui::Button("Save to Project / Mod", ImVec2(200, 26))) {
        // Will persist definition
    }
    ImGui::PopStyleColor();

    ImGui::Separator();

    // 3-Column Layout: Sidebar (List) | Viewport & Script Editor | Properties Inspector
    float sideW = 240.0f;
    float inspW = 310.0f;
    float centerW = ImGui::GetContentRegionAvail().x - sideW - inspW - 16.0f;
    float fullH = ImGui::GetContentRegionAvail().y;

    // 1. Left: Entity Selection List
    ImGui::BeginChild("EntitySidebar", ImVec2(sideW, fullH), true);
    renderEntitySidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    // 2. Center: Visual Preview on top, Blockly Script Editor on bottom
    ImGui::BeginChild("EntityCenter", ImVec2(centerW, fullH), false);
    renderPreviewViewport();
    ImGui::Spacing();
    renderVisualScriptEditor();
    ImGui::EndChild();

    ImGui::SameLine();

    // 3. Right: Properties & Stats Inspector
    ImGui::BeginChild("EntityInspector", ImVec2(inspW, fullH), true);
    renderPropertiesInspector();
    ImGui::EndChild();

    ImGui::End();
}


bool EntityStudioWindow::isEntityMarked(u8 kind, u8 id) const {
    u32 key = (static_cast<u32>(kind) << 8) | id;
    return m_markedEntities.find(key) != m_markedEntities.end();
}

void EntityStudioWindow::markEntity(u8 kind, u8 id, const std::string& name, const std::string& category, const std::string& notes) {
    u32 key = (static_cast<u32>(kind) << 8) | id;
    DebugMarkedEntity ent;
    ent.kind = kind;
    ent.id = id;
    ent.name = name;
    ent.category = category;
    ent.notes = notes;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    ent.timestamp = ss.str();

    m_markedEntities[key] = ent;
    saveMarkedEntities();
}

void EntityStudioWindow::unmarkEntity(u8 kind, u8 id) {
    u32 key = (static_cast<u32>(kind) << 8) | id;
    m_markedEntities.erase(key);
    saveMarkedEntities();
}

std::string EntityStudioWindow::getEntityMarkNote(u8 kind, u8 id) const {
    u32 key = (static_cast<u32>(kind) << 8) | id;
    auto it = m_markedEntities.find(key);
    if (it != m_markedEntities.end()) {
        return it->second.notes;
    }
    return "";
}

void EntityStudioWindow::setEntityMarkNote(u8 kind, u8 id, const std::string& notes) {
    u32 key = (static_cast<u32>(kind) << 8) | id;
    auto it = m_markedEntities.find(key);
    if (it != m_markedEntities.end()) {
        it->second.notes = notes;
        saveMarkedEntities();
    }
}

void EntityStudioWindow::saveMarkedEntities() {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& [k, info] : m_markedEntities) {
        char hexBuf[16];
        std::snprintf(hexBuf, sizeof(hexBuf), "0x%02X", info.id);
        j.push_back({
            { "kind", info.kind },
            { "id", info.id },
            { "hexId", hexBuf },
            { "name", info.name },
            { "category", info.category },
            { "notes", info.notes },
            { "timestamp", info.timestamp }
        });
    }

    const char* paths[] = {
        "debug_marked_entities.json",
        "dist/USA/debug_marked_entities.json",
        "../../debug_marked_entities.json"
    };

    for (const char* p : paths) {
        std::ofstream out(p);
        if (out.is_open()) {
            out << j.dump(2);
            out.close();
        }
    }
}

void EntityStudioWindow::loadMarkedEntities() {
    m_markedEntities.clear();
    const char* paths[] = {
        "debug_marked_entities.json",
        "dist/USA/debug_marked_entities.json",
        "../../debug_marked_entities.json"
    };

    for (const char* p : paths) {
        std::ifstream in(p);
        if (in.is_open()) {
            try {
                nlohmann::json j;
                in >> j;
                if (j.is_array()) {
                    for (const auto& item : j) {
                        DebugMarkedEntity ent;
                        ent.kind = item.value("kind", (u8)0);
                        ent.id = item.value("id", (u8)0);
                        ent.name = item.value("name", "");
                        ent.category = item.value("category", "");
                        ent.notes = item.value("notes", "");
                        ent.timestamp = item.value("timestamp", "");
                        u32 key = (static_cast<u32>(ent.kind) << 8) | ent.id;
                        m_markedEntities[key] = ent;
                    }
                    break;
                }
            } catch (...) {
            }
        }
    }
}

void EntityStudioWindow::renderEntitySidebar() {
    // Mode Switcher Tabs
    if (ImGui::Button("Custom (Mod)", ImVec2(105, 24))) {
        m_selectedSidebarTab = 0;
        updatePreviewTexture();
    }
    ImGui::SameLine();
    if (ImGui::Button("Vanilla (Game)", ImVec2(105, 24))) {
        m_selectedSidebarTab = 1;
        updatePreviewTexture();
    }
    ImGui::Separator();

    if (m_selectedSidebarTab == 0) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Mod Entities (%zu):", m_customEntities.size());
        ImGui::Separator();

        for (size_t i = 0; i < m_customEntities.size(); ++i) {
            const auto& ent = m_customEntities[i];
            bool isSel = (m_selectedEntityIndex == static_cast<int>(i));

            std::string label = "[" + std::to_string(i + 1) + "] " + ent.name;
            if (ImGui::Selectable(label.c_str(), isSel)) {
                m_selectedEntityIndex = static_cast<int>(i);
                updatePreviewTexture();
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Vanilla Game Registry:");

        // Search Filter input
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##VanillaFilter", "Search name, ID...", m_vanillaFilterBuf, sizeof(m_vanillaFilterBuf));

        // Category Filter Buttons
        struct CatBtn { const char* label; int filterId; };
        static const CatBtn s_filters[] = {
            { "All", -1 },
            { "Enemies", 0 },
            { "Bosses", 1 },
            { "NPCs", 2 },
            { "Objects", 3 },
            { "Projectiles", 4 },
            { "Managers", 5 }
        };

        for (int i = 0; i < 7; ++i) {
            bool isAct = (m_selectedCategoryFilter == s_filters[i].filterId && !m_showOnlyMarked);
            if (isAct) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.85f, 1.0f));
            }
            if (ImGui::SmallButton(s_filters[i].label)) {
                m_selectedCategoryFilter = s_filters[i].filterId;
                m_showOnlyMarked = false;
            }
            if (isAct) ImGui::PopStyleColor();
            if (i < 6) ImGui::SameLine();
        }

        // Marked Filter Button
        char markedBtnLabel[64];
        std::snprintf(markedBtnLabel, sizeof(markedBtnLabel), "[!] Marked (%zu)", m_markedEntities.size());
        if (m_showOnlyMarked) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        }
        if (ImGui::SmallButton(markedBtnLabel)) {
            m_showOnlyMarked = !m_showOnlyMarked;
        }
        if (m_showOnlyMarked) ImGui::PopStyleColor();

        ImGui::Separator();

        const auto& allEntities = TmcDatabase::getEntityRegistry();

        // Organize entities into categories
        struct CategoryGroup {
            std::string name;
            int filterId;
            std::vector<const tmc::EntityMeta*> items;
        };

        CategoryGroup groups[] = {
            { "Enemies", 0, {} },
            { "Bosses", 1, {} },
            { "NPCs & Characters", 2, {} },
            { "Objects & Props", 3, {} },
            { "Projectiles", 4, {} },
            { "Managers & Logic", 5, {} }
        };

        for (const auto& meta : allEntities) {
            // Text search filter
            if (m_vanillaFilterBuf[0] != '\0') {
                std::string lowerName = meta.name;
                std::string lowerFilter = m_vanillaFilterBuf;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                char hexIdBuf[16];
                std::snprintf(hexIdBuf, sizeof(hexIdBuf), "%02x", meta.id);
                if (lowerName.find(lowerFilter) == std::string::npos && std::string(hexIdBuf).find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }

            // Marked filter
            if (m_showOnlyMarked && !isEntityMarked(meta.kind, meta.id)) {
                continue;
            }

            // Assign to group
            if (meta.kind == 3) {
                if (meta.category == "Boss") groups[1].items.push_back(&meta);
                else groups[0].items.push_back(&meta);
            } else if (meta.kind == 7) {
                groups[2].items.push_back(&meta);
            } else if (meta.kind == 6) {
                groups[3].items.push_back(&meta);
            } else if (meta.kind == 4) {
                groups[4].items.push_back(&meta);
            } else if (meta.kind == 9) {
                groups[5].items.push_back(&meta);
            }
        }

        ImGui::BeginChild("VanillaListArea", ImVec2(-1, -1), true);

        if (m_showOnlyMarked) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Showing %zu Flagged Entities:", m_markedEntities.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Show All")) {
                m_showOnlyMarked = false;
            }
            ImGui::Separator();
        }

        for (auto& grp : groups) {
            if (m_selectedCategoryFilter != -1 && m_selectedCategoryFilter != grp.filterId && !m_showOnlyMarked) {
                continue;
            }
            if (grp.items.empty()) continue;

            char headerLabel[128];
            std::snprintf(headerLabel, sizeof(headerLabel), "%s (%zu)###cat_%s", grp.name.c_str(), grp.items.size(), grp.name.c_str());

            if (ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto* meta : grp.items) {
                    bool marked = isEntityMarked(meta->kind, meta->id);
                    bool isSel = (m_selectedVanillaKind == meta->kind && m_selectedVanillaId == meta->id);

                    char itemLabel[160];
                    if (marked) {
                        std::snprintf(itemLabel, sizeof(itemLabel), "[!] [0x%02X] %s", meta->id, meta->name.c_str());
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.45f, 1.0f));
                    } else {
                        std::snprintf(itemLabel, sizeof(itemLabel), "    [0x%02X] %s", meta->id, meta->name.c_str());
                    }

                    if (ImGui::Selectable(itemLabel, isSel)) {
                        m_selectedVanillaKind = meta->kind;
                        m_selectedVanillaId = meta->id;
                        updatePreviewTexture();
                    }

                    if (marked) {
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::IsItemHovered()) {
                        if (marked) {
                            std::string note = getEntityMarkNote(meta->kind, meta->id);
                            ImGui::SetTooltip("Kind: %u | ID: 0x%02X\nCategory: %s\n%s\n\n[!] MARKED ISSUE:\n%s",
                                              meta->kind, meta->id, meta->category.c_str(), meta->description.c_str(),
                                              note.empty() ? "(No notes specified)" : note.c_str());
                        } else {
                            ImGui::SetTooltip("Kind: %u | ID: 0x%02X\nCategory: %s\n%s",
                                              meta->kind, meta->id, meta->category.c_str(), meta->description.c_str());
                        }
                    }
                }
            }
        }

        ImGui::EndChild();
    }
}

void EntityStudioWindow::renderPreviewViewport() {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Sprite & Animation Viewport:");

    ImGui::BeginChild("PreviewBox", ImVec2(-1, 200), true);
    if (m_previewTexture) {
        float size = 64.0f * m_previewZoom;
        float availX = ImGui::GetContentRegionAvail().x;
        float offX = std::max(0.0f, (availX - size) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);
        ImGui::Image(reinterpret_cast<ImTextureID>(m_previewTexture), ImVec2(size, size));
    }

    // Animation Controls Toolbar
    ImGui::Separator();
    if (ImGui::Button(m_isPlayingAnim ? "Pause" : "Play", ImVec2(70, 22))) {
        m_isPlayingAnim = !m_isPlayingAnim;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::SliderFloat("Speed", &m_animSpeed, 0.2f, 3.0f, "%.1fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::SliderFloat("Zoom", &m_previewZoom, 1.0f, 5.0f, "%.1fx");

    ImGui::SameLine();
    const char* animList[] = { "Idle South", "Walk South", "Walk North", "Walk East", "Attack", "Damage / Hit" };
    ImGui::SetNextItemWidth(140);
    ImGui::Combo("Anim", &m_selectedAnimIndex, animList, IM_ARRAYSIZE(animList));

    ImGui::EndChild();
}

void EntityStudioWindow::renderVisualScriptEditor() {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Visual Behavior Script (Blockly Graph):");
    ImGui::TextDisabled("Define events, dialogue, movement and triggers using connectable blocks.");

    if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(m_customEntities.size())) {
        return;
    }
    auto& ent = m_customEntities[m_selectedEntityIndex];

    // Add Block Action Bar
    if (ImGui::Button("+ Event Block", ImVec2(110, 24))) {
        ImGui::OpenPopup("AddEventPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Action Block", ImVec2(110, 24))) {
        ImGui::OpenPopup("AddActionPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Condition Block", ImVec2(125, 24))) {
        ImGui::OpenPopup("AddConditionPopup");
    }

    // Popups for adding blocks
    if (ImGui::BeginPopup("AddEventPopup")) {
        if (ImGui::MenuItem("On Spawn (Room Entered)")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_SPAWN, "On Spawn", "", 0, 0 });
        }
        if (ImGui::MenuItem("On Player Interact (Talk / A Button)")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_TALK, "On Player Interact", "", 0, 0 });
        }
        if (ImGui::MenuItem("On Damaged (Hit by Sword / Item)")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_DAMAGE, "On Damaged", "", 0, 0 });
        }
        if (ImGui::MenuItem("On Defeated / Death")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_DEATH, "On Defeated", "", 0, 0 });
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AddActionPopup")) {
        if (ImGui::MenuItem("Show Dialogue Message")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::ACTION_SHOW_MESSAGE, "Show Dialogue Message", "Hello traveler!", 100, 0 });
        }
        if (ImGui::MenuItem("Give Item Reward")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::ACTION_GIVE_ITEM, "Give Item Reward", "Piece of Heart", 0x22, 1 });
        }
        if (ImGui::MenuItem("Set Game Flag")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::ACTION_SET_FLAG, "Set Game Flag", "Quest Complete", 0x0100, 1 });
        }
        if (ImGui::MenuItem("Play Sound Effect")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::ACTION_PLAY_SOUND, "Play Sound Effect", "SFX_ITEM_GET", 0x68, 0 });
        }
        if (ImGui::MenuItem("Change Animation")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::ACTION_CHANGE_ANIM, "Change Animation", "Walk North", 1, 0 });
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AddConditionPopup")) {
        if (ImGui::MenuItem("Check If Player Has Item")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::CONDITION_HAS_ITEM, "If Player Has Item", "Jabber Nut", 0x29, 0 });
        }
        if (ImGui::MenuItem("Check If Flag Is Active")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::CONDITION_CHECK_FLAG, "If Flag Active", "Boss Defeated", 0x0050, 1 });
        }
        if (ImGui::MenuItem("Check Player Proximity")) {
            ent.scriptBlocks.push_back({ ScriptBlockType::CONDITION_PLAYER_NEAR, "If Player Distance < X", "Within 48px", 48, 0 });
        }
        ImGui::EndPopup();
    }

    // Scrollable Block Sequence Area
    ImGui::BeginChild("ScriptBlockCanvas", ImVec2(-1, -1), true);

    for (size_t b = 0; b < ent.scriptBlocks.size(); ++b) {
        auto& block = ent.scriptBlocks[b];
        ImGui::PushID(static_cast<int>(b));

        // Colors according to block category: Event (Orange), Condition (Yellow), Action (Blue)
        ImVec4 blockColor = ImVec4(0.25f, 0.45f, 0.75f, 0.9f); // Action Blue
        if (block.type == ScriptBlockType::EVENT_ON_SPAWN ||
            block.type == ScriptBlockType::EVENT_ON_TALK ||
            block.type == ScriptBlockType::EVENT_ON_DAMAGE ||
            block.type == ScriptBlockType::EVENT_ON_DEATH) {
            blockColor = ImVec4(0.85f, 0.45f, 0.15f, 0.9f); // Event Orange
        } else if (block.type == ScriptBlockType::CONDITION_HAS_ITEM ||
                   block.type == ScriptBlockType::CONDITION_CHECK_FLAG ||
                   block.type == ScriptBlockType::CONDITION_PLAYER_NEAR) {
            blockColor = ImVec4(0.80f, 0.70f, 0.15f, 0.9f); // Condition Amber
        }

        ImGui::PushStyleColor(ImGuiCol_Header, blockColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, blockColor);

        std::string headerTitle = "[" + std::to_string(b + 1) + "] " + block.label;
        bool open = ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::PopStyleColor(2);

        if (open) {
            ImGui::Indent(16.0f);
            if (block.type == ScriptBlockType::ACTION_SHOW_MESSAGE) {
                char msgBuf[128];
                std::snprintf(msgBuf, sizeof(msgBuf), "%s", block.argString.c_str());
                if (ImGui::InputText("Message Text", msgBuf, sizeof(msgBuf))) {
                    block.argString = msgBuf;
                }
            } else if (block.type == ScriptBlockType::ACTION_GIVE_ITEM) {
                ImGui::InputInt("Item ID", &block.argInt);
            } else if (block.type == ScriptBlockType::ACTION_SET_FLAG || block.type == ScriptBlockType::CONDITION_CHECK_FLAG) {
                ImGui::InputInt("Flag ID (Hex)", &block.argInt, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
            } else if (block.type == ScriptBlockType::CONDITION_PLAYER_NEAR) {
                ImGui::SliderInt("Trigger Radius (px)", &block.argInt, 16, 256);
            }

            // Move Up / Down / Remove buttons
            if (ImGui::SmallButton("Up") && b > 0) {
                std::swap(ent.scriptBlocks[b], ent.scriptBlocks[b - 1]);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Down") && b + 1 < ent.scriptBlocks.size()) {
                std::swap(ent.scriptBlocks[b], ent.scriptBlocks[b + 1]);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            if (ImGui::SmallButton("Remove Block")) {
                ent.scriptBlocks.erase(ent.scriptBlocks.begin() + b);
                ImGui::PopStyleColor();
                ImGui::Unindent(16.0f);
                ImGui::PopID();
                break;
            }
            ImGui::PopStyleColor();

            ImGui::Unindent(16.0f);
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    ImGui::EndChild();
}

void EntityStudioWindow::renderPropertiesInspector() {
    if (m_selectedSidebarTab == 1) {
        // Inspecting a Vanilla Game Entity
        std::string vName = TmcDatabase::getEntityName(m_selectedVanillaKind, m_selectedVanillaId);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Vanilla Entity Reference:");
        ImGui::Separator();

        ImGui::Text("Name: %s", vName.c_str());
        ImGui::Text("Kind: %u (%s)", m_selectedVanillaKind,
                    m_selectedVanillaKind == 3 ? "Enemy" :
                    m_selectedVanillaKind == 7 ? "NPC" :
                    m_selectedVanillaKind == 6 ? "Object" :
                    m_selectedVanillaKind == 9 ? "Manager" : "Player");
        ImGui::Text("ID: 0x%02X (%u)", m_selectedVanillaId, m_selectedVanillaId);

        SpriteInfo sprInfo = EntitySpriteProvider::get().getVanillaSpriteInfo(m_selectedVanillaKind, m_selectedVanillaId);
        if (sprInfo.gfxIndex != 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Sprite #%u | Palette #%u | GFX #%u",
                               sprInfo.spriteIndex, sprInfo.paletteIndex, sprInfo.gfxIndex);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Sprite #%u | Palette #%u",
                               sprInfo.spriteIndex, sprInfo.paletteIndex);
        }

        // Find metadata description if available
        std::string metaCategory = "Entity";
        std::string metaDesc = "";
        for (const auto& meta : TmcDatabase::getEntityRegistry()) {
            if (meta.kind == m_selectedVanillaKind && meta.id == m_selectedVanillaId) {
                metaCategory = meta.category;
                metaDesc = meta.description;
                break;
            }
        }

        ImGui::Text("Category: %s", metaCategory.c_str());
        ImGui::Separator();
        if (!metaDesc.empty()) {
            ImGui::TextWrapped("Description:\n%s", metaDesc.c_str());
        }

        // Debug / Error Tracker Section
        ImGui::Spacing();
        ImGui::Separator();
        bool isMarked = isEntityMarked(m_selectedVanillaKind, m_selectedVanillaId);
        if (isMarked) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.2f, 0.2f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.25f, 0.3f, 0.8f));
        }
        if (ImGui::CollapsingHeader("Entity Debug & Issue Tracker", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool markedChecked = isMarked;
            if (ImGui::Checkbox("Flag this entity (has sprite, script or visual error)", &markedChecked)) {
                if (markedChecked) {
                    markEntity(m_selectedVanillaKind, m_selectedVanillaId, vName, metaCategory, "");
                } else {
                    unmarkEntity(m_selectedVanillaKind, m_selectedVanillaId);
                }
            }

            if (markedChecked) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "[!] Entity Flagged for Review");
                ImGui::Text("Issue notes (for Antigravity AI / dev review):");
                char noteBuf[512] = "";
                std::string curNote = getEntityMarkNote(m_selectedVanillaKind, m_selectedVanillaId);
                std::snprintf(noteBuf, sizeof(noteBuf), "%s", curNote.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextMultiline("##IssueNotes", noteBuf, sizeof(noteBuf), ImVec2(-1, 60))) {
                    setEntityMarkNote(m_selectedVanillaKind, m_selectedVanillaId, noteBuf);
                }
                if (ImGui::Button("Save Notes", ImVec2(100, 24))) {
                    saveMarkedEntities();
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear Flag", ImVec2(100, 24))) {
                    unmarkEntity(m_selectedVanillaKind, m_selectedVanillaId);
                }
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 0.9f));
        if (ImGui::Button("Clone as Editable Custom Entity", ImVec2(-1, 30))) {
            CustomEntityDefinition cloned;
            cloned.id = "custom_" + std::to_string(m_selectedVanillaId);
            cloned.name = "Custom " + vName;
            cloned.baseKind = static_cast<u8>(m_selectedVanillaKind);
            cloned.baseId = static_cast<u8>(m_selectedVanillaId);
            cloned.health = 4;
            cloned.damage = 2;
            cloned.moveSpeed = 1.0f;
            cloned.scriptBlocks.push_back({ ScriptBlockType::EVENT_ON_SPAWN, "On Spawn", "", 0, 0 });
            cloned.scriptBlocks.push_back({ ScriptBlockType::ACTION_CHANGE_ANIM, "Play Animation", "Idle South", 0, 0 });

            m_customEntities.push_back(cloned);
            m_selectedEntityIndex = static_cast<int>(m_customEntities.size()) - 1;
            m_selectedSidebarTab = 0; // Switch to custom tab
            updatePreviewTexture();
        }
        ImGui::PopStyleColor();
        return;
    }

    if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(m_customEntities.size())) {
        ImGui::Text("No entity selected.");
        return;
    }

    auto& ent = m_customEntities[m_selectedEntityIndex];
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Entity Identity & Stats:");
    ImGui::Separator();

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", ent.name.c_str());
    if (ImGui::InputText("Display Name", nameBuf, sizeof(nameBuf))) {
        ent.name = nameBuf;
    }

    char idBuf[64];
    std::snprintf(idBuf, sizeof(idBuf), "%s", ent.id.c_str());
    if (ImGui::InputText("Identifier (Key)", idBuf, sizeof(idBuf))) {
        ent.id = idBuf;
    }

    ImGui::Separator();
    ImGui::Text("Base Template Engine Hook:");

    // Kind Selector
    const char* kindNames[] = {
        "Player (1)", "Enemy (3)", "Projectile (4)", "Object (6)", "NPC (7)", "Player Item (8)", "Manager (9)"
    };
    const u8 kindValues[] = { 1, 3, 4, 6, 7, 8, 9 };
    int curKindIdx = 1;
    for (int k = 0; k < 7; ++k) {
        if (kindValues[k] == ent.baseKind) {
            curKindIdx = k;
            break;
        }
    }
    if (ImGui::Combo("Base Kind", &curKindIdx, kindNames, IM_ARRAYSIZE(kindNames))) {
        ent.baseKind = kindValues[curKindIdx];
        updatePreviewTexture();
    }

    // Base ID Selector
    std::vector<const EntityMeta*> matching;
    for (const auto& meta : TmcDatabase::getEntityRegistry()) {
        if (meta.kind == ent.baseKind) {
            matching.push_back(&meta);
        }
    }

    std::string baseName = TmcDatabase::getEntityName(ent.baseKind, ent.baseId);
    if (ImGui::BeginCombo("Base Prototype", baseName.c_str())) {
        for (const auto* meta : matching) {
            bool isSel = (meta->id == ent.baseId);
            char lBuf[128];
            std::snprintf(lBuf, sizeof(lBuf), "0x%02X - %s", meta->id, meta->name.c_str());
            if (ImGui::Selectable(lBuf, isSel)) {
                ent.baseId = meta->id;
                updatePreviewTexture();
            }
            if (isSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("Combat & Movement Attributes:");
    ImGui::SliderInt("Health (HP)", &ent.health, 1, 64);
    ImGui::SliderInt("Damage Output", &ent.damage, 0, 16);
    ImGui::SliderFloat("Move Speed", &ent.moveSpeed, 0.2f, 5.0f, "%.1f px/f");

    ImGui::Separator();
    ImGui::Text("Layer Interaction:");
    if (ImGui::RadioButton("Layer 1 (Normal / Overhead)", ent.defaultCollisionLayer == 1)) {
        ent.defaultCollisionLayer = 1;
    }
    if (ImGui::RadioButton("Layer 2 (Lower / Ground)", ent.defaultCollisionLayer == 2)) {
        ent.defaultCollisionLayer = 2;
    }
}

} // namespace tmc
