/*
    Ipnosy - Meteora Plugins
    Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "NAM/dsp.h"
#include "Resampler.h"
#include "TubeEngine.h"

class IpnosyProcessor : public juce::AudioProcessor
{
public:
    // posizioni della manopola centrale (0..1 su 270 gradi)
    static constexpr float kPos0900 = 1.0f / 6.0f; // ore 9
    static constexpr float kPos1200 = 0.5f;        // ore 12
    static constexpr float kPos1500 = 5.0f / 6.0f; // ore 15: si accende la valvola
    static constexpr float kPosMax  = 1.0f;        // massimo

    IpnosyProcessor();
    ~IpnosyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    bool modelsLoaded() const { return numModelsLoaded == 3; }

    // per la grafica
    float getTubeWarm() const    { return tube.warm.load(); }
    float getTubeLevelDb() const { return tube.levelDb.load(); }

    static double tubeDriveFor (float gain);

private:
    static constexpr double kModelRate   = 48000.0;
    static constexpr int    kModelChunk  = 1024;
    static constexpr int    kFifoPrefill = 8;

    void loadModels();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n);
    void runModels (const float* in, float* out, int m);
    int  measureResamplerLatency (double hostRate) const;
    static void targetWeights (float gain, float w[3], float& levelGain);

    std::array<std::unique_ptr<nam::DSP>, 3> models;
    int numModelsLoaded = 0;

    bool resampling = false;
    int maxBlock = 512;
    int latency = 0;

    StreamResampler up, down;
    SampleFifo outFifo;
    TubeEngine tube;

    std::vector<float> monoIn, modelIn, modelOut, downOut, wet;
    std::vector<NAM_SAMPLE> namIn;
    std::array<std::vector<NAM_SAMPLE>, 3> namOut;

    juce::AudioBuffer<float> dryDelay; // pulito ritardato (per Blend e bypass)
    int dryWrite = 0;

    float wSmooth[3] = { 0.0f, 1.0f, 0.0f };
    float lowGainSmooth = 1.0f;
    float weightCoeff = 0.999f;
    float paramCoeff = 0.999f;
    float onSmooth = 1.0f, blendSmooth = 1.0f;
    float inGainSmooth = 1.0f, outGainSmooth = 1.0f;

    std::atomic<float>* gainParam   = nullptr;
    std::atomic<float>* blendParam  = nullptr;
    std::atomic<float>* onParam     = nullptr;
    std::atomic<float>* inputParam  = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IpnosyProcessor)
};
