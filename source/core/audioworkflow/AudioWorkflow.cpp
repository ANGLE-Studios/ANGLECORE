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

#include "AudioWorkflow.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    AudioWorkflow::AudioWorkflow() :
        Workflow(),
        VoiceAssigner(),
        m_exporter(std::make_shared<Exporter<float>>()),
        m_mixer(std::make_shared<Mixer>())
    {
        /* We add the Exporter and Mixer to the AudioWorkflow */
        addWorker(m_exporter);
        addWorker(m_mixer);

        /*
        * And then we create the base streams and workers that will comprise every
        * AudioWorkflow, starting with the Exporter.
        */

        /* We connect the mixer into the exporter */
        for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
        {
            std::shared_ptr<Stream> exporterInputStream = std::make_shared<Stream>();
            addStream(exporterInputStream);

            plugStreamIntoWorker(exporterInputStream->id, m_exporter->id, c);
            plugWorkerIntoStream(m_mixer->id, c, exporterInputStream->id);
        }

        /*
        * We prepare all of the Mixer's input streams, so that the memory is already
        * allocated, and the streams do not need to be created and destroyed
        * repeatedly, which would have unnecessarily consumed ID numbers.
        */
        for (unsigned short i = 0; i < m_mixer->getNumInputs(); i++)
        {
            std::shared_ptr<Stream> mixerInputStream = std::make_shared<Stream>();
            addStream(mixerInputStream);
            plugStreamIntoWorker(mixerInputStream->id, m_mixer->id, i);
        }
    }

    std::vector<std::shared_ptr<Worker>> AudioWorkflow::buildRenderingSequence(const ConnectionPlan& connectionPlan) const
    {
        std::vector<std::shared_ptr<Worker>> renderingSequence;
        completeRenderingSequenceForWorker(m_exporter, connectionPlan, renderingSequence);
        return renderingSequence;
    }

    void AudioWorkflow::setExporterOutput(float** buffer, unsigned short numChannels, uint32_t startSample)
    {
        m_exporter->setOutputBuffer(buffer, numChannels, startSample);
    }

    uint32_t AudioWorkflow::getMixerInputStreamID(unsigned short voiceNumber, unsigned short instrumentRackNumber, unsigned short channel) const
    {
        /*
        * We assume voiceNumber, instrumentRackNumber, and channel are in-range, and
        * we compute the corresponding input port to retrieve the stream ID from.
        */
        unsigned short inputPort = voiceNumber * ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE * ANGLECORE_NUM_CHANNELS + instrumentRackNumber * ANGLECORE_NUM_CHANNELS + channel;

        /*
        * Provided every input number is in-range, which we assumed, the inputPort
        * is guaranteed to be in-range for the Mixer's input bus. So we do not need
        * to perform any check before accessing this input bus. In addition, because
        * all the Mixer's input streams were initialized on construction, it is also
        * guaranteed that the Mixer's input bus does not contain any null pointer.
        */
        return m_mixer->getInputBus()[inputPort]->id;
    }
}