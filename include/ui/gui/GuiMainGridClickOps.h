#pragma once

#include <functional>
#include <string>
#include <utility>

namespace arachno {

struct GuiMainGridClickResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainGridClickContext {
    int button = 0;
    int mx = 0;
    int my = 0;
    bool altDown = false;
    bool shiftDown = false;

    int& paintNoteMidi;
    bool& keyboardSelectionActive;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;
    bool& draggingSelection;
    int& dragAnchorRow;
    int& dragAnchorTrack;

    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<std::pair<int, int>()> currentCursor;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainGridClickResult handleMainGridClick(const GuiMainGridClickContext& context);

} // namespace arachno
