#include <iostream>
#include <fmt/core.h>
#include <JuceHeader.h>
#include "ReverbEffect.h"

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
        setSize(400, 300);
        synth.clearSounds();
        synth.clearVoices();

        synth.addSound(new SquareWaveSound());
        synth.addVoice(new SquareWaveVoice());

        setAudioChannels(0, 2); // No inputs, 2 outputs

        // Automatically start a middle C note
        synth.noteOn(1, 60, 0.8f);  // Channel 1, Note 60 (Middle C), Velocity 0.8

        // Configure the slider
        reverbSlider.setRange(0.0, 1.0, 0.01);
        reverbSlider.setValue(0.3); // Default value matching initial reverb settings
        reverbSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 20);
        reverbSlider.onValueChange = [this]() { 
            reverbEffect.setParameter("wetLevel", reverbSlider.getValue()); 
        };
        addAndMakeVisible(reverbSlider);

        reverbLabel.setText("Reverb Wet Level:", juce::dontSendNotification);
        addAndMakeVisible(reverbLabel);

        // Apply initial slider value to reverb effect
        reverbEffect.setParameter("wetLevel", reverbSlider.getValue());
    }

    ~MainComponent() override {
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        std::cout << "prepareToPlay called with sampleRate: " << sampleRate << std::endl;
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.clearActiveBufferRegion();
        juce::MidiBuffer emptyBuffer;
        synth.renderNextBlock(*bufferToFill.buffer, emptyBuffer, 0, bufferToFill.numSamples);

        // Apply the reverb effect to the buffer
        reverbEffect.process(*bufferToFill.buffer, bufferToFill.numSamples);
    }

    void releaseResources() override {}

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText("Square Wave with Reverb", getLocalBounds(), juce::Justification::centred, true);
    }

    void resized() override {
        reverbLabel.setBounds(10, 10, getWidth() - 20, 20);
        reverbSlider.setBounds(10, 40, getWidth() - 20, 20);
    }

private:
    juce::Synthesiser synth;
    ReverbEffect reverbEffect; // Add the reverb effect as a member variable

    juce::Slider reverbSlider; // Slider for controlling the reverb wet level
    juce::Label reverbLabel;   // Label for the slider

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
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
