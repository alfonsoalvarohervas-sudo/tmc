#include "project_manager.hpp"
#include "rom_data_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace tmc {

ProjectManager::ProjectManager() {
    newProject(0, 0);
}

ProjectManager::~ProjectManager() = default;

bool ProjectManager::newProject(u8 area, u8 room) {
    m_activeRoom = RoomModel();
    m_activeRoom.areaIndex = area;
    m_activeRoom.roomIndex = room;
    m_activeRoom.name = "Custom Room";
    m_activeRoom.header.pixel_width = 256;
    m_activeRoom.header.pixel_height = 256;

    m_activeTileset = TileSetData();
    m_activeTileset.metaTilesBottom.resize(512);
    m_activeTileset.metaTilesTop.resize(512);

    m_undoStack.clear();
    m_redoStack.clear();
    m_projectPath.clear();
    m_modified = false;
    return true;
}

UndoState ProjectManager::captureCurrentState(const std::string& desc) {
    UndoState state;
    state.description = desc;
    state.layerBottom = m_activeRoom.layerBottom;
    state.layerTop = m_activeRoom.layerTop;
    state.collision = m_activeRoom.collision;
    state.actTiles = m_activeRoom.actTiles;
    state.entitiesMain = m_activeRoom.entitiesMain;
    state.tileEntities = m_activeRoom.tileEntities;
    state.transitions = m_activeRoom.transitions;
    return state;
}

void ProjectManager::restoreState(const UndoState& state) {
    m_activeRoom.layerBottom = state.layerBottom;
    m_activeRoom.layerTop = state.layerTop;
    m_activeRoom.collision = state.collision;
    m_activeRoom.actTiles = state.actTiles;
    m_activeRoom.entitiesMain = state.entitiesMain;
    m_activeRoom.tileEntities = state.tileEntities;
    m_activeRoom.transitions = state.transitions;
    m_modified = true;
}

void ProjectManager::pushUndo(const std::string& description) {
    m_undoStack.push_back(captureCurrentState(description));
    if (m_undoStack.size() > MAX_UNDO_DEPTH) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
    m_modified = true;
}

void ProjectManager::undo() {
    if (m_undoStack.empty()) return;
    m_redoStack.push_back(captureCurrentState("Redo State"));
    UndoState prev = m_undoStack.back();
    m_undoStack.pop_back();
    restoreState(prev);
}

void ProjectManager::redo() {
    if (m_redoStack.empty()) return;
    m_undoStack.push_back(captureCurrentState("Undo State"));
    UndoState next = m_redoStack.back();
    m_redoStack.pop_back();
    restoreState(next);
}

void ProjectManager::setTile(size_t layerIndex, size_t x, size_t y, u16 tileIndex) {
    if (x >= MAX_MAP_WIDTH || y >= MAX_MAP_HEIGHT) return;
    size_t idx = y * MAX_MAP_WIDTH + x;
    auto& targetLayer = (layerIndex == 0) ? m_activeRoom.layerBottom : m_activeRoom.layerTop;
    if (idx < targetLayer.size() && targetLayer[idx] != tileIndex) {
        targetLayer[idx] = tileIndex;
        m_modified = true;
    }
}

void ProjectManager::setCollision(size_t x, size_t y, u8 colType) {
    if (x >= MAX_MAP_WIDTH || y >= MAX_MAP_HEIGHT) return;
    size_t idx = y * MAX_MAP_WIDTH + x;
    if (idx < m_activeRoom.collision.size() && m_activeRoom.collision[idx] != colType) {
        m_activeRoom.collision[idx] = colType;
        m_modified = true;
    }
}

void ProjectManager::setActTile(size_t x, size_t y, u8 actType) {
    if (x >= MAX_MAP_WIDTH || y >= MAX_MAP_HEIGHT) return;
    size_t idx = y * MAX_MAP_WIDTH + x;
    if (idx < m_activeRoom.actTiles.size() && m_activeRoom.actTiles[idx] != actType) {
        m_activeRoom.actTiles[idx] = actType;
        m_modified = true;
    }
}

bool ProjectManager::saveProject(const std::string& filePath) {
    json j;
    j["version"] = 1;
    j["room_name"] = m_activeRoom.name;
    j["area_index"] = m_activeRoom.areaIndex;
    j["room_index"] = m_activeRoom.roomIndex;

    j["header"] = {
        { "map_x", m_activeRoom.header.map_x },
        { "map_y", m_activeRoom.header.map_y },
        { "pixel_width", m_activeRoom.header.pixel_width },
        { "pixel_height", m_activeRoom.header.pixel_height },
        { "tileSet_id", m_activeRoom.header.tileSet_id }
    };

    j["portal"] = {
        { "in_use", m_activeRoom.portal.in_use },
        { "type", m_activeRoom.portal.type },
        { "x", m_activeRoom.portal.x },
        { "y", m_activeRoom.portal.y },
        { "exit_dir", m_activeRoom.portal.exit_dir },
        { "mode", m_activeRoom.portal.mode }
    };

    j["layer_bottom"] = m_activeRoom.layerBottom;
    j["layer_top"] = m_activeRoom.layerTop;
    j["collision"] = m_activeRoom.collision;
    j["act_tiles"] = m_activeRoom.actTiles;

    // Entities Main (Prop 0)
    json entsMain = json::array();
    for (const auto& ent : m_activeRoom.entitiesMain) {
        entsMain.push_back({
            { "kind", ent.kind },
            { "flags", ent.flags },
            { "id", ent.id },
            { "type", ent.type },
            { "type2", ent.type2 },
            { "xPos", ent.xPos },
            { "yPos", ent.yPos },
            { "spritePtr", ent.spritePtr }
        });
    }
    j["entities_main"] = entsMain;

    // Entities Secondary (Prop 1)
    json entsSec = json::array();
    for (const auto& ent : m_activeRoom.entitiesSecondary) {
        entsSec.push_back({
            { "kind", ent.kind },
            { "flags", ent.flags },
            { "id", ent.id },
            { "type", ent.type },
            { "type2", ent.type2 },
            { "xPos", ent.xPos },
            { "yPos", ent.yPos },
            { "spritePtr", ent.spritePtr }
        });
    }
    j["entities_secondary"] = entsSec;

    // Entities Respawn / Enemies (Prop 2)
    json entsRespawn = json::array();
    for (const auto& ent : m_activeRoom.entitiesRespawn) {
        entsRespawn.push_back({
            { "kind", ent.kind },
            { "flags", ent.flags },
            { "id", ent.id },
            { "type", ent.type },
            { "type2", ent.type2 },
            { "xPos", ent.xPos },
            { "yPos", ent.yPos },
            { "spritePtr", ent.spritePtr }
        });
    }
    j["entities_respawn"] = entsRespawn;

    // Tile Entities
    json tes = json::array();
    for (const auto& te : m_activeRoom.tileEntities) {
        tes.push_back({
            { "type", te.type },
            { "localFlag", te.localFlag },
            { "param2", te.param2 },
            { "param3", te.param3 },
            { "tilePos", te.tilePos },
            { "param6", te.param6 },
            { "param7", te.param7 }
        });
    }
    j["tile_entities"] = tes;

    // Transitions
    json trans = json::array();
    for (const auto& tr : m_activeRoom.transitions) {
        trans.push_back({
            { "warp_type", tr.warp_type },
            { "startX", tr.startX },
            { "startY", tr.startY },
            { "endX", tr.endX },
            { "endY", tr.endY },
            { "shape", tr.shape },
            { "destArea", tr.destArea },
            { "destRoom", tr.destRoom },
            { "playerLayer", tr.playerLayer },
            { "transition_type", tr.transition_type },
            { "facing_direction", tr.facing_direction },
            { "transitionSFX", tr.transitionSFX }
        });
    }
    j["transitions"] = trans;

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    m_projectPath = filePath;
    m_modified = false;
    return true;
}

bool ProjectManager::loadProject(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    json j;
    file >> j;

    m_activeRoom.name = j.value("room_name", "Custom Room");
    m_activeRoom.areaIndex = j.value("area_index", 0);
    m_activeRoom.roomIndex = j.value("room_index", 0);

    if (j.contains("header")) {
        m_activeRoom.header.map_x = j["header"].value("map_x", 0);
        m_activeRoom.header.map_y = j["header"].value("map_y", 0);
        m_activeRoom.header.pixel_width = j["header"].value("pixel_width", 256);
        m_activeRoom.header.pixel_height = j["header"].value("pixel_height", 256);
        m_activeRoom.header.tileSet_id = j["header"].value("tileSet_id", 0);
    }

    if (j.contains("portal")) {
        m_activeRoom.portal.in_use = j["portal"].value("in_use", false);
        m_activeRoom.portal.type = j["portal"].value("type", 0);
        m_activeRoom.portal.x = j["portal"].value("x", 0);
        m_activeRoom.portal.y = j["portal"].value("y", 0);
        m_activeRoom.portal.exit_dir = j["portal"].value("exit_dir", 0);
        m_activeRoom.portal.mode = j["portal"].value("mode", 0);
    }

    if (j.contains("layer_bottom")) m_activeRoom.layerBottom = j["layer_bottom"].get<std::vector<u16>>();
    if (j.contains("layer_top"))    m_activeRoom.layerTop = j["layer_top"].get<std::vector<u16>>();
    if (j.contains("collision"))    m_activeRoom.collision = j["collision"].get<std::vector<u8>>();
    if (j.contains("act_tiles"))    m_activeRoom.actTiles = j["act_tiles"].get<std::vector<u8>>();

    m_activeRoom.entitiesMain.clear();
    if (j.contains("entities_main")) {
        for (const auto& item : j["entities_main"]) {
            EntityData ent;
            ent.kind = item.value("kind", 0);
            ent.flags = item.value("flags", 0);
            ent.id = item.value("id", 0);
            ent.type = item.value("type", 0);
            ent.type2 = item.value("type2", 0);
            ent.xPos = item.value("xPos", 0);
            ent.yPos = item.value("yPos", 0);
            ent.spritePtr = item.value("spritePtr", 0);
            m_activeRoom.entitiesMain.push_back(ent);
        }
    }

    m_activeRoom.entitiesSecondary.clear();
    if (j.contains("entities_secondary")) {
        for (const auto& item : j["entities_secondary"]) {
            EntityData ent;
            ent.kind = item.value("kind", 0);
            ent.flags = item.value("flags", 0);
            ent.id = item.value("id", 0);
            ent.type = item.value("type", 0);
            ent.type2 = item.value("type2", 0);
            ent.xPos = item.value("xPos", 0);
            ent.yPos = item.value("yPos", 0);
            ent.spritePtr = item.value("spritePtr", 0);
            m_activeRoom.entitiesSecondary.push_back(ent);
        }
    }

    m_activeRoom.entitiesRespawn.clear();
    if (j.contains("entities_respawn")) {
        for (const auto& item : j["entities_respawn"]) {
            EntityData ent;
            ent.kind = item.value("kind", 0);
            ent.flags = item.value("flags", 0);
            ent.id = item.value("id", 0);
            ent.type = item.value("type", 0);
            ent.type2 = item.value("type2", 0);
            ent.xPos = item.value("xPos", 0);
            ent.yPos = item.value("yPos", 0);
            ent.spritePtr = item.value("spritePtr", 0);
            m_activeRoom.entitiesRespawn.push_back(ent);
        }
    }

    m_activeRoom.tileEntities.clear();
    if (j.contains("tile_entities")) {
        for (const auto& item : j["tile_entities"]) {
            TileEntity te;
            te.type = item.value("type", 0);
            te.localFlag = item.value("localFlag", 0);
            te.param2 = item.value("param2", 0);
            te.param3 = item.value("param3", 0);
            te.tilePos = item.value("tilePos", 0);
            te.param6 = item.value("param6", 0);
            te.param7 = item.value("param7", 0);
            m_activeRoom.tileEntities.push_back(te);
        }
    }

    m_activeRoom.transitions.clear();
    if (j.contains("transitions")) {
        for (const auto& item : j["transitions"]) {
            Transition tr;
            tr.warp_type = item.value("warp_type", 0);
            tr.startX = item.value("startX", 0);
            tr.startY = item.value("startY", 0);
            tr.endX = item.value("endX", 0);
            tr.endY = item.value("endY", 0);
            tr.shape = item.value("shape", 0);
            tr.destArea = item.value("destArea", 0);
            tr.destRoom = item.value("destRoom", 0);
            tr.playerLayer = item.value("playerLayer", 1);
            tr.transition_type = item.value("transition_type", 0);
            tr.facing_direction = item.value("facing_direction", 0);
            tr.transitionSFX = item.value("transitionSFX", 0);
            m_activeRoom.transitions.push_back(tr);
        }
    }

    m_undoStack.clear();
    m_redoStack.clear();
    m_projectPath = filePath;
    m_modified = false;
    return true;
}

bool ProjectManager::exportMod(const std::string& modName, const std::string& modsDirectory, const RomDataReader* romReader) {
    try {
        fs::path modPath = fs::path(modsDirectory) / modName;
        fs::create_directories(modPath / "room_props");
        fs::create_directories(modPath / "room_properties");
        fs::create_directories(modPath / "tilemaps");
        fs::create_directories(modPath / "rom_data");

        // 1. Export mod_manifest.json
        json manifest;
        manifest["name"] = modName;
        manifest["description"] = "Custom room map generated by TMC Level Editor";
        manifest["version"] = "1.0.0";
        manifest["target_area"] = m_activeRoom.areaIndex;
        manifest["target_room"] = m_activeRoom.roomIndex;

        json replacements = json::object();

        // 2. Export Room Properties binary
        // Prop 0: Entities Main
        std::string prop0Filename = "room_prop_area" + std::to_string(m_activeRoom.areaIndex) +
                                    "_room" + std::to_string(m_activeRoom.roomIndex) + "_p0.bin";
        std::ofstream prop0File(modPath / "room_props" / prop0Filename, std::ios::binary);
        if (prop0File.is_open()) {
            for (const auto& ent : m_activeRoom.entitiesMain) {
                u8 buf[16] = {};
                buf[0] = ent.kind;
                buf[1] = ent.flags;
                buf[2] = ent.id;
                buf[3] = ent.type;
                std::memcpy(&buf[4], &ent.type2, 4);
                std::memcpy(&buf[8], &ent.xPos, 2);
                std::memcpy(&buf[10], &ent.yPos, 2);
                std::memcpy(&buf[12], &ent.spritePtr, 4);
                prop0File.write(reinterpret_cast<char*>(buf), 16);
            }
            u8 term[16] = { 0xFF };
            prop0File.write(reinterpret_cast<char*>(term), 16);
        }

        // Prop 1: Entities Secondary
        std::string prop1Filename = "room_prop_area" + std::to_string(m_activeRoom.areaIndex) +
                                    "_room" + std::to_string(m_activeRoom.roomIndex) + "_p1.bin";
        std::ofstream prop1File(modPath / "room_props" / prop1Filename, std::ios::binary);
        if (prop1File.is_open()) {
            for (const auto& ent : m_activeRoom.entitiesSecondary) {
                u8 buf[16] = {};
                buf[0] = ent.kind;
                buf[1] = ent.flags;
                buf[2] = ent.id;
                buf[3] = ent.type;
                std::memcpy(&buf[4], &ent.type2, 4);
                std::memcpy(&buf[8], &ent.xPos, 2);
                std::memcpy(&buf[10], &ent.yPos, 2);
                std::memcpy(&buf[12], &ent.spritePtr, 4);
                prop1File.write(reinterpret_cast<char*>(buf), 16);
            }
            u8 term[16] = { 0xFF };
            prop1File.write(reinterpret_cast<char*>(term), 16);
        }

        // Prop 2: Entities Respawn / Enemies
        std::string prop2Filename = "room_prop_area" + std::to_string(m_activeRoom.areaIndex) +
                                    "_room" + std::to_string(m_activeRoom.roomIndex) + "_p2.bin";
        std::ofstream prop2File(modPath / "room_props" / prop2Filename, std::ios::binary);
        if (prop2File.is_open()) {
            for (const auto& ent : m_activeRoom.entitiesRespawn) {
                u8 buf[16] = {};
                buf[0] = ent.kind;
                buf[1] = ent.flags;
                buf[2] = ent.id;
                buf[3] = ent.type;
                std::memcpy(&buf[4], &ent.type2, 4);
                std::memcpy(&buf[8], &ent.xPos, 2);
                std::memcpy(&buf[10], &ent.yPos, 2);
                std::memcpy(&buf[12], &ent.spritePtr, 4);
                prop2File.write(reinterpret_cast<char*>(buf), 16);
            }
            u8 term[16] = { 0xFF };
            prop2File.write(reinterpret_cast<char*>(term), 16);
        }

        // 3. Export Tile Entities binary (Prop 3)
        std::string prop3Filename = "room_prop_area" + std::to_string(m_activeRoom.areaIndex) +
                                    "_room" + std::to_string(m_activeRoom.roomIndex) + "_p3.bin";
        std::ofstream prop3File(modPath / "room_props" / prop3Filename, std::ios::binary);
        if (prop3File.is_open()) {
            for (const auto& te : m_activeRoom.tileEntities) {
                u8 buf[8] = {};
                buf[0] = te.type;
                buf[1] = te.localFlag;
                buf[2] = te.param2;
                buf[3] = te.param3;
                std::memcpy(&buf[4], &te.tilePos, 2);
                buf[6] = te.param6;
                buf[7] = te.param7;
                prop3File.write(reinterpret_cast<char*>(buf), 8);
            }
            u8 term[8] = { 0x00 };
            prop3File.write(reinterpret_cast<char*>(term), 8);
        }

        // 4. Export Metatile Streams (BG2 & BG1) both raw and LZ77 compressed
        int tWidth = m_activeRoom.header.tileWidth();
        int tHeight = m_activeRoom.header.tileHeight();
        size_t totalTiles = static_cast<size_t>(tWidth * tHeight);

        std::vector<u8> bg2Stream(totalTiles * 2, 0);
        std::vector<u8> bg1Stream(totalTiles * 2, 0);
        for (int y = 0; y < tHeight; ++y) {
            for (int x = 0; x < tWidth; ++x) {
                size_t roomIdx = static_cast<size_t>(y * tWidth + x);
                size_t mapIdx = static_cast<size_t>(y * MAX_MAP_WIDTH + x);
                
                u16 meta2 = (mapIdx < m_activeRoom.layerBottom.size()) ? m_activeRoom.layerBottom[mapIdx] : 0;
                bg2Stream[roomIdx * 2 + 0] = static_cast<u8>(meta2 & 0xFF);
                bg2Stream[roomIdx * 2 + 1] = static_cast<u8>((meta2 >> 8) & 0xFF);

                u16 meta1 = (mapIdx < m_activeRoom.layerTop.size()) ? m_activeRoom.layerTop[mapIdx] : 0;
                bg1Stream[roomIdx * 2 + 0] = static_cast<u8>(meta1 & 0xFF);
                bg1Stream[roomIdx * 2 + 1] = static_cast<u8>((meta1 >> 8) & 0xFF);
            }
        }

        std::string bg2TilemapName = "tilemap_area" + std::to_string(m_activeRoom.areaIndex) +
                                     "_room" + std::to_string(m_activeRoom.roomIndex) + "_bg2.bin";
        std::string bg1TilemapName = "tilemap_area" + std::to_string(m_activeRoom.areaIndex) +
                                     "_room" + std::to_string(m_activeRoom.roomIndex) + "_bg1.bin";

        std::ofstream bg2File(modPath / "tilemaps" / bg2TilemapName, std::ios::binary);
        if (bg2File.is_open()) {
            bg2File.write(reinterpret_cast<char*>(bg2Stream.data()), bg2Stream.size());
        }

        std::ofstream bg1File(modPath / "tilemaps" / bg1TilemapName, std::ios::binary);
        if (bg1File.is_open()) {
            bg1File.write(reinterpret_cast<char*>(bg1Stream.data()), bg1Stream.size());
        }

        // Compress metatile streams with LZ77
        std::vector<u8> bg2Lz = RomDataReader::compressLZ77(bg2Stream.data(), bg2Stream.size());
        std::vector<u8> bg1Lz = RomDataReader::compressLZ77(bg1Stream.data(), bg1Stream.size());

        std::string bg2LzName = bg2TilemapName + ".lz";
        std::string bg1LzName = bg1TilemapName + ".lz";

        std::ofstream bg2LzFile(modPath / "tilemaps" / bg2LzName, std::ios::binary);
        if (bg2LzFile.is_open()) {
            bg2LzFile.write(reinterpret_cast<char*>(bg2Lz.data()), bg2Lz.size());
        }

        std::ofstream bg1LzFile(modPath / "tilemaps" / bg1LzName, std::ios::binary);
        if (bg1LzFile.is_open()) {
            bg1LzFile.write(reinterpret_cast<char*>(bg1Lz.data()), bg1Lz.size());
        }

        // Map tilemaps replacements in manifest
        replacements["tilemaps/" + bg2TilemapName] = "tilemaps/" + bg2TilemapName;
        replacements["tilemaps/" + bg1TilemapName] = "tilemaps/" + bg1TilemapName;

        // Try reading USA area_room_maps.json to get exact relative asset path if present
        std::vector<std::string> mapJsonCandidates = {
            "dist/USA/assets/usa/area_room_maps.json",
            "assets/usa/area_room_maps.json",
            "build/pc/assets/usa/area_room_maps.json"
        };
        for (const auto& jsonPath : mapJsonCandidates) {
            if (fs::exists(jsonPath)) {
                std::ifstream jf(jsonPath);
                if (jf.is_open()) {
                    try {
                        json mapsJ;
                        jf >> mapsJ;
                        std::string aKey = std::to_string(m_activeRoom.areaIndex);
                        if (mapsJ.contains(aKey) && mapsJ[aKey].is_array() && m_activeRoom.roomIndex < mapsJ[aKey].size()) {
                            const auto& roomEntry = mapsJ[aKey][m_activeRoom.roomIndex];
                            if (roomEntry.is_array()) {
                                for (const auto& def : roomEntry) {
                                    std::string fPath = def.value("file", "");
                                    bool isCompressed = def.value("compressed", false);
                                    if (!fPath.empty()) {
                                        if (fPath.find("bottom") != std::string::npos) {
                                             replacements[fPath] = isCompressed ? ("tilemaps/" + bg2LzName) : ("tilemaps/" + bg2TilemapName);
                                        } else if (fPath.find("top") != std::string::npos) {
                                             replacements[fPath] = isCompressed ? ("tilemaps/" + bg1LzName) : ("tilemaps/" + bg1TilemapName);
                                        }
                                    }
                                }
                            }
                        }
                    } catch (...) {}
                    break;
                }
            }
        }

        // Try reading USA area_tables.json to get exact entity prop filenames
        std::vector<std::string> tableJsonCandidates = {
            "dist/USA/assets/usa/area_tables.json",
            "assets/usa/area_tables.json",
            "build/pc/assets/usa/area_tables.json"
        };
        for (const auto& jsonPath : tableJsonCandidates) {
            if (fs::exists(jsonPath)) {
                std::ifstream jf(jsonPath);
                if (jf.is_open()) {
                    try {
                        json tblJ;
                        jf >> tblJ;
                        std::string aKey = std::to_string(m_activeRoom.areaIndex);
                        if (tblJ.contains(aKey) && tblJ[aKey].is_array() && m_activeRoom.roomIndex < tblJ[aKey].size()) {
                            const auto& roomFiles = tblJ[aKey][m_activeRoom.roomIndex];
                            if (roomFiles.is_array()) {
                                if (roomFiles.size() > 0 && roomFiles[0].is_string()) {
                                    std::string p0 = roomFiles[0].get<std::string>();
                                    if (!p0.empty()) replacements[p0] = "room_props/" + prop0Filename;
                                }
                                if (roomFiles.size() > 1 && roomFiles[1].is_string()) {
                                    std::string p1 = roomFiles[1].get<std::string>();
                                    if (!p1.empty()) replacements[p1] = "room_props/" + prop1Filename;
                                }
                                if (roomFiles.size() > 2 && roomFiles[2].is_string()) {
                                    std::string p2 = roomFiles[2].get<std::string>();
                                    if (!p2.empty()) replacements[p2] = "room_props/" + prop2Filename;
                                }
                                if (roomFiles.size() > 3 && roomFiles[3].is_string()) {
                                    std::string p3 = roomFiles[3].get<std::string>();
                                    if (!p3.empty()) replacements[p3] = "room_props/" + prop3Filename;
                                }
                            }
                        }
                    } catch (...) {}
                    break;
                }
            }
        }

        manifest["replace"] = replacements;
        std::ofstream manifestFileOut(modPath / "mod_manifest.json");
        if (manifestFileOut.is_open()) {
            manifestFileOut << manifest.dump(2);
        }

        // Auto-save active project to persist editor edits
        saveProject("autosave.tmcproj");

        std::cout << "[ProjectManager] Mod exported successfully to: " << modPath.string() << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ProjectManager] Export failed: " << e.what() << std::endl;
        return false;
    }
}

bool ProjectManager::launchGamePlaytest(const std::string& modName, const std::string& tmcExecutablePath) {
    std::string resolvedPath = tmcExecutablePath;

    // Check candidate paths if specified path doesn't exist
    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
        const std::vector<std::string> candidates = {
            "build/pc/tmc_pc.exe",
            "dist/USA/tmc_pc.exe",
            "dist/EUR/tmc_pc.exe",
            "dist/EU/tmc_pc.exe",
            "tmc_pc.exe",
            "build/pc/tmc_pc",
            "dist/USA/tmc_pc",
            "dist/EUR/tmc_pc",
            "dist/EU/tmc_pc",
            "./tmc_pc"
        };
        for (const auto& path : candidates) {
            if (fs::exists(path)) {
                resolvedPath = path;
                break;
            }
        }
    }

    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
        std::cerr << "[ProjectManager] tmc_pc executable not found. Checked build/pc/tmc_pc.exe, dist/USA/tmc_pc.exe, etc." << std::endl;
        return false;
    }

    std::cout << "[ProjectManager] Launching playtest with executable: " << resolvedPath << std::endl;

#ifdef _WIN32
    // Launch on Windows with TMC_MODS environment variable set
    std::string envCmd = "cmd /c set \"TMC_MODS=" + modName + "\" && start \"\" \"" + resolvedPath + "\"";
    std::system(envCmd.c_str());
#else
    std::string envCmd = "TMC_MODS=" + modName + " \"" + resolvedPath + "\" &";
    std::system(envCmd.c_str());
#endif
    return true;
}

} // namespace tmc
