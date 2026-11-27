#pragma once
#include "PluginProcessor.h"
#include <array>

class CBEditor : public AudioProcessorEditor, private Timer
{
public:
    explicit CBEditor (CBProcessor&);
    ~CBEditor() override { stopTimer(); }

    void paint (Graphics&) override;
    void resized() override;
    void mouseDown (const MouseEvent&) override;

private:
    enum { numBands = 64, numCols = 80 };

    struct Col  { float y = 0, speed = 5; int len = 10; };
    struct Star { Point<float> p, v; float life = 1; };
    struct Dust { Point<float> p, v; float life = 1, r = 6; };

    void timerCallback() override;
    void analyse();
    void resetCol (int i, bool scatter);
    void layout();                       // positions the screen, controls and keyboard

    void loadSkinList();
    void stepSkin (int dir);
    void randomize();
    void stepSize (float factor);        // whole plugin window
    void stepScreen (float delta);       // visualizer screen only

    // drawing (fixed 1280x720 design space, scaled at paint time)
    void drawCanvas (Graphics&, bool vfx);
    void drawFrame (Graphics&);
    void drawTower (Graphics&, Point<float> c, float w, float h, float exc, float shake);
    void drawWoofer (Graphics&, Point<float> c, float r, float exc);
    void pinstripe (Graphics&, Rectangle<float>, float radius);
    void drawPanels (Graphics&);
    void drawCar (Graphics&);
    void drawKeyboard (Graphics&);
    void drawButton (Graphics&, Rectangle<float>, const String&, bool on);
    float carScale() const { return 0.95f * canvas.getWidth() / 880.0f; }

    CBProcessor& proc;
    ComponentBoundsConstrainer constrainer;
    float scale = 1.0f;

    dsp::FFT fft { CBProcessor::fftOrder };
    dsp::WindowingFunction<float> window { (size_t) CBProcessor::fftSize, dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * CBProcessor::fftSize> fftData {};

    float bands[numBands] = {};
    float dispL = 0, dispR = 0, bass = 0, bassEnv = 0, treble = 0;
    float coneX = 0, coneV = 0;          // spring-driven subwoofer cone
    float keyDisp[128] = {};

    Col cols[numCols];
    Array<Star> stars;
    Array<Dust> dust;
    float wheelAngle = 0, phase = 0, carLift = 0, lastLift = 0;
    Random rng;

    File skinFolder;
    Array<File> skins;
    int skinIndex = -1;
    Image skin;

    Colour accent { 0xffff1a1a };
    bool showMatrix = true, showStars = true, showCar = true, showWarp = true;
    int fitMode = 0;                     // 0 = FIT (whole picture), 1 = FILL (crop), 2 = STRETCH (exact)

    // layout (design space), filled by layout()
    float screenScale = 1.0f;
    Rectangle<float> canvas, strip, rMeter, rSpec, grp[7];
    float stripY = 0, kbY = 0, kbH = 0;
    Rectangle<float> btnOn, btnOff, btnPrev, btnNext, btnDice, btnMinus, btnPlus, btnScaleMinus, btnScalePlus;
    Rectangle<float> chipMatrix, chipStars, chipCar, chipWarp, chipFit;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CBEditor)
};
