#include "ui/gui/GuiSynthTopSectionOps.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "GuiInput.h"

namespace arachno {

GuiSynthTopSectionResult drawSynthTopSection(const GuiSynthTopSectionContext& context) {
    GuiSynthTopSectionResult result;
    result.panelRight = std::max(result.panelLeft + 120, context.synthWindowWidth - 16);
    const bool compactTop = context.synthWindowWidth < 980;
    const int navY = 64;
    const int buttonH = 22;
    const int buttonGap = 4;

    std::string instLabel = "INST " + std::to_string(context.instrument) + "  " + context.patch.name;
    context.drawText(
        result.panelLeft,
        54,
        context.fitText(instLabel, result.panelRight - result.panelLeft - 4),
        context.textColor);

    const UiRect closeRect {result.panelRight - (compactTop ? 62 : 70), navY, compactTop ? 62 : 70, buttonH};
    context.drawButton(closeRect, "CLOSE", false);
    context.synthWindowHits.push_back({closeRect, "window_close", "", "", "", 0.0});

    int navX = result.panelLeft;
    auto addNavButton = [&](const std::string& label, int width, const std::string& kind) {
        if (navX + width > closeRect.x - 6) {
            return false;
        }
        const UiRect rect {navX, navY, width, buttonH};
        context.drawButton(rect, label, false);
        context.synthWindowHits.push_back({rect, kind, "", "", "", 0.0});
        navX += width + buttonGap;
        return true;
    };
    (void)addNavButton("<", 30, "inst_prev");
    (void)addNavButton(">", 30, "inst_next");
    (void)addNavButton("NEW", compactTop ? 52 : 64, "inst_new");
    (void)addNavButton("CLONE", compactTop ? 60 : 70, "inst_clone");
    (void)addNavButton("RENAME", compactTop ? 70 : 80, "inst_rename");
    (void)addNavButton("AUD", compactTop ? 52 : 60, "inst_aud");

    const int filesY = navY + buttonH + 6;
    int fileX = result.panelLeft;
    auto addFileButton = [&](const std::string& label, int width, const std::string& kind) {
        const UiRect rect {fileX, filesY, width, buttonH};
        context.drawButton(rect, label, false);
        context.synthWindowHits.push_back({rect, kind, "", "", "", 0.0});
        fileX += width + buttonGap;
        return rect;
    };
    addFileButton(compactTop ? "ADD" : "ADD PATCH", compactTop ? 62 : 106, "patch_import_new");
    addFileButton(compactTop ? "LOAD" : "LOAD PATCH", compactTop ? 66 : 112, "patch_import_replace");
    addFileButton(compactTop ? "ALL" : "LOAD ALL", compactTop ? 60 : 96, "patch_import_replace_all");
    const UiRect exportRect = addFileButton(compactTop ? "SAVE" : "SAVE PATCH", compactTop ? 66 : 112, "patch_export");

    const std::array<std::pair<const char*, int>, 4> paramPages {{
        {compactTop ? "SND" : "SOUND", 0},
        {"MOD", 1},
        {"FX", 2},
        {"ENV", 3},
    }};
    int pageW = compactTop ? 48 : 70;
    int pageX = result.panelRight - ((pageW + buttonGap) * static_cast<int>(paramPages.size()));
    if (pageX <= exportRect.x + exportRect.width + 8) {
        pageX = exportRect.x + exportRect.width + 12;
        pageW = compactTop ? 44 : 56;
    }
    for (const auto& page : paramPages) {
        const UiRect pageRect {pageX, filesY, pageW, buttonH};
        const bool active = context.synthParamPage == page.second;
        context.drawButton(pageRect, page.first, active);
        context.synthWindowHits.push_back({pageRect, "param_page", "", "", "", static_cast<double>(page.second)});
        pageX += pageW + buttonGap;
    }

    result.oscTop = filesY + buttonH + 10;
    const int oscToggleWidth = 46;
    const int oscNameX = result.panelLeft;
    const int oscWaveStartX = result.panelLeft + 60;
    const int oscToggleX = result.panelRight - oscToggleWidth;
    const int oscWaveGap = 4;
    context.drawText(oscNameX, result.oscTop + 14, "OSC A", context.textColor);
    context.drawText(oscNameX, result.oscTop + 14 + result.oscRowHeight, "OSC B", context.textColor);
    context.drawText(oscNameX, result.oscTop + 14 + (result.oscRowHeight * 2), "OSC C", context.textColor);
    context.drawText(oscNameX, result.oscTop + 14 + (result.oscRowHeight * 3), "OSC D", context.textColor);
    const UiRect oscAOnRect {oscToggleX, result.oscTop, oscToggleWidth, 20};
    const UiRect oscBOnRect {oscToggleX, result.oscTop + result.oscRowHeight, oscToggleWidth, 20};
    const UiRect oscCOnRect {oscToggleX, result.oscTop + (result.oscRowHeight * 2), oscToggleWidth, 20};
    const UiRect oscDOnRect {oscToggleX, result.oscTop + (result.oscRowHeight * 3), oscToggleWidth, 20};
    context.drawButton(oscAOnRect, context.patch.oscillatorAEnabled ? "ON" : "OFF", context.patch.oscillatorAEnabled);
    context.drawButton(oscBOnRect, context.patch.oscillatorBEnabled ? "ON" : "OFF", context.patch.oscillatorBEnabled);
    context.drawButton(oscCOnRect, context.patch.oscillatorCEnabled ? "ON" : "OFF", context.patch.oscillatorCEnabled);
    context.drawButton(oscDOnRect, context.patch.oscillatorDEnabled ? "ON" : "OFF", context.patch.oscillatorDEnabled);
    context.synthWindowHits.push_back(
        {oscAOnRect, "param_set", "osc_a_enabled", "", "", 0.0, context.patch.oscillatorAEnabled ? 0.0 : 1.0});
    context.synthWindowHits.push_back(
        {oscBOnRect, "param_set", "osc_b_enabled", "", "", 0.0, context.patch.oscillatorBEnabled ? 0.0 : 1.0});
    context.synthWindowHits.push_back(
        {oscCOnRect, "param_set", "osc_c_enabled", "", "", 0.0, context.patch.oscillatorCEnabled ? 0.0 : 1.0});
    context.synthWindowHits.push_back(
        {oscDOnRect, "param_set", "osc_d_enabled", "", "", 0.0, context.patch.oscillatorDEnabled ? 0.0 : 1.0});

    const std::vector<std::pair<std::string, std::string>> waves {
        {"sine", "SIN"},
        {"square", "SQR"},
        {"saw", "SAW"},
        {"triangle", "TRI"},
        {"noise", "NOI"},
        {"supersaw", "SUP"},
    };
    const std::string oscAName = lowerCopy(waveformName(context.patch.oscillatorA));
    const std::string oscBName = lowerCopy(waveformName(context.patch.oscillatorB));
    const std::string oscCName = lowerCopy(waveformName(context.patch.oscillatorC));
    const std::string oscDName = lowerCopy(waveformName(context.patch.oscillatorD));
    const int waveColumns = static_cast<int>(waves.size());
    const int totalWaveGap = (waveColumns - 1) * oscWaveGap;
    const int waveAvailable = std::max(140, oscToggleX - oscWaveStartX - 8);
    const int waveButtonWidth = std::max(34, std::min(52, (waveAvailable - totalWaveGap) / waveColumns));
    int wx = oscWaveStartX;
    for (const auto& wave : waves) {
        const UiRect aRect {wx, result.oscTop, waveButtonWidth, 20};
        const UiRect bRect {wx, result.oscTop + result.oscRowHeight, waveButtonWidth, 20};
        const UiRect cRect {wx, result.oscTop + (result.oscRowHeight * 2), waveButtonWidth, 20};
        const UiRect dRect {wx, result.oscTop + (result.oscRowHeight * 3), waveButtonWidth, 20};
        context.drawButton(aRect, wave.second, wave.first == oscAName);
        context.drawButton(bRect, wave.second, wave.first == oscBName);
        context.drawButton(cRect, wave.second, wave.first == oscCName);
        context.drawButton(dRect, wave.second, wave.first == oscDName);
        context.synthWindowHits.push_back({aRect, "wave", "", "A", wave.first});
        context.synthWindowHits.push_back({bRect, "wave", "", "B", wave.first});
        context.synthWindowHits.push_back({cRect, "wave", "", "C", wave.first});
        context.synthWindowHits.push_back({dRect, "wave", "", "D", wave.first});
        wx += waveButtonWidth + oscWaveGap;
    }

    return result;
}

} // namespace arachno
