#pragma once

#include "tmc_types.hpp"
#include <string>
#include <vector>
#include <deque>
#include <functional>

namespace tmc {

class RomDataReader;

struct UndoState {
    std::string description;
    std::vector<u16> layerBottom;
    std::vector<u16> layerTop;
    std::vector<u8> collision;
    std::vector<u8> actTiles;
    std::vector<EntityData> entitiesMain;
    std::vector<TileEntity> tileEntities;
    std::vector<Transition> transitions;
};

class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();

    // Active project state
    RoomModel& getActiveRoom() { return m_activeRoom; }
    const RoomModel& getActiveRoom() const { return m_activeRoom; }
    TileSetData& getActiveTileset() { return m_activeTileset; }
    const TileSetData& getActiveTileset() const { return m_activeTileset; }

    const std::string& getProjectPath() const { return m_projectPath; }
    bool isModified() const { return m_modified; }
    void markModified() { m_modified = true; }

    // Project file operations (JSON)
    bool newProject(u8 area, u8 room);
    bool saveProject(const std::string& filePath);
    bool loadProject(const std::string& filePath);

    // Export Mod for Project Picori PC Port
    bool exportMod(const std::string& modName, const std::string& modsDirectory, const RomDataReader* romReader = nullptr);
    bool launchGamePlaytest(const std::string& modName, const std::string& tmcExecutablePath);

    // Undo / Redo system
    void pushUndo(const std::string& description);
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    void undo();
    void redo();

    // Helper tile manipulators with undo tracking
    void setTile(size_t layerIndex, size_t x, size_t y, u16 tileIndex);
    void setCollision(size_t x, size_t y, u8 colType);
    void setActTile(size_t x, size_t y, u8 actType);

private:
    RoomModel   m_activeRoom;
    TileSetData m_activeTileset;
    std::string m_projectPath;
    bool        m_modified = false;

    std::deque<UndoState> m_undoStack;
    std::deque<UndoState> m_redoStack;
    static constexpr size_t MAX_UNDO_DEPTH = 50;

    UndoState captureCurrentState(const std::string& desc);
    void restoreState(const UndoState& state);
};

} // namespace tmc
