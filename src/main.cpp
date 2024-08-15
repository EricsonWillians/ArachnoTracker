#include <iostream>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>
#include <JuceHeader.h>

class SquareWaveSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

class SquareWaveVoice : public juce::SynthesiserVoice {
public:
    SquareWaveVoice() : currentAngle(0), angleDelta(0), level(0.5), tailOff(0) {}

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SquareWaveSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override {
        currentAngle = 0.0;
        level = velocity;
        tailOff = 0.0;

        double cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        double cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override {
        if (allowTailOff) {
            if (tailOff == 0.0) {
                tailOff = 1.0;
            }
        } else {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (angleDelta != 0.0) {
            if (tailOff > 0.0) {
                while (--numSamples >= 0) {
                    float currentSample = (float)(std::sin(currentAngle) > 0.0 ? level : -level);

                    for (int i = outputBuffer.getNumChannels(); --i >= 0;) {
                        outputBuffer.addSample(i, startSample, currentSample);
                    }

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99;

                    if (tailOff <= 0.005) {
                        clearCurrentNote();
                        angleDelta = 0.0;
                        break;
                    }
                }
            } else {
                while (--numSamples >= 0) {
                    float currentSample = (float)(std::sin(currentAngle) > 0.0 ? level : -level);

                    for (int i = outputBuffer.getNumChannels(); --i >= 0;) {
                        outputBuffer.addSample(i, startSample, currentSample);
                    }

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

private:
    double currentAngle, angleDelta, level, tailOff;
};

class MainComponent : public juce::AudioAppComponent {
public:
    MainComponent() {
        setSize(200, 200);
        synth.clearSounds();
        synth.clearVoices();

        synth.addSound(new SquareWaveSound());
        synth.addVoice(new SquareWaveVoice());

        setAudioChannels(0, 2); // No inputs, 2 outputs

        // Automatically start a middle C note
        synth.noteOn(1, 60, 0.8f);  // Channel 1, Note 60 (Middle C), Velocity 0.8
    }

    ~MainComponent() override {
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        spdlog::info("prepareToPlay called with sampleRate: {}", sampleRate);
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.clearActiveBufferRegion();
        juce::MidiBuffer emptyBuffer;
        synth.renderNextBlock(*bufferToFill.buffer, emptyBuffer, 0, bufferToFill.numSamples);
    }

    void releaseResources() override {}

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText("Square Wave Test", getLocalBounds(), juce::Justification::centred, true);
    }

    void resized() override {}

private:
    juce::Synthesiser synth;
};

class SquareWaveApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Square Wave Test"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise(const juce::String&) override {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name) : juce::DocumentWindow(name, juce::Colours::black, juce::DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    private:
        std::unique_ptr<MainComponent> mainComponent;
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SquareWaveApp)
