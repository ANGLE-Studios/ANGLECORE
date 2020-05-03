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

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "workflow/Workflow.h"
#include "voiceassigner/VoiceAssigner.h"
#include "Exporter.h"
#include "Mixer.h"

namespace ANGLECORE
{
    /**
    * \class AudioWorkflow AudioWorkflow.h
    * Workflow internally structured to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class AudioWorkflow :
        public Workflow,
        public VoiceAssigner
    {
    public:

        /** Builds the base structure of the AudioWorkflow (Exporter, Mixer...) */
        AudioWorkflow();

        /**
        * Builds and returns the Workflow's rendering sequence, starting from its
        * Exporter. This method allocates memory, so it should never be called by
        * the real-time thread. Note that the method relies on the move semantics to
        * optimize its vector return.
        * @param[in] connectionPlan The ConnectionPlan that will be executed next,
        *   and that should therefore be taken into account in the computation
        */
        std::vector<std::shared_ptr<Worker>> buildRenderingSequence(const ConnectionPlan& connectionPlan) const;

        /**
        * Set the Exporter's memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        * @param[in] numChannels Number of output channels.
        * @param[in] startSample Position to start from in the buffer.
        */
        void setExporterOutput(float** buffer, unsigned short numChannels, uint32_t startSample);

    private:
        std::shared_ptr<Exporter<float>> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
    };
}