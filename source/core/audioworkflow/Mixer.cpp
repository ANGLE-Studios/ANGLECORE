/**********************************************************************
**
** This file is part of ANGLECORE, an open-source software development
** kit for audio plugins.
**
** ANGLECORE is free software: you can redistribute it and / or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** ANGLECORE is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with ANGLECORE.If not, see <https://www.gnu.org/licenses/>.
**
** Copyright(C) 2020, ANGLE
**
**********************************************************************/

#include "Mixer.h"

#include "../../config/AudioConfig.h"
#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    Mixer::Mixer() :
        Worker(ANGLECORE_NUM_VOICES * ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE * ANGLECORE_NUM_CHANNELS, ANGLECORE_NUM_CHANNELS),
        m_totalNumInstruments(ANGLECORE_NUM_VOICES * ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE),
        m_voiceStart(ANGLECORE_NUM_VOICES),
        m_rackStart(ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)
    {
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            m_voiceIncrements[v] = ANGLECORE_NUM_VOICES - v;
            m_voiceIsOn[v] = false;
        }

        for (unsigned short i = 0; i < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE; i++)
        {
            m_rackIncrements[i] = ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE - i;
            m_rackIsOn[i] = false;
        }
    }

    void Mixer::work(unsigned int numSamplesToWorkOn)
    {
        for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
        {
            floating_type* output = getOutputStream(c);

            /*
            * The output bus is supposed to be already connected to existing streams
            * (towards the Exporter), so we do not need to check if 'output' is a
            * null pointer. We know it is not.

            /* We first clear the output buffer: */
            for (unsigned int s = 0; s < numSamplesToWorkOn; s++)
                output[s] = static_cast<floating_type>(0.0);

            /* We iterate through the voices using the increments */
            for (unsigned short v = m_voiceStart; v < ANGLECORE_NUM_VOICES; v += m_voiceIncrements[v])
            {
                /* We iterate through the racks using the increments */
                for (unsigned short i = m_rackStart; i < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE; i += m_rackIncrements[i])
                {
                    /*
                    * The following formula computes the input port number
                    * corresponding to the current voice, instrument rack, and
                    * channel:
                    */
                    unsigned short inputPortNumber = v * ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE * ANGLECORE_NUM_CHANNELS + i * ANGLECORE_NUM_CHANNELS + c;

                    const floating_type* input = getInputStream(inputPortNumber);

                    /*
                    * Just like the output bus, the input bus is supposed to be
                    * already connected to existing streams, so we do not need to
                    * check if 'input' is a null pointer. We know it is not.
                    */

                    /*
                    * We can finally sum the audio output of each instruments to its
                    * corresponding output stream.
                    */
                    for (unsigned int s = 0; s < numSamplesToWorkOn; s++)
                        output[s] += input[s];
                }
            }
        }
    }

    void Mixer::turnVoiceOn(unsigned short voiceNumber)
    {
        m_voiceIsOn[voiceNumber] = true;
        updateVoiceIncrements();
    }

    void Mixer::turnVoiceOff(unsigned short voiceNumber)
    {
        m_voiceIsOn[voiceNumber] = false;
        updateVoiceIncrements();
    }

    void Mixer::turnRackOn(unsigned short rackNumber)
    {
        m_rackIsOn[rackNumber] = true;
        updateRackIncrements();
    }

    void Mixer::turnRackOff(unsigned short rackNumber)
    {
        m_rackIsOn[rackNumber] = false;
        updateRackIncrements();
    }

    void Mixer::updateVoiceIncrements()
    {
        /*
        * The algorithm used here is a fast, backward algorithm that computes the
        * number of jumps the mixer should make after mixing all the instruments of
        * a voice. Note that the last value of m_voiceIncrements is always equal to
        * 1, because it is never modified after the constructor is called, not even
        * by this method.
        */

        for (uint32_t i = ANGLECORE_NUM_VOICES - 1; i >= 1; i--)

            /*
            * We only mix a voice if that voice is on. If it is, we fix an increment
            * of 1 after the previous voice. Otherwise, we should skip the voice, so
            * we fix an increment that ensures we bypass the voice when traversing
            * the input bus.
            */
            m_voiceIncrements[i - 1] = m_voiceIsOn[i] ? 1 : m_voiceIncrements[i] + 1;

        /*
        * Finally, using the same test as before, we compute the start position
        * within the list of voices, which may vary depending on the voices' on/off
        * status. The start position can be 0, if the first voice should be
        * rendered. Otherwise, we use the precomputed increments to determine the
        * position of the first voice to mix.
        */
        m_voiceStart = m_voiceIsOn[0] ? 0 : m_voiceIncrements[0];
    }

    void Mixer::updateRackIncrements()
    {
        /*
        * The algorithm used here is a fast, backward algorithm that computes the
        * number of jumps the mixer should make while mixing all the instruments of
        * a voice. Note that the last value of m_rackIncrements is always equal to
        * 1, because it is never modified after the constructor is called, not even
        * by this method.
        */

        for (uint32_t i = ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE - 1; i >= 1; i--)

            /*
            * We only mix an instrument rack if the rack is on. If it is, we fix an
            * increment of 1 after the previous rack. Otherwise, we should skip the
            * rack, so we fix an increment that ensures we bypass the rack when
            * traversing the input bus.
            */
            m_rackIncrements[i - 1] = m_rackIsOn[i] ? 1 : m_rackIncrements[i] + 1;

        /*
        * Finally, using the same test as before, we compute the start position
        * within the list of racks, which may vary depending on the racks' on/off
        * status. The start position can be 0, if the first rack should be rendered.
        * Otherwise, we use the precomputed increments to determine the position of
        * the first rack to mix.
        */
        m_rackStart = m_rackIsOn[0] ? 0 : m_rackIncrements[0];
    }
}