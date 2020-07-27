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
#include <stdint.h>
#include <unordered_map>

#include "workflow/Workflow.h"
#include "voiceassigner/VoiceAssigner.h"
#include "../../utility/Lockable.h"
#include "Exporter.h"
#include "Mixer.h"
#include "../../config/AudioConfig.h"
#include "Voice.h"
#include "GlobalContext.h"
#include "instrument/Instrument.h"
#include "ParameterRegister.h"
#include "ParameterRegistrationPlan.h"
#include "parameter/ParameterGenerator.h"

namespace ANGLECORE
{
    /**
    * \class AudioWorkflow AudioWorkflow.h
    * Workflow internally structured to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class AudioWorkflow :
        public Workflow,
        public VoiceAssigner,
        public Lockable
    {
    public:

        /** Builds the base structure of the AudioWorkflow (Exporter, Mixer...) */
        AudioWorkflow();

        /**
        * Sets the sample rate of the AudioWorkflow.
        * @param[in] sampleRate The value of the sample rate, in Hz.
        */
        void setSampleRate(floating_type sampleRate);

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
        * inside. Returns the valid rack number of an empty rack if has found one,
        * and an out-of-range number otherwise.
        */
        unsigned short findEmptyRack() const;

        /**
        * Adds an Instrument to the given Voice, at the given \p rackNumber, then
        * build the Instrument's environment, plan its bridging to the real-time
        * rendering pipeline by completing the given \p connectionPlanToComplete,
        * and complete the \p parameterRegistrationPlan to add new parameters to the
        * appropriate ParameterRegister if they were not already there. Note that
        * every parameter is expected to be in-range and valid, and that no safety
        * check will be performed by this method.
        * @param[in] voiceNumber Voice to place the Instrument in.
        * @param[in] rackNumber Rack to insert the Instrument into within the given
        *   voice.
        * @param[in] instrument The Instrument to insert. It should not be a null
        *   pointer.
        * @param[out] connectionPlanToComplete The ConnectionPlan to complete with
        *   bridging instructions.
        * @param[in, out] parameterRegistrationPlan The ParameterRegistrationPlan to
        *   use as is or complete with other registration instructions.
        */
        void addInstrumentAndPlanBridging(unsigned short voiceNumber, unsigned short rackNumber, const std::shared_ptr<Instrument>& instrument, ConnectionPlan& connectionPlanToComplete, ParameterRegistrationPlan& parameterRegistrationPlan);

        /**
        * Tries to find a Voice that is free, i.e. not currently playing anything,
        * in order to make it play some sound. Returns the valid voice number of an
        * empty voice if has found one, and an out-of-range number otherwise.
        */
        unsigned short findFreeVoice() const;

        /**
        * Turns the given Voice on. This method must only be called by the real-time
        * thread.
        * @param[in] voiceNumber Voice to turn on.
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Turns the given Voice off and sets it free. This method must only be
        * called by the real-time thread.
        * @param[in] voiceNumber Voice to turn off and set free.
        */
        void turnVoiceOffAndSetItFree(unsigned short voiceNumber);

        /**
        * Instructs the AudioWorkflow to take the given Voice, so it is not marked
        * as free anymore, and to make it play the given note. This method takes the
        * note, sends the note to play to the Voice, then resets every Instrument
        * located in the latter, and starts them all. Note that every parameter is
        * expected to be in-range, and that no safety check will be performed by
        * this method.
        * @param[in] voiceNumber Voice that should play the note.
        * @param[in] noteNumber Note number to send to the Voice.
        * @param[in] velocity Velocity to play the note with.
        */
        void takeVoiceAndPlayNote(unsigned short voiceNumber, unsigned char noteNumber, unsigned char noteVelocity);

        /**
        * Returns true if the given Voice is on and playing the given \p noteNumber,
        * and false otherwise. Note that \p voiceNumber is expected to be in-range,
        * and that, consequently, no safety check will be performed by this method.
        * @param[in] voiceNumber Voice to test.
        * @param[in] noteNumber Note to look for in the given Voice.
        */
        bool playsNoteNumber(unsigned short voiceNumber, unsigned char noteNumber) const;

        /**
        * Requests the Mixer to turn the given Rack on and use it in the mix. This
        * method must only be called by the real-time thread.
        * @param[in] rackNumber Rack to turn on.
        */
        void turnRackOn(unsigned short rackNumber);

        /**
        * Requests the Mixer to turn the given Rack off and stop using it in the
        * mix. This method must only be called by the real-time thread.
        * @param[in] rackNumber Rack to turn off.
        */
        void turnRackOff(unsigned short rackNumber);

        /**
        * Requests all the instruments contained in the given Voice to stop playing,
        * and returns the Voice's audio tail duration in samples, which is the
        * number of samples before the Voice begins to emit complete silence.
        * This method must only be called by the real-time thread.
        * @param[in] voiceNumber Voice that should stop playing.
        */
        uint32_t stopVoice(unsigned short voiceNumber);

        /**
        * Executes the given \p plan, and adds or removes entries from the
        * AudioWorkflow's parameter registers as instructed. This method needs to be
        * fast as it might be called by the real-time thread at the beginning of any
        * rendering session. Note that the plan passed in as argument may be altered
        * when executed, as the AudioWorkflow will move some shared pointers around.
        * @param[in] plan The ParameterRegistrationPlan to execute.
        */
        void executeParameterRegistrationPlan(ParameterRegistrationPlan& plan);

        /**
        * Tries to find the ParameterGenerator corresponding to the given Parameter.
        * The AudioWorkflow will search through the parameters that are registered
        * for the given \p rackNumber and look for a matching identifier. If one is
        * found, this method will return the associated ParameterGenerator.
        * Otherwise, this method will return a null pointer. Note that the rack
        * number is expected to be in-range, as no safety check will be performed by
        * this method.
        * @param[in] rackNumber The Rack number. It must be in-range.
        * @param[in] parameterIdentifier The Parameter's identifier. This can be
        *   passed in as a C string, as it will be implicitely converted into a
        *   StringView. If this parameter does not correspond to any parameter of
        *   the Instrument located at \p rackNumber, then this method will return a
        *   null pointer.
        */
        std::shared_ptr<ParameterGenerator> findParameterGenerator(unsigned short rackNumber, StringView parameterIdentifier);

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
        * Retrieve the ID of the Stream containing the reciprocal of the current
        * sample rate in the AudioWorkflow's global context.
        */
        uint32_t getSampleRateReciprocalStreamID() const;

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
        std::shared_ptr<Exporter> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
        Voice m_voices[ANGLECORE_NUM_VOICES];
        GlobalContext m_globalContext;
        ParameterRegister m_parameterRegisters[ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE];
    };
}