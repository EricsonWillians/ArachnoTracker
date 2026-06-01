#include "ui/gui/GuiSynthInteractionOps.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

#include "PatchIO.h"
#include "ui/gui/GuiPathDefaults.h"

namespace arachno {

namespace {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

bool triggerSynthKeyboardPointer(
    const std::vector<PianoKeyHit>& synthKeyboardHits,
    int mx,
    int my,
    bool allowRetrigger,
    int& synthLastPointerMidi,
    const std::function<void(int)>& auditionSynthPreviewMidi) {
    for (auto keyIt = synthKeyboardHits.rbegin(); keyIt != synthKeyboardHits.rend(); ++keyIt) {
        if (!keyIt->rect.contains(mx, my)) {
            continue;
        }
        if (allowRetrigger || keyIt->midiNote != synthLastPointerMidi) {
            synthLastPointerMidi = keyIt->midiNote;
            auditionSynthPreviewMidi(keyIt->midiNote);
        }
        return true;
    }
    return false;
}

bool handleSynthWindowClick(const GuiSynthClickContext& context) {
    for (auto hitIt = context.synthWindowHits.rbegin(); hitIt != context.synthWindowHits.rend(); ++hitIt) {
        const SynthWindowHit& hit = *hitIt;
        if (!hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        const int instrument = context.clampInstrumentIndex();
        if (hit.kind == "window_close") {
            context.setSynthWindowVisible(false);
        } else if (hit.kind == "inst_prev") {
            if (instrument >= 0) {
                context.selectInstrument(instrument - 1);
            }
        } else if (hit.kind == "inst_next") {
            if (instrument >= 0) {
                context.selectInstrument(instrument + 1);
            }
        } else if (hit.kind == "inst_new") {
            AppActionRequest request;
            request.actionId = "editor.instrument.new";
            request.parameters = {{"name", "NewPatch"}};
            context.runAction(request);
            context.selectInstrument(static_cast<int>(context.session.song().instruments.size()) - 1);
        } else if (hit.kind == "inst_clone") {
            if (instrument >= 0) {
                AppActionRequest request;
                request.actionId = "editor.instrument.clone";
                request.parameters = {{"source_index", std::to_string(instrument)}};
                context.runAction(request);
                context.selectInstrument(static_cast<int>(context.session.song().instruments.size()) - 1);
            }
        } else if (hit.kind == "inst_rename") {
            if (instrument >= 0) {
                context.beginInlinePrompt(
                    InlinePromptKind::RenameInstrument,
                    "Rename instrument",
                    "New patch name",
                    context.session.song().instruments[static_cast<std::size_t>(instrument)].patch.name,
                    -1,
                    instrument);
            }
        } else if (hit.kind == "inst_aud") {
            context.auditionArmedInstrument();
        } else if (hit.kind == "patch_import_new") {
            const AppSessionSnapshot snap = context.activeSnapshot();
            context.beginInlinePrompt(
                InlinePromptKind::ImportPatchAsNewPath,
                "Import patch as new instrument",
                "Path to .arachnopatch",
                defaultPatchPath(
                    snap.hasProjectPath,
                    snap.projectPath,
                    patchStemFromName("imported_patch"),
                    std::filesystem::current_path()),
                -1,
                -1);
        } else if (hit.kind == "patch_import_replace") {
            if (instrument >= 0) {
                const AppSessionSnapshot snap = context.activeSnapshot();
                context.beginInlinePrompt(
                    InlinePromptKind::ImportPatchReplacePath,
                    "Load patch into selected instrument",
                    "Path to .arachnopatch",
                    defaultPatchPath(
                        snap.hasProjectPath,
                        snap.projectPath,
                        context.session.song().instruments[static_cast<std::size_t>(instrument)].patch.name,
                        std::filesystem::current_path()),
                    -1,
                    instrument);
            }
        } else if (hit.kind == "patch_import_replace_all") {
            const AppSessionSnapshot snap = context.activeSnapshot();
            context.beginInlinePrompt(
                InlinePromptKind::ImportPatchReplaceAllPath,
                "Load patches into current instruments",
                "File: apply to all | Folder: map sorted .arachnopatch files by instrument index",
                defaultPatchBulkPath(
                    snap.hasProjectPath,
                    snap.projectPath,
                    std::filesystem::current_path()),
                -1,
                -2);
        } else if (hit.kind == "patch_export") {
            if (instrument >= 0) {
                const AppSessionSnapshot snap = context.activeSnapshot();
                context.beginInlinePrompt(
                    InlinePromptKind::ExportPatchPath,
                    "Export selected instrument patch",
                    "Output .arachnopatch path",
                    defaultPatchPath(
                        snap.hasProjectPath,
                        snap.projectPath,
                        context.session.song().instruments[static_cast<std::size_t>(instrument)].patch.name,
                        std::filesystem::current_path()),
                    -1,
                    instrument);
            }
        } else if (hit.kind == "kb_octave_down") {
            context.synthKeyboardBaseOctave = std::max(0, context.synthKeyboardBaseOctave - 1);
        } else if (hit.kind == "kb_octave_up") {
            context.synthKeyboardBaseOctave = std::min(
                std::max(0, 10 - context.synthKeyboardVisibleOctaves),
                context.synthKeyboardBaseOctave + 1);
        } else if (hit.kind == "kb_octave_sync") {
            context.synthKeyboardBaseOctave = std::clamp(
                context.armedOctave - 1,
                0,
                std::max(0, 10 - context.synthKeyboardVisibleOctaves));
        } else if (hit.kind == "param_page") {
            context.synthParamPage = std::clamp(static_cast<int>(std::lround(hit.value)), 0, 3);
            context.synthParamScroll = 0;
        } else if (hit.kind == "osc_target") {
            context.synthParamOscTarget = std::clamp(static_cast<int>(std::lround(hit.value)), -1, 3);
        } else if (hit.kind == "wave") {
            if (instrument >= 0) {
                if (context.setSynthWaveform(instrument, hit.oscillator, hit.wave)) {
                    const SynthPatch& updatedPatch = context.session.song().instruments[static_cast<std::size_t>(instrument)].patch;
                    int oscillatorIndex = 0;
                    const std::string osc = lowerCopy(hit.oscillator);
                    if (osc == "b") oscillatorIndex = 1;
                    else if (osc == "c") oscillatorIndex = 2;
                    else if (osc == "d") oscillatorIndex = 3;
                    context.auditionSynthOscillatorPreview(updatedPatch, oscillatorIndex, context.synthPreviewMidi);
                }
            }
        } else if (hit.kind == "key_note") {
            if (instrument >= 0) {
                const int midi = std::clamp(static_cast<int>(std::lround(hit.value)), 0, 127);
                context.synthLastPointerMidi = midi;
                context.auditionSynthPreviewMidi(midi);
            }
        } else if (hit.kind == "param_set") {
            if (instrument >= 0) {
                const SynthParamDef* def = context.findSynthParamDef(hit.parameter);
                if (def != nullptr) {
                    const double value = clampQuantizedSynthParamValue(*def, hit.value);
                    if (context.setSynthParameter(instrument, hit.parameter, value)) {
                        const bool enabling = value >= 0.5;
                        if (enabling) {
                            const SynthPatch& patch = context.session.song().instruments[static_cast<std::size_t>(instrument)].patch;
                            if (hit.parameter == "osc_b_enabled" && patch.oscillatorMix < 0.08) {
                                (void)context.setSynthParameter(instrument, "oscillator_mix", 0.32);
                            } else if (hit.parameter == "osc_c_enabled") {
                                if (patch.oscillatorCMix < 0.08) {
                                    (void)context.setSynthParameter(instrument, "oscillator_c_mix", 0.28);
                                }
                                if (patch.oscCLevel < 0.1) {
                                    (void)context.setSynthParameter(instrument, "osc_c_level", 1.0);
                                }
                            } else if (hit.parameter == "osc_d_enabled") {
                                if (patch.oscillatorDMix < 0.08) {
                                    (void)context.setSynthParameter(instrument, "oscillator_d_mix", 0.24);
                                }
                                if (patch.oscDLevel < 0.1) {
                                    (void)context.setSynthParameter(instrument, "osc_d_level", 1.0);
                                }
                            }
                        }
                    }
                }
            }
        } else if (hit.kind == "param_delta") {
            if (instrument >= 0) {
                const SynthParamDef* def = context.findSynthParamDef(hit.parameter);
                if (def != nullptr) {
                    const SynthPatch& patch = context.session.song().instruments[static_cast<std::size_t>(instrument)].patch;
                    const double value = clampQuantizedSynthParamValue(
                        *def,
                        context.getSynthParameterValue(patch, hit.parameter) + hit.delta);
                    context.setSynthParameter(instrument, hit.parameter, value);
                }
            }
        } else if (hit.kind == "param_knob") {
            if (instrument >= 0) {
                const SynthParamDef* def = context.findSynthParamDef(hit.parameter);
                if (def != nullptr) {
                    const SynthPatch& patch = context.session.song().instruments[static_cast<std::size_t>(instrument)].patch;
                    context.synthParamDragActive = true;
                    context.synthParamDragKnob = true;
                    context.synthParamDragName = hit.parameter;
                    context.synthParamDragRect = hit.rect;
                    context.synthParamDragStartX = context.mx;
                    context.synthParamDragStartY = context.my;
                    context.synthParamDragStartValue = context.getSynthParameterValue(patch, hit.parameter);
                    context.synthParamDragLastValue = context.synthParamDragStartValue;
                    context.synthParamDragDirty = false;
                }
            }
        } else if (hit.kind == "param_slider") {
            if (instrument >= 0) {
                const SynthParamDef* def = context.findSynthParamDef(hit.parameter);
                if (def != nullptr) {
                    const double ratio = static_cast<double>(context.mx - hit.rect.x)
                        / static_cast<double>(std::max(1, hit.rect.width));
                    const double value = clampQuantizedSynthParamValue(
                        *def,
                        def->minimum + (std::clamp(ratio, 0.0, 1.0) * (def->maximum - def->minimum)));
                    context.setSynthParameter(instrument, hit.parameter, value);
                    context.synthParamDragActive = true;
                    context.synthParamDragKnob = false;
                    context.synthParamDragName = hit.parameter;
                    context.synthParamDragRect = hit.rect;
                    context.synthParamDragStartX = context.mx;
                    context.synthParamDragStartY = context.my;
                    context.synthParamDragStartValue = value;
                    context.synthParamDragLastValue = value;
                    context.synthParamDragDirty = true;
                }
            }
        }
        return true;
    }
    return false;
}

} // namespace arachno
