#pragma once
#include <JuceHeader.h>

// Audio pass-through effect. It never changes the sound: it only copies audio
// into a ring buffer and tracks peak levels so the editor can draw them.
class CBProcessor : public AudioProcessor
{
public:
    enum { fftOrder = 11, fftSize = 1 << 11, fifoSize = 8192 };

    CBProcessor();

    void prepareToPlay (double sr, int) override        { sampleRate = sr; }
    void releaseResources() override                    {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const String getName() const override               { return "Chicago Beats Visualizer"; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const String getProgramName (int) override          { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Newest n samples (mono) in time order. Called from the UI thread.
    void copyLatestSamples (float* dest, int n) const;

    AudioParameterBool* vfxOn = nullptr;      // the Motion VFX ON/OFF switch
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };   // max-hold, UI resets with exchange(0)
    std::atomic<float> keyVel[128];           // 0..1 note velocity (filled when MIDI arrives)
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<float> uiScale { 1.0f };      // saved with the project

private:
    float fifo[fifoSize] = {};
    std::atomic<int> writePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CBProcessor)
};
