#include "ui/gui/GuiMidiInput.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace arachno {

GuiMidiInput::~GuiMidiInput() {
    close();
}

bool GuiMidiInput::open() {
#if ARACHNO_HAS_ALSA
    close();
    snd_seq_t* seq = nullptr;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0 || seq == nullptr) {
        status_ = "MIDI IN: unavailable";
        retryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        return false;
    }
    snd_seq_set_client_name(seq, "ArachnoTracker Patch MIDI In");
    const int port = snd_seq_create_simple_port(
        seq,
        "Patch Designer MIDI In",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        snd_seq_close(seq);
        status_ = "MIDI IN: unavailable";
        retryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        return false;
    }
    seq_ = seq;
    port_ = port;
    connectionCount_ = 0;

    bool envConnected = false;
    if (const char* env = std::getenv("ARACHNO_MIDI_INPUT"); env != nullptr && *env != '\0') {
        int sourceClient = -1;
        int sourcePort = -1;
        if (parseAddress(env, sourceClient, sourcePort) && connectFrom(sourceClient, sourcePort)) {
            envConnected = true;
        }
    }
    if (!envConnected) {
        (void)autoConnectInputs();
    }
    if (connectionCount_ > 0) {
        status_ = "MIDI IN: active (" + std::to_string(connectionCount_) + " source)";
    } else {
        status_ = "MIDI IN: ready (connect keyboard or set ARACHNO_MIDI_INPUT=client:port)";
    }
    retryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    return true;
#else
    status_ = "MIDI IN: unavailable";
    retryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    return false;
#endif
}

void GuiMidiInput::close() {
#if ARACHNO_HAS_ALSA
    if (seq_ != nullptr) {
        if (port_ >= 0) {
            (void)snd_seq_delete_simple_port(seq_, port_);
        }
        snd_seq_close(seq_);
    }
    seq_ = nullptr;
    port_ = -1;
    connectionCount_ = 0;
#endif
    status_ = "MIDI IN: off";
}

bool GuiMidiInput::poll(std::vector<GuiMidiEvent>& outEvents) {
    bool changed = false;
    const auto now = std::chrono::steady_clock::now();
#if ARACHNO_HAS_ALSA
    if (seq_ == nullptr || port_ < 0) {
        if (now >= retryAt_ && open()) {
            changed = true;
        }
        return changed;
    }
    if (connectionCount_ <= 0 && now >= retryAt_) {
        const int reconnected = autoConnectInputs();
        if (reconnected > 0) {
            status_ = "MIDI IN: active (" + std::to_string(connectionCount_) + " source)";
            changed = true;
        }
        retryAt_ = now + std::chrono::seconds(2);
    }

    while (true) {
        snd_seq_event_t* midiEvent = nullptr;
        const int received = snd_seq_event_input(seq_, &midiEvent);
        if (received < 0 || midiEvent == nullptr) {
            break;
        }
        switch (midiEvent->type) {
            case SND_SEQ_EVENT_NOTEON: {
                const int midiNote = std::clamp(static_cast<int>(midiEvent->data.note.note), 0, 127);
                const int velocityInt = std::clamp(static_cast<int>(midiEvent->data.note.velocity), 0, 127);
                if (velocityInt <= 0) {
                    outEvents.push_back({GuiMidiEvent::Type::NoteOff, midiNote, 0.0f});
                } else {
                    const float velocity = std::clamp(static_cast<float>(velocityInt) / 127.0f, 0.02f, 1.0f);
                    outEvents.push_back({GuiMidiEvent::Type::NoteOn, midiNote, velocity});
                }
                changed = true;
                break;
            }
            case SND_SEQ_EVENT_NOTEOFF: {
                const int midiNote = std::clamp(static_cast<int>(midiEvent->data.note.note), 0, 127);
                outEvents.push_back({GuiMidiEvent::Type::NoteOff, midiNote, 0.0f});
                changed = true;
                break;
            }
            default:
                break;
        }
    }
#else
    (void)outEvents;
    (void)now;
#endif
    return changed;
}

const std::string& GuiMidiInput::status() const {
    return status_;
}

int GuiMidiInput::connectionCount() const {
#if ARACHNO_HAS_ALSA
    return connectionCount_;
#else
    return 0;
#endif
}

#if ARACHNO_HAS_ALSA
bool GuiMidiInput::connectFrom(int client, int port) {
    if (seq_ == nullptr || port_ < 0) {
        return false;
    }
    if (client == snd_seq_client_id(seq_)) {
        return false;
    }
    const int result = snd_seq_connect_from(seq_, port_, client, port);
    if (result < 0) {
        return false;
    }
    ++connectionCount_;
    return true;
}

bool GuiMidiInput::parseAddress(std::string text, int& client, int& port) const {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size()) {
        return false;
    }
    int parsedClient = -1;
    int parsedPort = -1;
    {
        std::istringstream in(text.substr(0, colon));
        in >> parsedClient;
        if (!in || !in.eof()) {
            return false;
        }
    }
    {
        std::istringstream in(text.substr(colon + 1));
        in >> parsedPort;
        if (!in || !in.eof()) {
            return false;
        }
    }
    if (parsedClient < 0 || parsedPort < 0) {
        return false;
    }
    client = parsedClient;
    port = parsedPort;
    return true;
}

int GuiMidiInput::autoConnectInputs() {
    if (seq_ == nullptr || port_ < 0) {
        return 0;
    }
    snd_seq_client_info_t* clientInfo = nullptr;
    snd_seq_port_info_t* portInfo = nullptr;
    snd_seq_client_info_alloca(&clientInfo);
    snd_seq_port_info_alloca(&portInfo);
    if (clientInfo == nullptr || portInfo == nullptr) {
        return 0;
    }
    const int selfClient = snd_seq_client_id(seq_);
    int connected = 0;
    snd_seq_client_info_set_client(clientInfo, -1);
    while (snd_seq_query_next_client(seq_, clientInfo) >= 0) {
        const int clientId = snd_seq_client_info_get_client(clientInfo);
        if (clientId < 0 || clientId == selfClient) {
            continue;
        }
        snd_seq_port_info_set_client(portInfo, clientId);
        snd_seq_port_info_set_port(portInfo, -1);
        while (snd_seq_query_next_port(seq_, portInfo) >= 0) {
            const unsigned int caps = snd_seq_port_info_get_capability(portInfo);
            if ((caps & SND_SEQ_PORT_CAP_READ) == 0) {
                continue;
            }
            const int portId = snd_seq_port_info_get_port(portInfo);
            if (connectFrom(clientId, portId)) {
                ++connected;
            }
        }
    }
    return connected;
}
#endif

} // namespace arachno

