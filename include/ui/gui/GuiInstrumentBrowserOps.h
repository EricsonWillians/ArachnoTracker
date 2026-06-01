#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

std::vector<int> filteredInstrumentIndicesForQuery(
    const AppSessionSnapshot& snapshot,
    const std::string& query);

struct GuiInstrumentBrowserOpenContext {
    bool& audioTuningDialogActive;
    bool& instrumentBrowserActive;
    std::string& instrumentBrowserQuery;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    int armedInstrument = 0;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
};

void openInstrumentBrowser(
    const GuiInstrumentBrowserOpenContext& context,
    const AppSessionSnapshot& snapshot);

struct GuiInstrumentBrowserCloseContext {
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
    std::function<void(int)> selectInstrument;

    bool& instrumentBrowserActive;
    std::string& instrumentBrowserQuery;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    UiRect& instrumentBrowserListRect;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;
};

void closeInstrumentBrowser(const GuiInstrumentBrowserCloseContext& context, bool applySelection);

void cycleInstrumentBy(
    int delta,
    int armedInstrument,
    const AppSessionSnapshot& snapshot,
    const std::function<void(int)>& selectInstrument);

void clampInstrumentListWindow(
    const AppSessionSnapshot& snapshot,
    int instrumentListVisibleRows,
    int& instrumentListStart);

void scrollInstrumentList(
    int delta,
    const AppSessionSnapshot& snapshot,
    int instrumentListVisibleRows,
    int& instrumentListStart);

} // namespace arachno
