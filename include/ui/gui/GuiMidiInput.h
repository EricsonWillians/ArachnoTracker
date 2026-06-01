#pragma once

#include <array>
#include <chrono>
#include <string>
#include <vector>

#ifndef ARACHNO_HAS_ALSA
#define ARACHNO_HAS_ALSA 0
#endif

#if ARACHNO_HAS_ALSA
#include <alsa/asoundlib.h>
#endif

namespace arachno {

struct GuiMidiEvent {
    enum class Type {
        NoteOn,
        NoteOff
    };
    Type type = Type::NoteOff;
    int midiNote = 60;
    float velocity = 1.0f;
};

class GuiMidiInput {
public:
    GuiMidiInput() = default;
    ~GuiMidiInput();

    GuiMidiInput(const GuiMidiInput&) = delete;
    GuiMidiInput& operator=(const GuiMidiInput&) = delete;

    bool open();
    void close();
    bool poll(std::vector<GuiMidiEvent>& outEvents);
    const std::string& status() const;
    int connectionCount() const;

private:
#if ARACHNO_HAS_ALSA
    bool connectFrom(int client, int port);
    bool parseAddress(std::string text, int& client, int& port) const;
    int autoConnectInputs();
#endif

#if ARACHNO_HAS_ALSA
    snd_seq_t* seq_ = nullptr;
    int port_ = -1;
    int connectionCount_ = 0;
#endif
    std::string status_ = "MIDI IN: off";
    std::chrono::steady_clock::time_point retryAt_ {};
};

} // namespace arachno

