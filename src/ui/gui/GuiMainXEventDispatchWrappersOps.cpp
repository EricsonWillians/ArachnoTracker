#include "ui/gui/GuiMainXEventDispatchWrappersOps.h"

namespace arachno {

GuiMainButtonPressDispatchResult dispatchMainButtonXEvent(
    const GuiMainButtonXEventDispatchContext& context,
    const XButtonEvent& event) {
    const GuiMainButtonPressContextFactoryInput buttonFactoryInput = context.makeButtonFactoryInput();
    return dispatchMainButtonPress(
        GuiMainButtonPressDispatchContext {
            static_cast<int>(event.button),
            event.x,
            event.y,
            static_cast<unsigned int>(event.state),
            context.sidebarViewport,
            context.verticalSplitterX,
            context.horizontalSplitterY,
            context.pointerX,
            context.pointerY,
            context.resizingSidebar,
            context.resizingTopPanel,
            context.playbackSampleRate,
            [&](int playbackSampleRate, int button, int mx, int my, unsigned int stateMask) {
                const GuiMainModalMouseContext modalContext = context.makeModalMouseContext(playbackSampleRate);
                return context.runModalMouse(modalContext, button, mx, my, stateMask);
            },
            [&](int button, int mx, int my, unsigned int stateMask) {
                const GuiMainWheelContext wheelContext = context.makeWheelContext(button, mx, my, stateMask);
                return context.runWheel(wheelContext);
            },
            [&](int mx, int my, int playbackSampleRate) {
                return context.dispatchPrimaryLeft(
                    makeMainPrimaryClickContextForButtonPress(
                        buttonFactoryInput,
                        mx,
                        my,
                        playbackSampleRate));
            },
            [&](bool pointerInSidebar, int mx, int my) {
                return context.dispatchSidebarLeft(
                    makeMainSidebarClickContextForButtonPress(
                        buttonFactoryInput,
                        pointerInSidebar,
                        mx,
                        my));
            },
            [&](int button, int mx, int my, bool altDown, bool shiftDown) {
                const GuiMainGridClickContext gridClickContext =
                    context.makeGridClickContext(button, mx, my, altDown, shiftDown);
                return context.runGridClick(gridClickContext);
            }});
}

GuiMainKeyPressDispatchResult dispatchMainKeyXEvent(
    const GuiMainKeyXEventDispatchContext& context,
    const XKeyEvent& event) {
    return dispatchMainKeyPress(
        GuiMainKeyPressDispatchContext {
            event,
            context.playbackSampleRate,
            [&](KeySym key,
                bool ctrlDown,
                bool shiftDown,
                bool altDown,
                int lookupCount,
                const char* lookupBuffer,
                const std::function<bool(KeySym)>& keyMatches,
                const std::function<int()>& resolvedDigit,
                int playbackSampleRate) {
                const GuiMainKeyModalContext modalContext = context.makeModalContext(
                    key,
                    ctrlDown,
                    shiftDown,
                    altDown,
                    lookupCount,
                    lookupBuffer,
                    keyMatches,
                    resolvedDigit,
                    playbackSampleRate);
                return context.runModal(modalContext);
            },
            [&](KeySym key,
                unsigned int keycode,
                bool ctrlDown,
                bool shiftDown,
                bool altDown,
                const std::function<bool(KeySym)>& keyMatches,
                const std::function<int()>& resolvedDigit) {
                const GuiMainKeyNoteContext noteContext =
                    context.makeNoteContext(key, keycode, ctrlDown, shiftDown, altDown, keyMatches, resolvedDigit);
                return context.runNote(noteContext);
            },
            [&](KeySym key,
                bool ctrlDown,
                bool shiftDown,
                bool altDown,
                const std::function<bool(KeySym)>& keyMatches) {
                const GuiMainKeyCommandContext commandContext =
                    context.makeCommandContext(key, ctrlDown, shiftDown, altDown, keyMatches);
                return context.runCommand(commandContext);
            },
            [&](KeySym key,
                bool ctrlDown,
                bool shiftDown,
                bool altDown,
                const std::function<bool(KeySym)>& keyMatches,
                const std::function<int()>& resolvedDigit) {
                const GuiMainKeyEditContext editContext =
                    context.makeEditContext(key, ctrlDown, shiftDown, altDown, keyMatches, resolvedDigit);
                return context.runEdit(editContext);
            }});
}

} // namespace arachno
