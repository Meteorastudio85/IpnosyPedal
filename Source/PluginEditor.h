/*
    Ipnosy - Meteora Plugins
    Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Slider invisibile: gestisce solo il mouse, il disegno lo fa l'editor.
class InvisibleKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override {}
};

class FootswitchArea : public juce::Component
{
public:
    explicit FootswitchArea (juce::RangedAudioParameter& p) : param (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool hitTest (int x, int y) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return juce::Point<float> ((float) x, (float) y).getDistanceFrom (c) <= (float) getWidth() * 0.5f;
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        pressed = true;
        param.beginChangeGesture();
        param.setValueNotifyingHost (param.getValue() > 0.5f ? 0.0f : 1.0f);
        param.endChangeGesture();
        if (onChange) onChange();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        pressed = false;
        if (onChange) onChange();
    }

    bool pressed = false;
    std::function<void()> onChange;

private:
    juce::RangedAudioParameter& param;
};

class IpnosyEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit IpnosyEditor (IpnosyProcessor&);
    ~IpnosyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider& s, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
                    const juce::String& paramId, double defaultValue);
    void prepareLeaf();
    void renderLeaf();
    void drawValuePopup (juce::Graphics& g, juce::Slider& s, float cx, float cy, float r);

    IpnosyProcessor& proc;

    // immagini originali (coordinate della foto 1396 x 1572)
    juce::Image bg, logo, blendCap, ledOn, tubeFil, tubeFull, leaf;
    // immagini ridimensionate alla finestra
    juce::Image sBg, sLogo, sBlend, sLed, sTubeFil, sTubeFull, sLeaf, leafOut;

    // dati della foglia per l'arcobaleno
    std::vector<float> leafS, leafV, leafA, leafH;

    InvisibleKnobLookAndFeel knobLaf;
    juce::Slider gainKnob, blendKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt, blendAtt;
    FootswitchArea footswitch;

    juce::Random rng;
    float filGlow = 0.0f, fullGlow = 0.0f, flicker = 0.0f;
    float leafAmt = 0.0f, leafPhase = 0.0f;

    float drawnGain = -1.0f, drawnBlend = -1.0f, drawnFil = -1.0f, drawnFull = -1.0f, drawnLeaf = -1.0f;
    bool drawnOn = false, drawnPressed = false, drawnHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IpnosyEditor)
};
