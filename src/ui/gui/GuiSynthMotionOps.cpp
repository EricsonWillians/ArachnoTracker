#include "ui/gui/GuiSynthMotionOps.h"

#include <algorithm>
#include <cmath>

#include <X11/X.h>

namespace arachno {

GuiSynthMotionResult handleSynthWindowMotion(
    const GuiSynthMotionContext& context,
    int x,
    int y,
    unsigned int stateMask) {
    GuiSynthMotionResult result;
    context.synthPointerX = x;
    context.synthPointerY = y;

    std::string hoveredParam;
    for (auto hitIt = context.synthWindowHits.rbegin(); hitIt != context.synthWindowHits.rend(); ++hitIt) {
        if (hitIt->kind == "param_label" && hitIt->rect.contains(context.synthPointerX, context.synthPointerY)) {
            hoveredParam = hitIt->parameter;
            break;
        }
    }
    if (hoveredParam != context.synthTooltipParam) {
        context.synthTooltipParam = hoveredParam;
        context.synthTooltipHoverSince = context.synthTooltipParam.empty()
            ? std::chrono::steady_clock::time_point {}
            : std::chrono::steady_clock::now();
        result.synthWindowNeedsRedraw = true;
        result.needsRedraw = true;
    }

    if (context.inlinePromptActive) {
        return result;
    }

    if (context.synthParamDragActive) {
        const int instrument = context.clampInstrumentIndex();
        if (instrument >= 0) {
            const SynthParamDef* def = context.findSynthParamDef(context.synthParamDragName);
            if (def != nullptr) {
                double value = context.synthParamDragStartValue;
                if (context.synthParamDragKnob) {
                    const double range = std::max(0.0001, def->maximum - def->minimum);
                    const double deltaY = static_cast<double>(context.synthParamDragStartY - y);
                    const double deltaX = static_cast<double>(x - context.synthParamDragStartX);
                    const double primaryDelta = deltaY + (deltaX * 0.35);
                    const bool fineAdjust = (stateMask & ShiftMask) != 0;
                    const double stepUnit = def->step > 0.0
                        ? def->step
                        : (range / 200.0);
                    const double pixelsPerStep = fineAdjust ? 8.0 : 2.0;
                    const double unitPerPixel = stepUnit / std::max(1.0, pixelsPerStep);
                    value = context.synthParamDragStartValue + (primaryDelta * unitPerPixel);
                } else {
                    const double ratio = static_cast<double>(x - context.synthParamDragRect.x)
                        / static_cast<double>(std::max(1, context.synthParamDragRect.width));
                    value = def->minimum + (std::clamp(ratio, 0.0, 1.0) * (def->maximum - def->minimum));
                }
                value = clampQuantizedSynthParamValue(*def, value);
                if (!std::isfinite(context.synthParamDragLastValue)
                    || std::abs(value - context.synthParamDragLastValue) > 0.000001) {
                    if (context.setSynthParameter(instrument, context.synthParamDragName, value, false)) {
                        context.synthParamDragLastValue = value;
                        context.synthParamDragDirty = true;
                        result.needsRedraw = true;
                        result.synthWindowNeedsRedraw = true;
                    }
                }
            }
        }
    }

    if (context.synthPointerDown && (stateMask & Button1Mask) != 0) {
        if (context.triggerSynthKeyboardPointer(x, y, false)) {
            result.needsRedraw = true;
            result.synthWindowNeedsRedraw = true;
        }
    }
    return result;
}

} // namespace arachno
