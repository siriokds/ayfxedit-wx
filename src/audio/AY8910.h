#pragma once

#include <cstdint>
#include <iosfwd>

static constexpr int kAudioSampleRate   = 48000;
static constexpr int kAudioBufferFrames = 4096;  // stereo s16 pairs -> 8192 s16 values
static constexpr int kAudioBufferSize   = kAudioBufferFrames * 2;  // stereo
static constexpr int kAYClockRate       = 1773400;

class AY8910
{
public:
    AY8910();
    ~AY8910();
    void Init(int clockRate, int sampleRate = kAudioSampleRate);
    void Reset(int clockRate, int sampleRate = kAudioSampleRate);
    void WriteRegister(uint8_t value);
    uint8_t ReadRegister();
    void SelectRegister(uint8_t reg);
    void Tick(unsigned int clockCycles);
    int EndFrame(int16_t* pSampleBuffer);

    void WriteReg(uint8_t reg, uint8_t value);

private:
    void EnvelopeReset();
    void Sync();

private:
    uint8_t  m_Registers[16];
    uint8_t  m_SelectedRegister;
    uint16_t m_TonePeriod[3];
    uint16_t m_ToneCounter[3];
    uint8_t  m_Amplitude[3];
    uint8_t  m_NoisePeriod;
    uint16_t m_NoiseCounter;
    uint32_t m_NoiseShift;
    uint16_t m_EnvelopePeriod;
    uint16_t m_EnvelopeCounter;
    bool     m_EnvelopeSegment;
    uint8_t  m_EnvelopeStep;
    uint8_t  m_EnvelopeVolume;
    bool     m_ToneDisable[3];
    bool     m_NoiseDisable[3];
    bool     m_EnvelopeMode[3];
    bool     m_Sign[3];
    int      m_iCycleCounter;
    int      m_iSampleCounter;
    int      m_iCyclesPerSample;
    int16_t* m_pBuffer;
    int      m_iBufferIndex;
    int      m_ElapsedCycles;
    int      m_iClockRate;
    int16_t  m_CurrentSample;
};

inline constexpr uint8_t kAY8910RegisterMask[16] = {0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0x1F,0xFF,0x1F,0x1F,0x1F,0xFF,0xFF,0x0F,0xFF,0xFF};
inline constexpr int16_t kAY8910VolumeTable[16]  = {0,40,60,86,124,186,264,440,518,840,1196,1526,2016,2602,3300,4096};
