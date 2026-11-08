/*
    Ipnosy - Meteora Plugins
    Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati.
*/

#include "PluginEditor.h"
#include "BinaryData.h"

#include <cmath>

//==============================================================================
// Geometria nella foto di riferimento (pixel, 1396 x 1572).
//==============================================================================
namespace Geo
{
    constexpr float refW = 1396.0f, refH = 1572.0f;

    // manopola centrale (Gain): il logo ruota
    constexpr float gainX = 705.0f, gainY = 907.0f, gainHit = 263.0f;
    constexpr float logoOX = 451.0f, logoOY = 653.0f;

    // Blend: nella foto la tacca e' a -144 gradi (tutto a sinistra)
    constexpr float blendX = 1095.0f, blendY = 1308.0f, blendHit = 100.0f;
    constexpr float blendOX = 1000.0f, blendOY = 1213.0f;
    constexpr float blendPhotoDeg = -144.0f;

    // LED
    constexpr float ledX = 306.0f, ledY = 1116.0f;
    constexpr float ledOX = 270.0f, ledOY = 1080.0f;

    // footswitch
    constexpr float fsX = 304.0f, fsY = 1298.0f, fsR = 95.0f;

    // valvola e foglia
    constexpr float tubeOX = 700.0f, tubeOY = 40.0f;
    constexpr float leafOX = 170.0f, leafOY = 465.0f;
    constexpr float leafHubX = 382.0f, leafHubY = 770.0f;

    constexpr int baseWidth = 540;
}

//==============================================================================
IpnosyEditor::IpnosyEditor (IpnosyProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      footswitch (*p.apvts.getParameter ("on"))
{
    bg       = juce::ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    logo     = juce::ImageCache::getFromMemory (BinaryData::logo_png,       BinaryData::logo_pngSize);
    blendCap = juce::ImageCache::getFromMemory (BinaryData::blend_png,      BinaryData::blend_pngSize);
    ledOn    = juce::ImageCache::getFromMemory (BinaryData::led_on_png,     BinaryData::led_on_pngSize);
    tubeFil  = juce::ImageCache::getFromMemory (BinaryData::tube_fil_png,   BinaryData::tube_fil_pngSize);
    tubeFull = juce::ImageCache::getFromMemory (BinaryData::tube_full_png,  BinaryData::tube_full_pngSize);
    leaf     = juce::ImageCache::getFromMemory (BinaryData::leaf_png,       BinaryData::leaf_pngSize);

    setOpaque (true);

    setupKnob (gainKnob,  gainAtt,  "gain",  0.5);
    setupKnob (blendKnob, blendAtt, "blend", 1.0);

    footswitch.onChange = [this] { repaint(); };
    addAndMakeVisible (footswitch);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) (Geo::refW / Geo::refH));
    setResizeLimits (360, (int) std::lround (360.0 * Geo::refH / Geo::refW),
                     1000, (int) std::lround (1000.0 * Geo::refH / Geo::refW));
    setSize (Geo::baseWidth, (int) std::lround (Geo::baseWidth * Geo::refH / Geo::refW));

    startTimerHz (30);
}

IpnosyEditor::~IpnosyEditor()
{
    stopTimer();
    gainKnob.setLookAndFeel (nullptr);
    blendKnob.setLookAndFeel (nullptr);
}

void IpnosyEditor::setupKnob (juce::Slider& s, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
                              const juce::String& paramId, double defaultValue)
{
    s.setLookAndFeel (&knobLaf);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (280);
    s.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    s.setDoubleClickReturnValue (true, defaultValue);
    s.onValueChange = [this] { repaint(); };
}

//==============================================================================
static juce::Image rescaleImage (const juce::Image& img, float s)
{
    if (! img.isValid())
        return {};
    return img.rescaled (juce::jmax (1, juce::roundToInt ((float) img.getWidth() * s)),
                         juce::jmax (1, juce::roundToInt ((float) img.getHeight() * s)),
                         juce::Graphics::highResamplingQuality);
}

void IpnosyEditor::resized()
{
    const float s = (float) getWidth() / Geo::refW;

    if (bg.isValid())
        sBg = bg.rescaled (getWidth(), getHeight(), juce::Graphics::highResamplingQuality);

    sLogo     = rescaleImage (logo, s);
    sBlend    = rescaleImage (blendCap, s);
    sLed      = rescaleImage (ledOn, s);
    sTubeFil  = rescaleImage (tubeFil, s);
    sTubeFull = rescaleImage (tubeFull, s);
    sLeaf     = rescaleImage (leaf, s);
    prepareLeaf();

    auto circle = [s] (float x, float y, float r)
    {
        return juce::Rectangle<float> (2.0f * r * s, 2.0f * r * s).withCentre ({ x * s, y * s }).toNearestInt();
    };

    gainKnob.setBounds  (circle (Geo::gainX,  Geo::gainY,  Geo::gainHit));
    blendKnob.setBounds (circle (Geo::blendX, Geo::blendY, Geo::blendHit));
    footswitch.setBounds (circle (Geo::fsX,   Geo::fsY,    Geo::fsR));

    repaint();
}

//==============================================================================
// Foglia arcobaleno: colori che scorrono attorno al centro della foglia,
// mantenendo luci e ombre del disegno originale.
void IpnosyEditor::prepareLeaf()
{
    leafS.clear(); leafV.clear(); leafA.clear(); leafH.clear();
    if (! sLeaf.isValid())
        return;

    const float s = (float) getWidth() / Geo::refW;
    const int w = sLeaf.getWidth(), h = sLeaf.getHeight();
    const float hubX = (Geo::leafHubX - Geo::leafOX) * s;
    const float hubY = (Geo::leafHubY - Geo::leafOY) * s;

    leafS.resize ((size_t) (w * h));
    leafV.resize ((size_t) (w * h));
    leafA.resize ((size_t) (w * h));
    leafH.resize ((size_t) (w * h));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const auto c = sLeaf.getPixelAt (x, y);
            const size_t i = (size_t) (y * w + x);
            leafA[i] = c.getFloatAlpha();
            leafV[i] = juce::jlimit (0.0f, 1.0f, c.getBrightness() * 1.25f + 0.06f);
            leafS[i] = juce::jlimit (0.0f, 1.0f, c.getSaturation() * 0.4f + 0.67f);
            const float ang = std::atan2 ((float) y - hubY, (float) x - hubX);
            leafH[i] = (ang + juce::MathConstants<float>::pi) / juce::MathConstants<float>::twoPi;
        }

    leafOut = juce::Image (juce::Image::ARGB, w, h, true);
}

void IpnosyEditor::renderLeaf()
{
    if (! leafOut.isValid() || leafA.empty())
        return;

    const int w = leafOut.getWidth(), h = leafOut.getHeight();
    juce::Image::BitmapData data (leafOut, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const size_t i = (size_t) (y * w + x);
            const float a = leafA[i] * leafAmt;
            if (a <= 0.002f)
            {
                data.setPixelColour (x, y, juce::Colours::transparentBlack);
                continue;
            }
            float hue = leafH[i] + leafPhase;
            hue -= std::floor (hue);
            data.setPixelColour (x, y, juce::Colour::fromHSV (hue, leafS[i], leafV[i], a));
        }
}

//==============================================================================
void IpnosyEditor::timerCallback()
{
    const float gain = proc.apvts.getRawParameterValue ("gain")->load();

    // --- valvola: segue il riscaldamento vero del suono (TubeEngine::warm)
    const float warm  = proc.getTubeWarm();
    const float drv   = juce::jlimit (0.0f, 1.0f, (gain - IpnosyProcessor::kPos1500) / (1.0f - IpnosyProcessor::kPos1500));
    const float sig   = juce::jlimit (0.0f, 1.0f, (proc.getTubeLevelDb() + 40.0f) / 30.0f);

    if (warm > 0.001f)
        flicker += ((rng.nextFloat() - 0.5f) * 0.10f - flicker) * 0.5f;
    else
        flicker = 0.0f;

    // prima si accende il filamento, poi tutta la valvola
    const float filTarget  = juce::jlimit (0.0f, 1.0f, warm * 1.7f);
    const float fullTarget = juce::jlimit (0.0f, 1.0f, (warm - 0.2f) / 0.8f)
                             * (0.55f + 0.45f * drv) * (0.85f + 0.15f * sig);
    filGlow  = juce::jlimit (0.0f, 1.0f, filTarget  * (1.0f + 0.6f * flicker));
    fullGlow = juce::jlimit (0.0f, 1.0f, fullTarget * (1.0f + flicker));

    // --- foglia arcobaleno al massimo
    const float leafTarget = gain >= 0.985f ? 1.0f : 0.0f;
    leafAmt += (leafTarget - leafAmt) * 0.15f;
    if (leafAmt < 0.003f) leafAmt = 0.0f;
    if (leafAmt > 0.0f)
    {
        leafPhase += 1.0f / 120.0f; // un giro di colori ogni 4 secondi
        leafPhase -= std::floor (leafPhase);
        renderLeaf();
    }

    const bool on = proc.apvts.getRawParameterValue ("on")->load() > 0.5f;
    const float blend = proc.apvts.getRawParameterValue ("blend")->load();
    const bool hover = gainKnob.isMouseButtonDown() || blendKnob.isMouseButtonDown();

    const bool changed = std::abs (gain - drawnGain) > 1.0e-4f || std::abs (blend - drawnBlend) > 1.0e-4f
                      || std::abs (filGlow - drawnFil) > 0.004f || std::abs (fullGlow - drawnFull) > 0.004f
                      || leafAmt > 0.0f || leafAmt != drawnLeaf
                      || on != drawnOn || footswitch.pressed != drawnPressed || hover != drawnHover;

    if (changed)
    {
        drawnGain = gain; drawnBlend = blend; drawnFil = filGlow; drawnFull = fullGlow; drawnLeaf = leafAmt;
        drawnOn = on; drawnPressed = footswitch.pressed; drawnHover = hover;
        repaint();
    }
}

//==============================================================================
void IpnosyEditor::drawValuePopup (juce::Graphics& g, juce::Slider& s, float cx, float cy, float r)
{
    if (! s.isMouseButtonDown())
        return;

    const auto text = s.getTextFromValue (s.getValue());
    const float w = juce::jmax (70.0f, r * 1.1f), h = juce::jmax (24.0f, r * 0.34f);
    const juce::Rectangle<float> box (cx - w * 0.5f, cy - h * 0.5f, w, h);
    g.setColour (juce::Colour (0xcc0a0812));
    g.fillRoundedRectangle (box, h * 0.3f);
    g.setColour (juce::Colour (0xffeee6ff));
    g.setFont (juce::FontOptions (h * 0.6f, juce::Font::bold));
    g.drawText (text, box, juce::Justification::centred);
}

void IpnosyEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / Geo::refW;

    if (sBg.isValid())
        g.drawImageAt (sBg, 0, 0);
    else
        g.fillAll (juce::Colours::black);

    const int tubeX = juce::roundToInt (Geo::tubeOX * s), tubeY = juce::roundToInt (Geo::tubeOY * s);

    // --- valvola
    if (filGlow > 0.004f && sTubeFil.isValid())
    {
        g.setOpacity (filGlow);
        g.drawImageAt (sTubeFil, tubeX, tubeY);
    }
    if (fullGlow > 0.004f && sTubeFull.isValid())
    {
        g.setOpacity (fullGlow);
        g.drawImageAt (sTubeFull, tubeX, tubeY);
    }
    g.setOpacity (1.0f);

    // --- foglia arcobaleno
    if (leafAmt > 0.0f && leafOut.isValid())
        g.drawImageAt (leafOut, juce::roundToInt (Geo::leafOX * s), juce::roundToInt (Geo::leafOY * s));

    // --- manopola centrale: ruota il logo (dritto a ore 12)
    if (sLogo.isValid())
    {
        const float norm = (float) gainKnob.valueToProportionOfLength (gainKnob.getValue());
        const float ang = juce::degreesToRadians ((norm - 0.5f) * 270.0f);
        const float ox = Geo::logoOX * s, oy = Geo::logoOY * s;
        const float pcx = (Geo::gainX - Geo::logoOX) * s, pcy = (Geo::gainY - Geo::logoOY) * s;
        g.drawImageTransformed (sLogo, juce::AffineTransform::rotation (ang, pcx, pcy).translated (ox, oy), false);
    }

    // --- Blend
    if (sBlend.isValid())
    {
        const float norm = (float) blendKnob.valueToProportionOfLength (blendKnob.getValue());
        const float ang = juce::degreesToRadians (norm * 270.0f - 135.0f - Geo::blendPhotoDeg);
        const float ox = Geo::blendOX * s, oy = Geo::blendOY * s;
        const float pcx = (Geo::blendX - Geo::blendOX) * s, pcy = (Geo::blendY - Geo::blendOY) * s;
        g.drawImageTransformed (sBlend, juce::AffineTransform::rotation (ang, pcx, pcy).translated (ox, oy), false);
    }

    // --- LED verde
    if (proc.apvts.getRawParameterValue ("on")->load() > 0.5f)
    {
        if (sLed.isValid())
            g.drawImageAt (sLed, juce::roundToInt (Geo::ledOX * s), juce::roundToInt (Geo::ledOY * s));

        const juce::Point<float> c (Geo::ledX * s, Geo::ledY * s);
        juce::ColourGradient bloom (juce::Colour (0x8840ff60), c,
                                    juce::Colour (0x0040ff60), c.translated (95.0f * s, 0.0f), true);
        g.setGradientFill (bloom);
        g.fillEllipse (juce::Rectangle<float> (190.0f * s, 190.0f * s).withCentre (c));
    }

    // --- footswitch premuto
    if (footswitch.pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (juce::Rectangle<float> (2.0f * 75.0f * s, 2.0f * 75.0f * s).withCentre ({ Geo::fsX * s, Geo::fsY * s }));
    }

    // --- valori mentre si gira
    drawValuePopup (g, gainKnob,  Geo::gainX * s,  Geo::gainY * s,  Geo::gainHit * s);
    drawValuePopup (g, blendKnob, Geo::blendX * s, (Geo::blendY - 150.0f) * s, Geo::blendHit * s);

    if (! proc.modelsLoaded())
    {
        g.setColour (juce::Colours::red.withAlpha (0.85f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Errore: modelli NAM non caricati", getLocalBounds().removeFromTop (24), juce::Justification::centred);
    }
}
