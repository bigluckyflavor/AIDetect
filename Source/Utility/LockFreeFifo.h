#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/**
    Preallocated single-producer / single-consumer audio ring buffer.

    The audio thread is the sole producer (write); the analysis worker is the
    sole consumer (read). Index management is delegated to juce::AbstractFifo,
    which is designed for exactly this SPSC use and is wait-free on both sides.

    prepare() is the only call that allocates. write()/read() never allocate,
    never lock, and never block, so write() is safe to call from processBlock.
*/
class AudioRingBuffer
{
public:
    AudioRingBuffer() = default;

    /** Allocates storage. Call from a non-audio thread before streaming. */
    void prepare (int numChannels, int capacitySamples)
    {
        jassert (numChannels > 0 && capacitySamples > 0);
        channels = numChannels;
        buffer.setSize (numChannels, capacitySamples, false, true, false);
        buffer.clear();
        fifo.setTotalSize (capacitySamples);
        fifo.reset();
    }

    /** Discards any unread samples. Safe when neither side is mid-operation. */
    void reset()
    {
        fifo.reset();
    }

    int getNumReady()    const noexcept { return fifo.getNumReady(); }
    int getFreeSpace()   const noexcept { return fifo.getFreeSpace(); }
    int getCapacity()    const noexcept { return buffer.getNumSamples(); }
    int getNumChannels() const noexcept { return channels; }

    //==========================================================================
    /** Producer side (audio thread). Copies up to numSamples from source into
        the ring. If the ring is nearly full, writes as many as fit and returns
        that count (the surplus is dropped - the caller may track the shortfall
        as an overrun). Never allocates, locks, or blocks. */
    int write (const juce::AudioBuffer<float>& source, int sourceChannels, int numSamples) noexcept
    {
        const int toWrite = juce::jmin (numSamples, fifo.getFreeSpace());
        if (toWrite <= 0)
            return 0;

        int start1, size1, start2, size2;
        fifo.prepareToWrite (toWrite, start1, size1, start2, size2);

        for (int ch = 0; ch < channels; ++ch)
        {
            const bool haveSource = ch < sourceChannels;
            const float* src = haveSource ? source.getReadPointer (ch) : nullptr;

            if (size1 > 0)
            {
                if (haveSource) buffer.copyFrom (ch, start1, src, size1);
                else            buffer.clear (ch, start1, size1);
            }
            if (size2 > 0)
            {
                if (haveSource) buffer.copyFrom (ch, start2, src + size1, size2);
                else            buffer.clear (ch, start2, size2);
            }
        }

        fifo.finishedWrite (size1 + size2);
        return size1 + size2;
    }

    /** Consumer side (worker thread). Copies up to numSamples into dest at
        destOffset. If dest has more channels than the ring, extra dest channels
        receive a copy of the ring's last channel. Returns samples read. */
    int read (juce::AudioBuffer<float>& dest, int destOffset, int numSamples) noexcept
    {
        const int toRead = juce::jmin (numSamples, fifo.getNumReady());
        if (toRead <= 0)
            return 0;

        int start1, size1, start2, size2;
        fifo.prepareToRead (toRead, start1, size1, start2, size2);

        const int destChannels = dest.getNumChannels();
        for (int ch = 0; ch < destChannels; ++ch)
        {
            const int srcCh = juce::jmin (ch, channels - 1);
            if (size1 > 0) dest.copyFrom (ch, destOffset,         buffer, srcCh, start1, size1);
            if (size2 > 0) dest.copyFrom (ch, destOffset + size1, buffer, srcCh, start2, size2);
        }

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

private:
    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> buffer;
    int channels = 0;

    JUCE_DECLARE_NON_COPYABLE (AudioRingBuffer)
};
