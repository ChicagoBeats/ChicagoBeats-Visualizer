#include "PluginProcessor.h"
#include "PluginEditor.h"

CBProcessor::CBProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true))
{
    addParameter (vfxOn = new AudioParameterBool ("vfx", "Motion VFX", true));
    for (auto& k : keyVel) k = 0.0f;
}

bool CBProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == AudioChannelSet::stereo()
         || l.getMainOutputChannelSet() == AudioChannelSet::mono());
}

void CBProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    // MIDI -> key velocities (used by the piano keys, milestone 4 expands this)
    MidiBuffer::Iterator it (midi);
    MidiMessage msg;
    int pos = 0;
    while (it.getNextEvent (msg, pos))
    {
        if (msg.isNoteOn())       keyVel[msg.getNoteNumber()] = msg.getFloatVelocity();
        else if (msg.isNoteOff()) keyVel[msg.getNoteNumber()] = 0.0f;
    }

    const int n = b.getNumSamples();
    if (n <= 0 || b.getNumChannels() <= 0) return;

    const float* l = b.getReadPointer (0);
    const float* r = b.getNumChannels() > 1 ? b.getReadPointer (1) : l;

    float pl = 0.0f, pr = 0.0f;
    const int wp = writePos.load();
    for (int i = 0; i < n; ++i)
    {
        pl = jmax (pl, std::abs (l[i]));
        pr = jmax (pr, std::abs (r[i]));
        fifo[(wp + i) & (fifoSize - 1)] = 0.5f * (l[i] + r[i]);
    }
    writePos.store ((wp + n) & (fifoSize - 1));

    peakL.store (jmax (peakL.load(), pl));
    peakR.store (jmax (peakR.load(), pr));
    // audio is left untouched (pass-through)
}

void CBProcessor::copyLatestSamples (float* dest, int n) const
{
    const int wp = writePos.load();
    const int start = (wp - n + fifoSize) & (fifoSize - 1);
    for (int i = 0; i < n; ++i)
        dest[i] = fifo[(start + i) & (fifoSize - 1)];
}

AudioProcessorEditor* CBProcessor::createEditor() { return new CBEditor (*this); }

void CBProcessor::getStateInformation (MemoryBlock& dest)
{
    XmlElement xml ("CBV");
    xml.setAttribute ("vfx", vfxOn->get());
    xml.setAttribute ("scale", (double) uiScale.load());
    copyXmlToBinary (xml, dest);
}

void CBProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, size));
    if (xml != nullptr && xml->hasTagName ("CBV"))
    {
        *vfxOn = xml->getBoolAttribute ("vfx", true);
        uiScale = (float) xml->getDoubleAttribute ("scale", 1.0);
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CBProcessor(); }
