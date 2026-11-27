#include "PluginEditor.h"

namespace
{
    constexpr float W = 1280.0f, H = 720.0f;
    const Rectangle<float> canvas (250.0f, 170.0f, 780.0f, 330.0f);   // the picture/visualizer window

    ColourGradient chrome (float x, float y, float h)
    {
        ColourGradient g (Colour (0xfff2f2f2), x, y, Colour (0xff2b2b2b), x, y + h, false);
        g.addColour (0.45, Colour (0xff9c9c9c));
        g.addColour (0.55, Colour (0xff555555));
        return g;
    }

    void drawCrown (Graphics& g, float x0, float y0, float w, float h, Colour gem)
    {
        const float xs[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        const float ys[5] = { 0.25f, 0.10f, 0.0f, 0.10f, 0.25f };
        Path p;
        p.startNewSubPath (x0, y0 + h);
        for (int i = 0; i < 5; ++i)
        {
            p.lineTo (x0 + w * xs[i], y0 + h * ys[i]);
            if (i < 4) p.lineTo (x0 + w * (xs[i] + 0.125f), y0 + h * 0.55f);
        }
        p.lineTo (x0 + w, y0 + h);
        p.closeSubPath();

        g.setGradientFill (chrome (y0, y0, h));
        g.fillPath (p);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (p, PathStrokeType (1.5f));
        g.setColour (gem);
        for (int i = 0; i < 5; ++i)
            g.fillEllipse (x0 + w * xs[i] - 4.0f, y0 + h * ys[i] - 2.0f, 8.0f, 8.0f);
    }

    bool isBlackKey (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
}

CBEditor::CBEditor (CBProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < numCols; ++i) resetCol (i, true);

    skinFolder = File::getSpecialLocation (File::userDocumentsDirectory)
                    .getChildFile ("ChicagoBeatsVisualizer").getChildFile ("skins");
    skinFolder.createDirectory();
    loadSkinList();
    if (skins.size() > 0) stepSkin (1);

    constrainer.setFixedAspectRatio (16.0 / 9.0);
    constrainer.setSizeLimits (640, 360, 1920, 1080);
    setConstrainer (&constrainer);
    setResizable (true, true);                       // bottom-right drag handle

    const int w = roundToInt (W * jlimit (0.5f, 1.5f, proc.uiScale.load()));
    setSize (w, roundToInt (w * 9.0f / 16.0f));
    startTimerHz (30);
}

void CBEditor::resized()
{
    scale = (float) getWidth() / W;
    proc.uiScale = scale;
}

void CBEditor::stepSize (float factor)           // the in-plugin  -  /  +  buttons
{
    const int w = jlimit (640, 1920, roundToInt (getWidth() * factor));
    setSize (w, roundToInt (w * 9.0f / 16.0f));
}

void CBEditor::resetCol (int i, bool scatter)
{
    cols[i].len   = 6 + rng.nextInt (12);
    cols[i].speed = 3.0f + rng.nextFloat() * 6.0f;
    cols[i].y     = scatter ? rng.nextFloat() * canvas.getHeight()
                            : -rng.nextFloat() * 120.0f;
}

// ---------------------------------------------------------------- skins
void CBEditor::loadSkinList()
{
    skins.clear();
    skinFolder.findChildFiles (skins, File::findFiles, false, "*.png;*.jpg;*.jpeg;*.gif");
}

void CBEditor::stepSkin (int dir)
{
    loadSkinList();
    if (skins.isEmpty()) { skin = Image(); skinIndex = -1; return; }
    skinIndex = (skinIndex + dir + skins.size()) % skins.size();
    skin = ImageFileFormat::loadFrom (skins[skinIndex]);
}

void CBEditor::randomize()
{
    static const uint32 palette[] = { 0xffff1a1a, 0xffffc233, 0xff2d7bff, 0xff19e08a, 0xffb04bff, 0xffffffff };
    accent = Colour (palette[rng.nextInt (6)]);
    showMatrix = rng.nextBool();
    showStars  = rng.nextBool();
    if (! showMatrix && ! showStars) showMatrix = true;
    loadSkinList();
    if (skins.size() > 1) stepSkin (1 + rng.nextInt (skins.size() - 1));
}

void CBEditor::mouseDown (const MouseEvent& e)
{
    const auto pt = e.position / scale;
    if      (btnOn.contains (pt))    *proc.vfxOn = true;
    else if (btnOff.contains (pt))   *proc.vfxOn = false;
    else if (btnPrev.contains (pt))  stepSkin (-1);
    else if (btnNext.contains (pt))  stepSkin (1);
    else if (btnDice.contains (pt))  randomize();
    else if (btnMinus.contains (pt)) stepSize (0.9f);
    else if (btnPlus.contains (pt))  stepSize (1.1f);
}

// ---------------------------------------------------------------- audio analysis + motion
void CBEditor::analyse()
{
    proc.copyLatestSamples (fftData.data(), CBProcessor::fftSize);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) CBProcessor::fftSize);
    std::fill (fftData.begin() + CBProcessor::fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double binHz = proc.sampleRate.load() / (double) CBProcessor::fftSize;
    for (int b = 0; b < numBands; ++b)
    {
        const double f0 = 30.0 * std::pow (16000.0 / 30.0, b / (double) numBands);
        const double f1 = 30.0 * std::pow (16000.0 / 30.0, (b + 1) / (double) numBands);
        const int lo = jmax (1, (int) std::floor (f0 / binHz));
        const int hi = jmin (CBProcessor::fftSize / 2 - 1, jmax (lo + 1, (int) std::ceil (f1 / binHz)));
        float m = 0.0f;
        for (int k = lo; k <= hi; ++k) m = jmax (m, fftData[(size_t) k]);
        const float db = Decibels::gainToDecibels (m / (CBProcessor::fftSize * 0.25f), -90.0f);
        const float level = jlimit (0.0f, 1.0f, jmap (db, -70.0f, 0.0f, 0.0f, 1.0f));
        bands[b] = jmax (level, bands[b] * 0.85f);
    }

    float lo = 0, hi = 0;
    for (int i = 0; i < 12; ++i)  lo += bands[i];
    for (int i = 48; i < 64; ++i) hi += bands[i];
    bass   = jlimit (0.0f, 1.0f, std::pow (lo / 12.0f, 1.5f) * 1.6f);
    treble = jlimit (0.0f, 1.0f, hi / 16.0f);
    bassEnv = jmax (bass, bassEnv * 0.90f);

    // spring: kick slams the cone out, it overshoots and rebounds
    const float accel = (bass - coneX) * 0.55f - coneV * 0.22f;
    coneV += accel;
    coneX = jlimit (-0.35f, 1.25f, coneX + coneV);

    dispL = jmax (proc.peakL.exchange (0.0f), dispL * 0.88f);
    dispR = jmax (proc.peakR.exchange (0.0f), dispR * 0.88f);

    for (int i = 0; i < 128; ++i)
        keyDisp[i] = jmax (proc.keyVel[i].load(), keyDisp[i] * 0.90f);
}

void CBEditor::timerCallback()
{
    analyse();

    if (*proc.vfxOn)
    {
        if (showMatrix)
            for (int i = 0; i < numCols; ++i)
            {
                cols[i].y += cols[i].speed * (1.0f + treble * 2.0f);
                if (cols[i].y - cols[i].len * 14.0f > canvas.getHeight()) resetCol (i, false);
            }

        if (showStars)
        {
            if (rng.nextFloat() < 0.03f + bassEnv * 0.25f)
            {
                Star s;
                s.p = { canvas.getX() + rng.nextFloat() * canvas.getWidth() * 0.7f, canvas.getY() };
                s.v = { 8.0f + rng.nextFloat() * 8.0f, 3.0f + rng.nextFloat() * 4.0f };
                stars.add (s);
            }
            for (int i = stars.size(); --i >= 0;)
            {
                auto& s = stars.getReference (i);
                s.p += s.v;
                s.life -= 0.025f;
                if (s.life <= 0.0f || ! canvas.expanded (40.0f).contains (s.p)) stars.remove (i);
            }
        }
    }
    repaint();
}

// ---------------------------------------------------------------- painting
void CBEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale (scale));
    const bool vfx = *proc.vfxOn;

    g.fillAll (Colours::black);
    g.setGradientFill (ColourGradient (accent.withAlpha (0.30f), W * 0.5f, H * 0.5f,
                                       Colours::black, 0.0f, 0.0f, true));
    g.fillRect (0.0f, 0.0f, W, H);

    drawCanvas (g, vfx);
    drawFrame (g);
    drawPanels (g);
    drawTransport (g);
    drawKeyboard (g);

    // towers last so they overlap the frame edges, tilted outward
    const float shake = vfx ? bassEnv * 3.0f : 0.0f;
    const float exc   = vfx ? coneX : 0.0f;
    drawTower (g, { 118.0f, 250.0f },  180.0f, 366.0f, -7.0f, exc, shake);
    drawTower (g, { 1162.0f, 250.0f }, 180.0f, 366.0f,  7.0f, exc, shake);
}

void CBEditor::drawCanvas (Graphics& g, bool vfx)
{
    Path clip;
    clip.addRoundedRectangle (canvas, 18.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    // picture / skin (cover-fit). Without a skin, a dark gradient is shown.
    if (skin.isValid())
    {
        const float ir = (float) skin.getWidth() / (float) skin.getHeight();
        const float cr = canvas.getWidth() / canvas.getHeight();
        int sw = skin.getWidth(), sh = skin.getHeight();
        if (ir > cr) sw = roundToInt (sh * cr); else sh = roundToInt (sw / cr);
        const int sx = (skin.getWidth() - sw) / 2, sy = (skin.getHeight() - sh) / 2;
        g.drawImage (skin, (int) canvas.getX(), (int) canvas.getY(), (int) canvas.getWidth(), (int) canvas.getHeight(),
                     sx, sy, sw, sh);
    }
    else
    {
        g.setGradientFill (ColourGradient (accent.withAlpha (0.45f), canvas.getCentreX(), canvas.getBottom(),
                                           Colour (0xff050505), canvas.getCentreX(), canvas.getY(), false));
        g.fillRect (canvas);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.setFont (Font (18.0f, Font::bold));
        g.drawText ("Drop .png / .jpg / .gif skins into Documents\\ChicagoBeatsVisualizer\\skins",
                    canvas.toNearestInt(), Justification::centred);
    }

    if (vfx)
    {
        g.setColour (accent.withAlpha (bassEnv * 0.18f));       // bass pulse wash
        g.fillRect (canvas);

        if (showMatrix)
        {
            g.setFont (Font (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain));
            static const String chars ("01ABCDEF<>/|=+*");
            for (int i = 0; i < numCols; ++i)
            {
                const float x = canvas.getX() + i * 14.0f;
                for (int k = 0; k < cols[i].len; ++k)
                {
                    const float y = canvas.getY() + cols[i].y - k * 14.0f;
                    if (y < canvas.getY() - 14.0f || y > canvas.getBottom()) continue;
                    const float a = 1.0f - (float) k / (float) cols[i].len;
                    g.setColour ((k == 0 ? Colours::white : accent).withAlpha (a * 0.8f));
                    g.drawSingleLineText (String::charToString (chars[rng.nextInt (chars.length())]),
                                          (int) x, (int) y);
                }
            }
        }

        if (showStars)
            for (auto& s : stars)
            {
                const auto tail = s.p - s.v * 8.0f;
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (s.life), s.p.x, s.p.y,
                                                   Colours::transparentWhite, tail.x, tail.y, false));
                g.drawLine (s.p.x, s.p.y, tail.x, tail.y, 2.0f);
                g.setColour (Colours::white.withAlpha (s.life));
                g.fillEllipse (s.p.x - 2.0f, s.p.y - 2.0f, 4.0f, 4.0f);
            }
    }

    // spectrum bars along the bottom of the canvas (always live)
    const float bw = canvas.getWidth() / numBands;
    for (int b = 0; b < numBands; ++b)
    {
        const float h = bands[b] * canvas.getHeight() * 0.45f;
        const float x = canvas.getX() + b * bw;
        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (x + 1.0f, canvas.getBottom() - h, bw - 2.0f, h);
        g.setColour (Colours::white);
        g.fillRect (x + 1.0f, canvas.getBottom() - h, bw - 2.0f, 2.0f);
    }
    g.restoreState();
}

void CBEditor::drawFrame (Graphics& g)
{
    const auto outer = canvas.expanded (14.0f);
    const float glow = 0.35f + 0.65f * bassEnv;
    for (int i = 4; i >= 1; --i)                                // soft red glow
    {
        g.setColour (accent.withAlpha (glow * 0.08f * (float) (5 - i)));
        g.drawRoundedRectangle (outer.expanded ((float) i * 2.0f), 26.0f, 3.0f);
    }
    g.setColour (Colour (0xff050505));
    g.drawRoundedRectangle (outer, 24.0f, 12.0f);
    g.setColour (Colour (0xffd8d8d8));
    g.drawRoundedRectangle (outer.expanded (6.0f), 30.0f, 1.4f);
    g.drawRoundedRectangle (outer.reduced (6.0f), 18.0f, 1.0f);
    g.setColour (accent.withAlpha (glow));
    g.drawRoundedRectangle (canvas.expanded (3.0f), 20.0f, 2.0f);

    // title plate + crown
    const Rectangle<float> plate (368.0f, 78.0f, 543.0f, 76.0f);
    pinstripe (g, plate, 14.0f);

    g.setFont (Font ("Georgia", 46.0f, Font::bold | Font::italic));
    g.setColour (Colours::white);
    g.drawText ("Chicago", Rectangle<int> (384, 78, 250, 56), Justification::centredRight);
    g.setColour (accent);
    g.drawText (" Beats", Rectangle<int> (634, 78, 260, 56), Justification::centredLeft);
    g.setFont (Font (15.0f, Font::bold));
    g.setColour (Colours::white);
    g.drawText ("VISUALIZER", Rectangle<int> (368, 130, 543, 22), Justification::centred);

    drawCrown (g, 560.0f, 6.0f, 160.0f, 68.0f, accent);
}

void CBEditor::pinstripe (Graphics& g, Rectangle<float> r, float radius)
{
    g.setColour (Colour (0xff050505));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colour (0xffd8d8d8));
    g.drawRoundedRectangle (r.reduced (1.0f), radius, 1.4f);
    g.setColour (Colour (0xffd8d8d8).withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (6.0f), radius * 0.6f, 1.0f);
}

// One woofer: thin silver rings, glowing LED ring, and a cone made of fine
// concentric grooves. exc (0..1.25) pushes the cone toward the viewer: the cap
// grows and rises, the grooves spread, the LED ring flares.
void CBEditor::drawWoofer (Graphics& g, Point<float> c, float r, float exc)
{
    const float led = 0.25f + 0.75f * bassEnv;
    for (int i = 3; i >= 1; --i)
    {
        const float rr = r + 4.0f + i * 2.5f;
        g.setColour (accent.withAlpha (led * 0.10f * (float) (4 - i)));
        g.drawEllipse (c.x - rr, c.y - rr, 2.0f * rr, 2.0f * rr, 2.0f);
    }
    g.setColour (accent.withAlpha (led));
    g.drawEllipse (c.x - r - 4.0f, c.y - r - 4.0f, 2.0f * (r + 4.0f), 2.0f * (r + 4.0f), 1.8f);

    g.setColour (Colour (0xffd8d8d8));
    g.drawEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r, 1.4f);
    g.setColour (Colour (0xffd8d8d8).withAlpha (0.6f));
    g.drawEllipse (c.x - r * 0.93f, c.y - r * 0.93f, r * 1.86f, r * 1.86f, 1.0f);

    const float R = r * 0.86f;
    g.setColour (Colour (0xff020202));
    g.fillEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);

    const float capR = R * 0.26f * (1.0f + 0.25f * exc);
    const Point<float> capC (c.x + R * 0.02f, c.y - R * 0.22f * (1.0f + 0.5f * exc));

    const int N = 26;
    for (int i = 0; i < N; ++i)
    {
        const float t     = (float) i / (float) (N - 1);                  // 0 outer .. 1 inner
        const float rad   = jmap (t, R, capR * 1.15f) * (1.0f + 0.04f * exc * t);
        const float shift = std::pow (t, 1.4f);
        const float cx = c.x + (capC.x - c.x) * shift;
        const float cy = c.y + (capC.y - c.y) * shift;
        const float a  = 0.30f + 0.60f * (1.0f - t * 0.6f);
        g.setGradientFill (ColourGradient (Colour (0xff777777).withAlpha (a * 0.5f), cx, cy - rad,
                                           Colour (0xffeeeeee).withAlpha (a), cx, cy + rad, false));
        g.drawEllipse (cx - rad, cy - rad, 2.0f * rad, 2.0f * rad, 0.9f);
    }

    g.setColour (Colours::black);                                          // dust cap
    g.fillEllipse (capC.x - capR, capC.y - capR, 2.0f * capR, 2.0f * capR);
    Path arc;
    arc.addCentredArc (capC.x, capC.y, capR, capR, 0.0f, -2.4f, -0.7f, true);
    g.setColour (Colours::white.withAlpha (0.65f));
    g.strokePath (arc, PathStrokeType (1.3f));
}

// Tall two-woofer tower (black cabinet, silver pinstripes), tilted outward.
void CBEditor::drawTower (Graphics& g, Point<float> c, float w, float h, float tiltDeg, float exc, float shake)
{
    c += { (rng.nextFloat() - 0.5f) * shake, (rng.nextFloat() - 0.5f) * shake };

    g.saveState();
    g.addTransform (AffineTransform::rotation (degreesToRadians (tiltDeg), c.x, c.y));

    const Rectangle<float> cab (c.x - w * 0.5f, c.y - h * 0.5f, w, h);
    const float led = 0.25f + 0.75f * bassEnv;
    for (int i = 4; i >= 1; --i)                                           // glow behind the cabinet
    {
        g.setColour (accent.withAlpha (led * 0.05f * (float) (5 - i)));
        g.drawRoundedRectangle (cab.expanded ((float) i * 2.5f), 16.0f + (float) i * 2.0f, 3.0f);
    }
    pinstripe (g, cab, 14.0f);

    const float r = w * 0.40f;
    drawWoofer (g, { c.x, cab.getY() + h * 0.27f }, r, exc);
    drawWoofer (g, { c.x, cab.getY() + h * 0.70f }, r, exc);

    const Rectangle<float> plate (c.x - 34.0f, cab.getBottom() - 30.0f, 68.0f, 14.0f);
    g.setColour (Colour (0xff0a0a0a));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (Colour (0xffd8d8d8));
    g.drawRoundedRectangle (plate, 3.0f, 1.0f);
    g.setFont (Font (8.0f, Font::bold));
    g.drawText ("CHICAGO BEATS", plate, Justification::centred);

    g.restoreState();
}

void CBEditor::drawButton (Graphics& g, Rectangle<float> r, const String& t, bool on)
{
    g.setColour (on ? accent : Colour (0xff141414));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? Colours::white : accent.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);
    g.setFont (Font (14.0f, Font::bold));
    g.drawText (t, r, Justification::centred);
}

void CBEditor::drawPanels (Graphics& g)
{
    const Rectangle<float> left (10, 452, 162, 224), right (1108, 452, 162, 224);
    pinstripe (g, left, 10.0f);
    pinstripe (g, right, 10.0f);

    auto label = [&] (const String& t, Rectangle<float> r)
    {
        g.setColour (Colours::white);
        g.setFont (Font (13.0f, Font::bold));
        g.drawText (t, r, Justification::centred);
    };

    // left: Motion VFX / dB meter / spectrum
    const bool vfx = *proc.vfxOn;
    label ("MOTION VFX", { 10, 455, 162, 16 });
    drawButton (g, btnOn, "ON", vfx);
    drawButton (g, btnOff, "OFF", ! vfx);

    label ("DB METER", { 10, 507, 162, 14 });
    const float meters[4] = { dispL, dispR, bass, treble };
    for (int m = 0; m < 4; ++m)
        for (int s = 0; s < 12; ++s)
        {
            const bool lit = jlimit (0.0f, 1.0f, meters[m]) * 12.0f > (float) s;
            g.setColour (lit ? (s >= 9 ? Colours::white : accent) : Colour (0xff1c1c1c));
            g.fillRect (22.0f + m * 34.0f, 574.0f - s * 4.2f, 26.0f, 3.2f);
        }

    label ("SPECTRUM", { 10, 580, 162, 14 });
    for (int b = 0; b < 16; ++b)
    {
        float v = 0; for (int k = 0; k < 4; ++k) v = jmax (v, bands[b * 4 + k]);
        const float h = v * 52.0f;
        g.setColour (Colours::white);
        g.fillRect (20.0f + b * 8.8f, 670.0f - h, 6.0f, h);
    }

    // right: skin mode / randomizer / resize
    label ("SKIN MODE", { 1108, 455, 162, 16 });
    const Rectangle<float> thumb (1122, 473, 134, 40);
    g.setColour (Colour (0xff151515));
    g.fillRoundedRectangle (thumb, 4.0f);
    if (skin.isValid())
        g.drawImage (skin, (int) thumb.getX(), (int) thumb.getY(), (int) thumb.getWidth(), (int) thumb.getHeight(),
                     0, 0, skin.getWidth(), skin.getHeight());
    drawButton (g, btnPrev, "<", false);
    drawButton (g, btnNext, ">", false);

    label ("RANDOMIZER", { 1108, 547, 162, 14 });
    g.setColour (Colour (0xfff0f0f0));
    g.fillRoundedRectangle (btnDice, 8.0f);
    g.setColour (accent);
    for (auto d : { Point<float> (.25f, .25f), Point<float> (.75f, .25f), Point<float> (.5f, .5f),
                    Point<float> (.25f, .75f), Point<float> (.75f, .75f) })
        g.fillEllipse (btnDice.getX() + d.x * 44.0f - 4.0f, btnDice.getY() + d.y * 44.0f - 4.0f, 8.0f, 8.0f);

    label ("RESIZE INTERFACE", { 1108, 612, 162, 14 });
    drawButton (g, btnMinus, "-", false);
    drawButton (g, btnPlus, "+", false);
}

void CBEditor::drawTransport (Graphics& g)
{
    // decorative for now: the media transport arrives with the video/gif player milestone
    const Rectangle<float> bar (270, 528, 740, 46);
    pinstripe (g, bar, 10.0f);
    g.setColour (Colours::white);
    g.setFont (Font (17.0f, Font::bold));
    g.drawText ("Chicago Beats", Rectangle<int> (300, 531, 260, 22), Justification::centredLeft);
    g.setFont (Font (13.0f));
    g.drawText ("Now Playing...", Rectangle<int> (300, 551, 260, 18), Justification::centredLeft);

    g.setColour (accent);
    g.fillEllipse (616.0f, 527.0f, 48.0f, 48.0f);
    Path play; play.addTriangle (632.0f, 540.0f, 632.0f, 562.0f, 652.0f, 551.0f);
    g.setColour (Colours::white); g.fillPath (play);
    Path back, fwd;
    back.addTriangle (600.0f, 551.0f, 612.0f, 543.0f, 612.0f, 559.0f);
    fwd.addTriangle (680.0f, 543.0f, 680.0f, 559.0f, 692.0f, 551.0f);
    g.setColour (accent); g.fillPath (back); g.fillPath (fwd);
}

void CBEditor::drawKeyboard (Graphics& g)
{
    const float x0 = 180.0f, y0 = 590.0f, kw = 920.0f / 36.0f, kh = 86.0f;
    int wi = 0;
    for (int n = 36; n <= 96; ++n)
        if (! isBlackKey (n))
        {
            const float v = keyDisp[n];
            const Rectangle<float> k (x0 + wi * kw, y0, kw - 1.0f, kh);
            g.setColour (Colours::white.interpolatedWith (accent, jlimit (0.0f, 1.0f, v)));
            g.fillRect (k);
            if (v > 0.05f)
            {
                g.setColour (accent.withAlpha (v * 0.6f));
                g.fillRect (k.getX(), k.getY() - 10.0f * v, k.getWidth(), 10.0f * v);
            }
            ++wi;
        }

    wi = 0;
    for (int n = 36; n <= 96; ++n)
        if (! isBlackKey (n)) ++wi;
        else
        {
            const float v = keyDisp[n];
            const Rectangle<float> k (x0 + wi * kw - kw * 0.3f, y0, kw * 0.6f, kh * 0.58f);
            g.setColour (Colour (0xff111111).interpolatedWith (accent, jlimit (0.0f, 1.0f, v)));
            g.fillRect (k);
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawRect (k, 1.0f);
        }
}
