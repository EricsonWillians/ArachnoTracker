#include "ui/gui/GuiMainPrimaryClickOps.h"

#include <algorithm>

namespace arachno {

GuiMainPrimaryClickResult handleMainPrimaryLeftClick(const GuiMainPrimaryClickContext& context) {
    GuiMainPrimaryClickResult result;
    bool consumed = false;

    for (const auto& entry : context.transportButtons) {
        if (!entry.first.contains(context.mx, context.my)) {
            continue;
        }
        context.runTransportAction(entry.second);
        consumed = true;
        break;
    }
    if (!consumed) {
        for (const auto& entry : context.fileButtons) {
            if (!entry.first.contains(context.mx, context.my)) {
                continue;
            }
            context.runFileButtonAction(entry.second);
            consumed = true;
            break;
        }
    }
    if (!consumed) {
        for (const auto& entry : context.themeButtons) {
            if (!entry.first.contains(context.mx, context.my)) {
                continue;
            }
            context.setThemeMode(entry.second);
            consumed = true;
            break;
        }
    }
    if (!consumed) {
        for (const auto& entry : context.audioPerformanceButtons) {
            if (!entry.first.contains(context.mx, context.my)) {
                continue;
            }
            context.setAudioPerformanceMode(entry.second, context.playbackSampleRate);
            consumed = true;
            break;
        }
    }
    if (!consumed) {
        if (context.gridTrackPrevButton.contains(context.mx, context.my)
            || context.gridTrackNextButton.contains(context.mx, context.my)) {
            const int direction = context.gridTrackPrevButton.contains(context.mx, context.my) ? -1 : 1;
            context.shiftGridTrackWindow(direction);
            consumed = true;
        }
    }
    if (!consumed) {
        if (context.patternNewButton.contains(context.mx, context.my)) {
            context.beginPatternCreatePrompt();
            consumed = true;
        } else if (context.patternCloneButton.contains(context.mx, context.my)) {
            context.beginPatternClonePrompt();
            consumed = true;
        } else if (context.patternDeleteButton.contains(context.mx, context.my)) {
            context.deleteActivePattern();
            consumed = true;
        }
    }
    if (!consumed) {
        if (context.orderInsertButton.contains(context.mx, context.my)) {
            context.insertOrderAtSelection();
            consumed = true;
        } else if (context.orderAppendButton.contains(context.mx, context.my)) {
            context.appendOrderFromActivePattern();
            consumed = true;
        } else if (context.orderDeleteButton.contains(context.mx, context.my)) {
            context.removeSelectedOrder();
            consumed = true;
        }
    }
    if (!consumed) {
        if (context.patternPrevButton.contains(context.mx, context.my)
            || context.patternNextButton.contains(context.mx, context.my)) {
            context.cyclePattern(context.patternPrevButton.contains(context.mx, context.my) ? -1 : 1);
            consumed = true;
        }
    }
    if (!consumed) {
        if (context.orderPrevButton.contains(context.mx, context.my)
            || context.orderNextButton.contains(context.mx, context.my)) {
            context.cycleOrder(context.orderPrevButton.contains(context.mx, context.my) ? -1 : 1);
            consumed = true;
        }
    }
    if (!consumed) {
        for (const OrderSlotHit& slot : context.orderSlotHits) {
            if (!slot.rect.contains(context.mx, context.my)) {
                continue;
            }
            context.selectOrderSlot(std::max(0, slot.index), slot.valid);
            consumed = true;
            break;
        }
    }
    if (!consumed) {
        for (const TrackHeaderHit& hit : context.trackHeaderHits) {
            if (!hit.muteRect.contains(context.mx, context.my)
                && !hit.soloRect.contains(context.mx, context.my)
                && !hit.selectRect.contains(context.mx, context.my)) {
                continue;
            }
            consumed = context.handleTrackHeaderClick(hit, context.mx, context.my);
            if (consumed) {
                break;
            }
        }
    }

    result.consumed = consumed;
    result.needsRedraw = consumed;
    return result;
}

} // namespace arachno
