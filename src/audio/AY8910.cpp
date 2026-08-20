/*
 * Gearcoleco - ColecoVision Emulator
 * Copyright (C) 2026  Saverio Russo

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "AY8910.h"

AY8910::AY8910()
{
    m_pBuffer = nullptr;
}

AY8910::~AY8910()
{
    delete[] m_pBuffer;
    m_pBuffer = nullptr;
}

void AY8910::Init(int clockRate, int sampleRate)
{
    m_pBuffer = new int16_t[kAudioBufferSize];
    Reset(clockRate, sampleRate);
}

void AY8910::Reset(int clockRate, int sampleRate)
{
    m_iClockRate = clockRate;
    m_iCyclesPerSample = m_iClockRate / sampleRate;

    for (int i = 0; i < 16; i++)
    {
        m_Registers[i] = 0;
    }

    for (int i = 0; i < 3; i++)
    {
        m_TonePeriod[i] = 0;
        m_ToneCounter[i] = 0;
        m_Amplitude[i] = 0;
        m_ToneDisable[i] = false;
        m_NoiseDisable[i] = false;
        m_EnvelopeMode[i] = false;
        m_Sign[i] = false;
    }

    m_SelectedRegister = 0;
    m_NoisePeriod = 0;
    m_NoiseCounter = 0;
    m_NoiseShift = 1;
    m_EnvelopePeriod = 0;
    m_EnvelopeCounter = 0;
    m_EnvelopeSegment = false;
    m_EnvelopeStep = 0;
    m_EnvelopeVolume = 0;
    m_iCycleCounter = 0;
    m_iSampleCounter = 0;
    m_iBufferIndex = 0;

    for (int i = 0; i < kAudioBufferSize; i++)
    {
        m_pBuffer[i] = 0;
    }

    m_ElapsedCycles = 0;
    m_CurrentSample = 0;
}


void AY8910::WriteReg(uint8_t reg, uint8_t value)
{
    SelectRegister(reg);
    WriteRegister(value);
}

void AY8910::WriteRegister(uint8_t value)
{
    Sync();

    m_Registers[m_SelectedRegister] = value & kAY8910RegisterMask[m_SelectedRegister];

    switch (m_SelectedRegister)
    {
        // Channel A tone period
        case 0:
        case 1:
        {
            m_TonePeriod[0] = (m_Registers[1] << 8) | m_Registers[0];
            if (m_TonePeriod[0] == 0)
            {
                m_TonePeriod[0] = 1;
            }
            break;
        }
        // Channel B tone period
        case 2:
        case 3:
        {
            m_TonePeriod[1] = (m_Registers[3] << 8) | m_Registers[2];
            if (m_TonePeriod[1] == 0)
            {
                m_TonePeriod[1] = 1;
            }
            break;
        }
        // Channel C tone period
        case 4:
        case 5:
        {
            m_TonePeriod[2] = (m_Registers[5] << 8) | m_Registers[4];
            if (m_TonePeriod[2] == 0)
            {
                m_TonePeriod[2] = 1;
            }
            break;
        }
        // Noise period
        case 6:
        {
            m_NoisePeriod = m_Registers[6];
            if (m_NoisePeriod == 0)
            {
                m_NoisePeriod = 1;
            }
            break;
        }
        // Mixer
        case 7:
        {
            m_ToneDisable[0]  = ((m_Registers[7] >> 0) & 1);
            m_ToneDisable[1]  = ((m_Registers[7] >> 1) & 1);
            m_ToneDisable[2]  = ((m_Registers[7] >> 2) & 1);
            m_NoiseDisable[0] = ((m_Registers[7] >> 3) & 1);
            m_NoiseDisable[1] = ((m_Registers[7] >> 4) & 1);
            m_NoiseDisable[2] = ((m_Registers[7] >> 5) & 1);
            break;
        }
        // Channel A amplitude
        case 8:
        {
            m_Amplitude[0]    = m_Registers[8] & 0x0F;
            m_EnvelopeMode[0] = ((m_Registers[8] >> 4) & 1);
            break;
        }
        // Channel B amplitude
        case 9:
        {
            m_Amplitude[1]    = m_Registers[9] & 0x0F;
            m_EnvelopeMode[1] = ((m_Registers[9] >> 4) & 1);
            break;
        }
        // Channel C amplitude
        case 10:
        {
            m_Amplitude[2]    = m_Registers[10] & 0x0F;
            m_EnvelopeMode[2] = ((m_Registers[10] >> 4) & 1);
            break;
        }
        // Envelope period
        case 11:
        case 12:
        {
            m_EnvelopePeriod = (m_Registers[12] << 8) | m_Registers[11];
            break;
        }
        // Envelope shape
        case 13:
        {
            m_EnvelopeCounter = 0;
            m_EnvelopeSegment = false;
            EnvelopeReset();
            break;
        }
        default:
        {
            break;
        }
    }
}

uint8_t AY8910::ReadRegister()
{
    return m_Registers[m_SelectedRegister];
}

void AY8910::SelectRegister(uint8_t reg)
{
    m_SelectedRegister = reg & 0x0F;
}

void AY8910::EnvelopeReset()
{
    m_EnvelopeStep = 0;

    if (m_EnvelopeSegment)
    {
        switch (m_Registers[13])
        {
            case 8:
            case 11:
            case 13:
            case 14:
            {
                m_EnvelopeVolume = 0x0F;
                break;
            }
            default:
            {
                m_EnvelopeVolume = 0x00;
                break;
            }
        }
    }
    else
    {
        m_EnvelopeVolume = ((m_Registers[13] >> 2) & 1) ? 0x00 : 0x0F;
    }
}

void AY8910::Tick(unsigned int clockCycles)
{
    m_ElapsedCycles += clockCycles;
}

void AY8910::Sync()
{
    for (int i = 0; i < m_ElapsedCycles; i++)
    {
        m_iCycleCounter++;
        if (m_iCycleCounter >= 16)
        {
            m_iCycleCounter -= 16;

            for (int j = 0; j < 3; j++)
            {
                m_ToneCounter[j]++;
                if (m_ToneCounter[j] >= m_TonePeriod[j])
                {
                    m_ToneCounter[j] = 0;
                    m_Sign[j] = !m_Sign[j];
                }
            }

            m_NoiseCounter++;
            if (m_NoiseCounter >= (m_NoisePeriod << 1))
            {
                m_NoiseCounter = 0;
                m_NoiseShift = (m_NoiseShift >> 1) | (((m_NoiseShift ^ (m_NoiseShift >> 3)) & 0x01) << 16);
            }

            m_EnvelopeCounter++;
            if (m_EnvelopeCounter >= (m_EnvelopePeriod << 1))
            {
                m_EnvelopeCounter = 0;

                if (m_EnvelopeStep)
                {
                    if (m_EnvelopeSegment)
                    {
                        if ((m_Registers[13] == 10) || (m_Registers[13] == 12))
                        {
                            m_EnvelopeVolume++;
                        }
                        else if ((m_Registers[13] == 8) || (m_Registers[13] == 14))
                        {
                            m_EnvelopeVolume--;
                        }
                    }
                    else
                    {
                        if (((m_Registers[13] >> 2) & 1))
                        {
                            m_EnvelopeVolume++;
                        }
                        else
                        {
                            m_EnvelopeVolume--;
                        }
                    }
                }

                m_EnvelopeStep++;
                if (m_EnvelopeStep >= 16)
                {
                    if ((m_Registers[13] & 0x09) == 0x08)
                    {
                        m_EnvelopeSegment = !m_EnvelopeSegment;
                    }
                    else
                    {
                        m_EnvelopeSegment = true;
                    }
                    EnvelopeReset();
                }
            }
        }

        m_iSampleCounter++;
        if (m_iSampleCounter >= m_iCyclesPerSample)
        {
            m_iSampleCounter -= m_iCyclesPerSample;
            m_CurrentSample = 0;

            for (int j = 0; j < 3; j++)
            {
                if ((m_ToneDisable[j] || m_Sign[j]) && (m_NoiseDisable[j] || ((m_NoiseShift & 0x01) == 0x01)))
                {
                    m_CurrentSample += m_EnvelopeMode[j] ? kAY8910VolumeTable[m_EnvelopeVolume] : kAY8910VolumeTable[m_Amplitude[j]];
                }
            }

            if (m_iBufferIndex + 1 < kAudioBufferSize)
            {
                m_pBuffer[m_iBufferIndex]     = m_CurrentSample;
                m_pBuffer[m_iBufferIndex + 1] = m_CurrentSample;
                m_iBufferIndex += 2;
            }
        }
    }

    m_ElapsedCycles = 0;
}

int AY8910::EndFrame(int16_t* pSampleBuffer)
{
    Sync();

    int ret = 0;

    if (pSampleBuffer != nullptr)
    {
        ret = m_iBufferIndex;

        for (int i = 0; i < m_iBufferIndex; i++)
        {
            pSampleBuffer[i] = m_pBuffer[i];
        }
    }

    m_iBufferIndex = 0;

    return ret;
}
