/*
    Ipnosy - Meteora Plugins
    Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati.

    Catena (mono, come un pedale vero):

      in -> Input -> [48 kHz: 3 catture NAM sfumate dalla manopola] -> [valvola Marley da ore 15] -> wet
      pulito (stereo) -> ritardo uguale alla latenza ------------------------------------------------> dry
      uscita = Blend(dry, wet) -> Output ; footswitch: bypass sul dry ritardato
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include "NAM/get_dsp.h"

#include <cmath>
#include <filesystem>

IpnosyProcessor::IpnosyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    gainParam   = apvts.getRawParameterValue ("gain");
    blendParam  = apvts.getRawParameterValue ("blend");
    onParam     = apvts.getRawParameterValue ("on");
    inputParam  = apvts.getRawParameterValue ("input");
    outputParam = apvts.getRawParameterValue ("output");

    loadModels();
}

IpnosyProcessor::~IpnosyProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout IpnosyProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percent = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f, percent));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "blend", 1 }, "Blend",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f, percent));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "on", 1 }, "On", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input", 1 }, "Input",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
void IpnosyProcessor::loadModels()
{
    struct Embedded { const char* data; int size; };
    const Embedded embedded[3] = {
        { BinaryData::ag_0900_nam, BinaryData::ag_0900_namSize },
        { BinaryData::ag_1200_nam, BinaryData::ag_1200_namSize },
        { BinaryData::ag_max_nam,  BinaryData::ag_max_namSize  }
    };

    numModelsLoaded = 0;

    for (int k = 0; k < 3; ++k)
    {
        auto tmp = juce::File::createTempFile (".nam");

        try
        {
            if (tmp.replaceWithData (embedded[k].data, (size_t) embedded[k].size))
            {
               #if JUCE_WINDOWS
                const std::filesystem::path path (tmp.getFullPathName().toWideCharPointer());
               #else
                const std::filesystem::path path (tmp.getFullPathName().toStdString());
               #endif
                models[(size_t) k] = nam::get_dsp (path);
            }
        }
        catch (const std::exception& e)
        {
            juce::ignoreUnused (e);
            DBG ("Ipnosy: errore nel caricamento del modello " << k << ": " << e.what());
            models[(size_t) k].reset();
        }

        tmp.deleteFile();

        if (models[(size_t) k] != nullptr)
            ++numModelsLoaded;
    }
}

//==============================================================================
void IpnosyProcessor::targetWeights (float v, float w[3], float& levelGain)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    w[0] = w[1] = w[2] = 0.0f;
    levelGain = 1.0f;

    if (v <= kPos0900)
    {
        // da zero a ore 9: suono di ore 9 con il volume che sale da zero
        const float x = v / kPos0900;
        w[0] = 1.0f;
        levelGain = x * x;
    }
    else if (v <= kPos1200)
    {
        const float x = (v - kPos0900) / (kPos1200 - kPos0900);
        w[0] = 1.0f - x;
        w[1] = x;
    }
    else
    {
        const float x = (v - kPos1200) / (kPosMax - kPos1200);
        w[1] = 1.0f - x;
        w[2] = x;
    }
}

double IpnosyProcessor::tubeDriveFor (float gain)
{
    // da ore 15 al massimo il Drive della valvola va da 0,35 a 1
    const double x = juce::jlimit (0.0, 1.0, (double) (gain - kPos1500) / (double) (kPosMax - kPos1500));
    return 0.35 + 0.65 * x;
}

int IpnosyProcessor::measureResamplerLatency (double rate) const
{
    constexpr int block = 64;
    StreamResampler u, d;
    u.prepare (rate, kModelRate, block);
    const int maxM = u.maxOutputFor (block);
    d.prepare (kModelRate, rate, maxM, roundTripDownOffset (rate, kModelRate));

    SampleFifo f;
    f.prepare (8192);
    f.pushZeros (kFifoPrefill);

    std::vector<float> in ((size_t) block), mid ((size_t) maxM),
                       dn ((size_t) d.maxOutputFor (maxM)), out ((size_t) block);

    float best = 0.0f;
    int bestIdx = 0;

    for (int b = 0; b < 64; ++b)
    {
        std::fill (in.begin(), in.end(), 0.0f);
        if (b == 0)
            in[0] = 1.0f;

        const int m = u.process (in.data(), block, mid.data(), maxM);
        const int k = d.process (mid.data(), m, dn.data(), (int) dn.size());
        f.push (dn.data(), k);
        f.pop (out.data(), block);

        for (int i = 0; i < block; ++i)
        {
            if (std::abs (out[(size_t) i]) > best)
            {
                best = std::abs (out[(size_t) i]);
                bestIdx = b * block + i;
            }
        }
    }

    return bestIdx;
}

void IpnosyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (32, samplesPerBlock);
    resampling = std::abs (sampleRate - kModelRate) > 0.5;

    int maxM = maxBlock;
    int namLatency = 0;

    if (resampling)
    {
        up.prepare (sampleRate, kModelRate, maxBlock);
        maxM = up.maxOutputFor (maxBlock);
        down.prepare (kModelRate, sampleRate, maxM, roundTripDownOffset (sampleRate, kModelRate));
        outFifo.prepare (maxBlock * 4 + 1024);
        outFifo.clear();
        outFifo.pushZeros (kFifoPrefill);
        namLatency = measureResamplerLatency (sampleRate);
    }

    tube.prepare (sampleRate);
    latency = namLatency + tube.getLatency();

    monoIn.assign ((size_t) maxBlock, 0.0f);
    wet.assign ((size_t) maxBlock, 0.0f);
    modelIn.assign ((size_t) maxM, 0.0f);
    modelOut.assign ((size_t) maxM, 0.0f);
    downOut.assign ((size_t) (resampling ? down.maxOutputFor (maxM) : 1), 0.0f);

    namIn.assign ((size_t) kModelChunk, (NAM_SAMPLE) 0);
    for (auto& v : namOut)
        v.assign ((size_t) kModelChunk, (NAM_SAMPLE) 0);

    for (auto& m : models)
    {
        if (m == nullptr)
            continue;

        try
        {
            m->Reset (kModelRate, kModelChunk);
        }
        catch (const std::exception&)
        {
        }
    }

    weightCoeff = (float) std::exp (-1.0 / (0.03 * kModelRate));
    paramCoeff  = (float) std::exp (-1.0 / (0.015 * sampleRate));

    targetWeights (gainParam->load(), wSmooth, lowGainSmooth);
    onSmooth      = onParam->load() > 0.5f ? 1.0f : 0.0f;
    blendSmooth   = blendParam->load();
    inGainSmooth  = juce::Decibels::decibelsToGain (inputParam->load());
    outGainSmooth = juce::Decibels::decibelsToGain (outputParam->load());

    dryDelay.setSize (2, latency + 1);
    dryDelay.clear();
    dryWrite = 0;

    setLatencySamples (latency);
}

bool IpnosyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    return in.size() <= out.size();
}

//==============================================================================
void IpnosyProcessor::runModels (const float* in, float* out, int m)
{
    float tw[3], tg;
    targetWeights (gainParam->load(), tw, tg);

    for (int off = 0; off < m; off += kModelChunk)
    {
        const int len = juce::jmin (kModelChunk, m - off);

        for (int i = 0; i < len; ++i)
            namIn[(size_t) i] = (NAM_SAMPLE) in[off + i];

        for (size_t k = 0; k < 3; ++k)
        {
            if (models[k] != nullptr)
            {
                NAM_SAMPLE* ip = namIn.data();
                NAM_SAMPLE* op = namOut[k].data();
                models[k]->process (&ip, &op, len);
            }
            else
            {
                std::copy (namIn.begin(), namIn.begin() + len, namOut[k].begin());
            }
        }

        for (int i = 0; i < len; ++i)
        {
            float mix = 0.0f;

            for (size_t k = 0; k < 3; ++k)
            {
                wSmooth[k] = tw[k] + weightCoeff * (wSmooth[k] - tw[k]);
                mix += wSmooth[k] * (float) namOut[k][(size_t) i];
            }

            lowGainSmooth = tg + weightCoeff * (lowGainSmooth - tg);
            out[off + i] = mix * lowGainSmooth;
        }
    }
}

void IpnosyProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n)
{
    const int numIn  = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int numOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels(), 2);

    const float gain      = gainParam->load();
    const float inTarget  = juce::Decibels::decibelsToGain (inputParam->load());
    const float outTarget = juce::Decibels::decibelsToGain (outputParam->load());
    const float onTarget  = onParam->load() > 0.5f ? 1.0f : 0.0f;
    const float blendTarget = juce::jlimit (0.0f, 1.0f, blendParam->load());

    // 1) somma mono + Input
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numIn; ++ch)
            s += buffer.getSample (ch, start + i);
        if (numIn > 0)
            s /= (float) numIn;

        inGainSmooth = inTarget + paramCoeff * (inGainSmooth - inTarget);
        monoIn[(size_t) i] = s * inGainSmooth;
    }

    // 2) catture NAM (a 48 kHz)
    if (resampling)
    {
        const int m = up.process (monoIn.data(), n, modelIn.data(), (int) modelIn.size());
        runModels (modelIn.data(), modelOut.data(), m);
        const int k = down.process (modelOut.data(), m, downOut.data(), (int) downOut.size());
        outFifo.push (downOut.data(), k);
        outFifo.pop (wet.data(), n);
    }
    else
    {
        runModels (monoIn.data(), wet.data(), n);
    }

    // 3) valvola di Marley: si accende da ore 15
    tube.process (wet.data(), n, tubeDriveFor (gain), gain >= kPos1500 - 0.0005f);

    // 4) Blend con il pulito, Output e footswitch
    const int delayLen = dryDelay.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        onSmooth      = onTarget    + paramCoeff * (onSmooth - onTarget);
        blendSmooth   = blendTarget + paramCoeff * (blendSmooth - blendTarget);
        outGainSmooth = outTarget   + paramCoeff * (outGainSmooth - outTarget);

        const float w = wet[(size_t) i];

        float dryIn[2];
        dryIn[0] = numIn > 0 ? buffer.getSample (0, start + i) : 0.0f;
        dryIn[1] = numIn > 1 ? buffer.getSample (1, start + i) : dryIn[0];

        for (int ch = 0; ch < numOut; ++ch)
        {
            auto* dl = dryDelay.getWritePointer (ch);
            dl[dryWrite] = dryIn[ch];
            const float d = dl[(dryWrite + 1) % delayLen]; // pulito allineato al wet

            const float pedal = (d + blendSmooth * (w - d)) * outGainSmooth;
            buffer.setSample (ch, start + i, d + onSmooth * (pedal - d));
        }

        dryWrite = (dryWrite + 1) % delayLen;
    }
}

void IpnosyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, total);

    for (int start = 0; start < total; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, total - start));

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, total);
}

//==============================================================================
void IpnosyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void IpnosyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* IpnosyProcessor::createEditor()
{
    return new IpnosyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IpnosyProcessor();
}
