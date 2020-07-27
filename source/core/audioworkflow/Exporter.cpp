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

#include "Exporter.h"

#include "../../config/AudioConfig.h"
#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    Exporter::Exporter() :
        Worker(ANGLECORE_NUM_CHANNELS, 0),
        m_outputBuffer(nullptr),
        m_numOutputChannels(0),
        m_numVoicesOn(0)
    {}

    void Exporter::setOutputBuffer(export_type** buffer, unsigned short numChannels, uint32_t startSample)
    {
        m_outputBuffer = buffer;
        m_numOutputChannels = numChannels;
        m_startSample = startSample;
    }

    void Exporter::work(unsigned int numSamplesToWorkOn)
    {
        /*
        * It is assumed that both m_numOutputChannels and numSamplesToWorkOn are
        * in-range, i.e. less than or equal to the output buffer's number of
        * channels and the stream and buffer size respectively. It is also assumed
        * the output buffer has been properly set to a valid memory location.
        */

        /*
        * If the number of voices that are currently on is zero, then we do not need
        * to do any calculation: we can simply output zero.
        */
        if (m_numVoicesOn == 0)
        {
            /*
            * We fill the output buffer with zeros, without forgetting to start on
            * the start sample provided:
            */
            for (unsigned short c = 0; c < m_numOutputChannels; c++)
                for (uint32_t i = 0; i < numSamplesToWorkOn; i++)
                    m_outputBuffer[c][i + m_startSample] = static_cast<export_type>(0.0);

            /* And then we simply return here. */
            return;
        }

        /*
        * If we arrive here, then the number of voices currently on is greater than
        * zero, which means we have something to render.
        */

        /*
        * If the host requests less channels than rendered, we sum their content
        * using a modulo approach.
        */
        if (m_numOutputChannels < ANGLECORE_NUM_CHANNELS)
        {
            /* We first clear the output buffer */
            for (unsigned short c = 0; c < m_numOutputChannels; c++)
                for (uint32_t i = 0; i < numSamplesToWorkOn; i++)
                    m_outputBuffer[c][i + m_startSample] = static_cast<export_type>(0.0);

            /* And then we compute the sum into the output buffer */
            for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
            {
                const floating_type* channel = getInputStream(c);
                for (uint32_t i = 0; i < numSamplesToWorkOn; i++)
                    m_outputBuffer[c % m_numOutputChannels][i + m_startSample] += static_cast<export_type>(channel[i] * static_cast<floating_type>(ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN));
            }
        }

        /*
        * Otherwise, if we have not rendered enough channels for the host, we simply
        * duplicate data. Note that we do not have to clear the output buffer in this
        * case, as we write directly into it without performing any addition.
        */
        else
        {
            for (unsigned int c = 0; c < m_numOutputChannels; c++)
            {
                const floating_type* channel = getInputStream(c % ANGLECORE_NUM_CHANNELS);
                for (uint32_t i = 0; i < numSamplesToWorkOn; i++)
                    m_outputBuffer[c][i + m_startSample] = static_cast<export_type>(channel[i] * static_cast<floating_type>(ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN));
            }
        }
    }

    void Exporter::incrementVoiceCount()
    {
        if (m_numVoicesOn < ANGLECORE_NUM_VOICES)
            m_numVoicesOn++;
    }

    void Exporter::decrementVoiceCount()
    {
        if (m_numVoicesOn > 0)
            m_numVoicesOn--;
    }
}