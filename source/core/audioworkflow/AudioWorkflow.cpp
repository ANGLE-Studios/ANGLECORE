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
        m_exporter(std::make_shared<Exporter<float>>()),
        m_mixer(std::make_shared<Mixer>())
    {
        addWorker(m_exporter);
        addWorker(m_mixer);

        /* We connect the mixer into the exporter */
        for (unsigned short int c = 0; c < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS; c++)
        {
            std::shared_ptr<Stream> exporterInputStream = std::make_shared<Stream>();
            addStream(exporterInputStream);

            plugStreamIntoWorker(exporterInputStream->id, m_exporter->id, c);
            plugWorkerIntoStream(m_mixer->id, c, exporterInputStream->id);
        }
    }

    std::vector<std::shared_ptr<Worker>> AudioWorkflow::buildRenderingSequence() const
    {
        std::vector<std::shared_ptr<Worker>> renderingSequence;
        completeRenderingSequenceForWorker(m_exporter, renderingSequence);
        return renderingSequence;
    }
}