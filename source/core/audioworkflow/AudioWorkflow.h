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
#include "../../config/AudioConfig.h"
#include "Voice.h"
#include "GlobalContext.h"
#include "instrument/Instrument.h"

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

        /**
        * Tries to find a Rack that is empty in all voices to insert an instrument
        * inside. Returns a valid rack number corresponding to an empty rack if has
        * found one, and an out-of-range number otherwise.
        */
        unsigned short findEmptyRackNumber() const;

        /**
        * Adds an Instrument to the given Voice, at the given \p rackNumber, then
        * build the Instrument's Environment and plan its bridging to the real-time
        * rendering pipeline by completing the given \p planToComplete. Note that
        * every parameter is expected to be in-range and valid, and that no safety
        * check will be performed by this method.
        * @param[in] voiceNumber Voice to place the Instrument in.
        * @param[in] rackNumber Rack to insert the Instrument into within the given
        *   voice.
        * @param[in] instrument The Instrument to insert. It should not be a null
        *   pointer.
        * @param[in] planToComplete The ConnectionPlan to complete with bridging
        *   instructions.
        */
        void addInstrumentAndPlanBridging(unsigned short voiceNumber, unsigned short rackNumber, const std::shared_ptr<Instrument>& instrument, ConnectionPlan& planToComplete);

    protected:

        /**
        * Retrieve the ID of the Mixer's input Stream at the provided location. Note
        * that every parameter is expected to be in-range, and that no safety check
        * will be performed by this method.
        * @param[in] voiceNumber Voice whose workers ultimately fill in the Stream.
        * @param[in] rackNumber Rack at the end of which the Stream is located.
        * @param[in] channel Audio channel that the Stream corresponds to.
        */
        uint32_t getMixerInputStreamID(unsigned short voiceNumber, unsigned short rackNumber, unsigned short channel) const;

        /**
        * Retrieve the ID of the Stream containing the current sample rate in the
        * AudioWorkflow's global context.
        */
        uint32_t getSampleRateStreamID() const;

        /**
        * Retrieve the ID of the Stream containing the inverse of the current sample
        * rate in the AudioWorkflow's global context.
        */
        uint32_t getInverseSampleRateStreamID() const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * frequency. Note that voiceNumber is expected to be in-range, and that no
        * safety check will be performed by this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getFrequencyStreamID(unsigned short voiceNumber) const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * frequency divided by the global sample rate. Note that voiceNumber is
        * expected to be in-range, and that no safety check will be performed by
        * this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getFrequencyOverSampleRateStreamID(unsigned short voiceNumber) const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * velocity. Note that voiceNumber is expected to be in-range, and that no
        * safety check will be performed by this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getVelocityStreamID(unsigned short voiceNumber) const;

    private:
        std::shared_ptr<Exporter<float>> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
        Voice m_voices[ANGLECORE_NUM_VOICES];
        GlobalContext m_globalContext;
    };
}