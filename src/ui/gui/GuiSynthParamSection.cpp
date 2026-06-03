#include "ui/gui/GuiSynthParamSection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace arachno {

void drawSynthParameterSection(const GuiSynthParamSectionContext& context) {
    const std::array<const char*, 4> pageTitles {{
        "SOUND SHAPING",
        "MODULATION",
        "FX / COLOR",
        "ENVELOPES",
    }};
    context.drawText(16, context.controlsTop, pageTitles[static_cast<std::size_t>(std::clamp(context.synthParamPage, 0, 3))], context.colors.mutedText);
    context.drawText(
        228,
        context.controlsTop,
        context.fitText(
            "Knobs: drag up/down (Shift=fine), +/- for steps | Hover labels for guidance",
            std::max(120, context.panelRight - 236)),
        context.colors.mutedText);
    if (context.synthParamPage == 1) {
        context.drawText(16, context.controlsTop + 18, "FM ALG 0:CASCADE 1:SPLIT 2:PARALLEL 3:METAL", context.colors.mutedText);
    } else if (context.synthParamPage == 2) {
        context.drawText(16, context.controlsTop + 18, "F MODE 0:LADDER 1:4P LADDER 2:MS BITE", context.colors.mutedText);
    }
    const int targetY = context.controlsTop + 24;
    context.drawText(16, targetY + 14, "TARGET OSC", context.colors.mutedText);
    int targetX = 78;
    const std::array<std::pair<const char*, int>, 5> oscTargets {{
        {"ALL", -1},
        {"A", 0},
        {"B", 1},
        {"C", 2},
        {"D", 3},
    }};
    for (const auto& entry : oscTargets) {
        const UiRect rect {targetX, targetY, 36, 18};
        const bool active = context.synthParamOscTarget == entry.second;
        context.drawButton(rect, entry.first, active);
        context.synthWindowHits.push_back({rect, "osc_target", "", "", "", 0.0, static_cast<double>(entry.second)});
        targetX += 40;
    }
    std::vector<const SynthParamDef*> visibleDefs;
    visibleDefs.reserve(context.synthParamDefs.size());
    for (const SynthParamDef& def : context.synthParamDefs) {
        if (!synthParamBelongsToPage(def.name, context.synthParamPage)) {
            continue;
        }
        // When an oscillator target is selected (A/B/C/D), filter to show only
        // parameters that belong to that oscillator or are global sound-shaping params.
        if (context.synthParamOscTarget >= 0 && context.synthParamOscTarget <= 3) {
            const std::string& n = def.name;
            const char targetChar = static_cast<char>('a' + context.synthParamOscTarget);
            const std::string targetPrefix = std::string("osc_") + targetChar;
            const std::string targetPrefixUpper = std::string("OSC ") + static_cast<char>('A' + context.synthParamOscTarget);
            bool belongsToTarget = false;
            // Direct per-osc parameters (e.g., osc_a_level, osc_a_detune_cents)
            if (n.rfind(targetPrefix, 0) == 0) {
                belongsToTarget = true;
            }
            // Global params that affect the selected oscillator on the Sound page
            else if (context.synthParamPage == 0) {
                if (n == "oscillator_mix" && context.synthParamOscTarget <= 1) belongsToTarget = true;
                if (n == "oscillator_c_mix" && context.synthParamOscTarget == 2) belongsToTarget = true;
                if (n == "oscillator_d_mix" && context.synthParamOscTarget == 3) belongsToTarget = true;
                if (n == "detune_cents" && context.synthParamOscTarget == 1) belongsToTarget = true;
                if (n == "detune_c_cents" && context.synthParamOscTarget == 2) belongsToTarget = true;
                if (n == "detune_d_cents" && context.synthParamOscTarget == 3) belongsToTarget = true;
                if (n == "pulse_width") belongsToTarget = true;
                if (n == "pwm_depth") belongsToTarget = true;
                if (n == "drive") belongsToTarget = true;
            }
            // Global params on other pages are still shown
            else if (context.synthParamPage != 0) {
                belongsToTarget = true;
            }
            // Always show unison/stereo/pan/gain regardless of target
            if (n.find("unison") != std::string::npos || n == "stereo_spread" || n == "pan" || n == "gain") {
                belongsToTarget = true;
            }
            // Always show sub/noise on Sound page
            if (context.synthParamPage == 0 && (n.find("sub") != std::string::npos || n.find("noise") != std::string::npos)) {
                belongsToTarget = true;
            }
            if (!belongsToTarget) {
                continue;
            }
        }
        visibleDefs.push_back(&def);
    }
    if (visibleDefs.empty()) {
        for (const SynthParamDef& def : context.synthParamDefs) {
            visibleDefs.push_back(&def);
        }
    }
    const int controlsBottom = context.synthWindowHeight - 28;
    const int columns = context.synthWindowWidth >= 1180 ? 3 : 2;
    const int rowHeight = 62;
    const int viewportY = targetY + 24;
    const int viewportHeight = std::max(120, controlsBottom - viewportY - 6);
    const int viewportX = 16;
    const int viewportWidth = context.synthWindowWidth - 32;
    context.synthParamViewport = UiRect {viewportX, viewportY, viewportWidth, viewportHeight};
    const int scrollbarW = 8;
    const int gapToScrollbar = 8;
    const int contentWidth = viewportWidth - scrollbarW - gapToScrollbar;
    const int columnWidth = std::max(280, contentWidth / columns);
    const int rows = static_cast<int>((visibleDefs.size() + columns - 1) / columns);
    context.synthParamContentHeight = std::max(viewportHeight, rows * rowHeight + 6);
    const int maxParamScroll = std::max(0, context.synthParamContentHeight - viewportHeight);
    context.synthParamScroll = std::clamp(context.synthParamScroll, 0, maxParamScroll);

    context.drawFilledRect(viewportX, viewportY, viewportWidth, viewportHeight, context.colors.background);
    context.drawRect(viewportX, viewportY, viewportWidth, viewportHeight, context.colors.gridLine);
    context.setClipRect(viewportX + 1, viewportY + 1, std::max(1, contentWidth - 2), std::max(1, viewportHeight - 2));

    for (int index = 0; index < static_cast<int>(visibleDefs.size()); ++index) {
        const SynthParamDef& def = *visibleDefs[static_cast<std::size_t>(index)];
        const std::string mappedName = mapSynthParameterToOscTarget(def.name, context.synthParamOscTarget);
        const bool mapped = mappedName != def.name;
        const int col = index % columns;
        const int row = index / columns;
        const int x = viewportX + 6 + (col * columnWidth);
        const int y = viewportY + 4 + (row * rowHeight) - context.synthParamScroll;
        if (y < viewportY - rowHeight || y > viewportY + viewportHeight) {
            continue;
        }
        const double value = getSynthParameterValue(context.patch, mappedName);
        std::ostringstream valueText;
        valueText.setf(std::ios::fixed);
        valueText.precision(def.step >= 1.0 ? 0 : 2);
        valueText << value;
        std::string valueLabel = valueText.str();
        if (def.name == "fm_algorithm") {
            const int mode = std::clamp(static_cast<int>(std::lround(value)), 0, 3);
            static const std::array<const char*, 4> names {"CAS", "SPL", "PAR", "MET"};
            valueLabel = names[static_cast<std::size_t>(mode)];
        } else if (def.name == "filter_mode") {
            const int mode = std::clamp(static_cast<int>(std::lround(value)), 0, 2);
            static const std::array<const char*, 3> names {"LDR", "4PL", "MS"};
            valueLabel = names[static_cast<std::size_t>(mode)];
        }
        const UiRect cellRect {x, y, columnWidth - 10, rowHeight - 6};
        context.drawFilledRect(cellRect.x, cellRect.y, cellRect.width, cellRect.height, context.colors.panel);
        context.drawRect(cellRect.x, cellRect.y, cellRect.width, cellRect.height, context.colors.gridLine);
        const UiRect knobRect {x + 8, y + 8, 34, 34};
        const double ratio = std::clamp(
            (value - def.minimum) / std::max(0.0001, def.maximum - def.minimum),
            0.0,
            1.0);
        context.drawKnob(knobRect, ratio, mapped);
        const std::string knobLabel = mapped ? (def.label + " [" + std::string(1, static_cast<char>('A' + context.synthParamOscTarget)) + "]") : def.label;
        const UiRect labelRect {x + 50, y + 6, std::max(80, columnWidth - 130), 16};
        const bool labelHovered = context.synthTooltipParam == mappedName;
        context.drawText(labelRect.x, labelRect.y + 10, context.fitText(knobLabel, labelRect.width), labelHovered ? context.colors.buttonActive : context.colors.text);
        context.drawText(x + 50, y + 34, valueLabel, context.colors.mutedText);
        const UiRect minusRect {x + 50, y + 38, 22, 18};
        const UiRect plusRect {x + 76, y + 38, 22, 18};
        context.drawButton(minusRect, "-", false);
        context.drawButton(plusRect, "+", false);
        context.synthWindowHits.push_back({labelRect, "param_label", mappedName, "", "", 0.0});
        context.synthWindowHits.push_back({minusRect, "param_delta", mappedName, "", "", -def.step});
        context.synthWindowHits.push_back({plusRect, "param_delta", mappedName, "", "", def.step});
        context.synthWindowHits.push_back({knobRect, "param_knob", mappedName, "", "", 0.0});
    }
    context.clearClip();
    if (maxParamScroll > 0) {
        const int sbX = viewportX + viewportWidth - scrollbarW - 2;
        const int sbY = viewportY + 2;
        const int sbH = viewportHeight - 4;
        context.drawFilledRect(sbX, sbY, scrollbarW, sbH, context.colors.gridLine);
        const double visibleRatio = static_cast<double>(viewportHeight) / static_cast<double>(context.synthParamContentHeight);
        const int thumbH = std::max(14, static_cast<int>(std::lround(sbH * visibleRatio)));
        const int thumbTravel = std::max(0, sbH - thumbH);
        const double scrollRatio = static_cast<double>(context.synthParamScroll) / static_cast<double>(std::max(1, maxParamScroll));
        const int thumbY = sbY + static_cast<int>(std::lround(scrollRatio * thumbTravel));
        context.drawFilledRect(sbX, thumbY, scrollbarW, thumbH, context.colors.buttonActive);
    }

    if (!context.synthTooltipParam.empty()) {
        bool stillOnLabel = false;
        for (const SynthWindowHit& hit : context.synthWindowHits) {
            if (hit.kind == "param_label"
                && hit.parameter == context.synthTooltipParam
                && hit.rect.contains(context.synthPointerX, context.synthPointerY)) {
                stillOnLabel = true;
                break;
            }
        }
        if (!stillOnLabel) {
            context.synthTooltipParam.clear();
        }
    }
    if (!context.synthTooltipParam.empty() && context.synthTooltipHoverSince != std::chrono::steady_clock::time_point {}) {
        const auto hoverMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - context.synthTooltipHoverSince);
        if (hoverMs.count() >= 520) {
            const std::string tooltip = synthParamTooltipText(context.synthTooltipParam);
            std::istringstream words(tooltip);
            std::vector<std::string> lines;
            std::string current;
            std::string word;
            const int maxTextW = 420;
            while (words >> word) {
                const std::string next = current.empty() ? word : (current + " " + word);
                if (context.textWidth(next) > maxTextW && !current.empty()) {
                    lines.push_back(current);
                    current = word;
                } else {
                    current = next;
                }
            }
            if (!current.empty()) {
                lines.push_back(current);
            }
            if (lines.empty()) {
                lines.push_back(tooltip);
            }
            int longest = 0;
            for (const std::string& line : lines) {
                longest = std::max(longest, context.textWidth(line));
            }
            const int pad = 7;
            const int tooltipW = longest + (pad * 2);
            const int tooltipH = static_cast<int>(lines.size()) * 16 + (pad * 2);
            const int tx = std::clamp(context.synthPointerX + 14, 10, context.synthWindowWidth - tooltipW - 10);
            const int ty = std::clamp(context.synthPointerY + 16, 10, context.synthWindowHeight - tooltipH - 10);
            context.drawFilledRect(tx, ty, tooltipW, tooltipH, context.colors.panel);
            context.drawRect(tx, ty, tooltipW, tooltipH, context.colors.buttonActive);
            for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
                context.drawText(
                    tx + pad,
                    ty + pad + 12 + static_cast<int>(lineIndex) * 16,
                    lines[lineIndex],
                    context.colors.text);
            }
        }
    }
}

} // namespace arachno
