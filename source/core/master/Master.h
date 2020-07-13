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
#include <mutex>
#include <thread>
#include <chrono>
#include <utility>

#include "../audioworkflow/AudioWorkflow.h"
#include "../renderer/Renderer.h"
#include "MIDIBuffer.h"
#include "../audioworkflow/instrument/Instrument.h"
#include "../../config/RenderingConfig.h"
#include "../../config/AudioConfig.h"
#include "../requests/InstrumentRequest.h"
#include "../../utility/StringView.h"

namespace ANGLECORE
{
    /**
    * \class Master Master.h
    * Agent that orchestrates the rendering and the interaction with the end-user
    * requests. It should be the only entry point from outside when developping an
    * ANGLECORE-based application.
    */
    class Master
    {
    public:
        Master();

        /**
        * Sets the sample rate of the Master's AudioWorkflow.
        * @param[in] sampleRate The value of the sample rate, in Hz.
        */
        void setSampleRate(floating_type sampleRate);

        /**
        * Clears the Master's internal MIDIBuffer to prepare for rendering the next
        * audio block.
        */
        void clearMIDIBufferForNextAudioBlock();

        /**
        * Adds a new MIDIMessage at the end of the Master's internal MIDIBuffer, and
        * returns a reference to it. This method should only be called by the
        * real-time thread, right before calling the renderNextAudioBlock() method.
        */
        MIDIMessage& pushBackNewMIDIMessage();

        /**
        * Requests the Master to change one Parameter's value within the Instrument
        * positioned at the rack number \p rackNumber. Note that this does not mean
        * the request will take effect immediately: the Master will post the request
        * to the real-time thread to be taken care of in the next rendering session.
        * @param[in] rackNumber The Instrument's rack number. If this number is not
        *   valid, this method will have no effect.
        * @param[in] parameterIdentifier The Parameter's identifier. This can be
        *   passed in as a C string, as it will be implicitely converted into a
        *   StringView. If this parameter does not correspond to any parameter of
        *   the Instrument located at \p rackNumber, then this method will have no
        *   effect.
        * @param[in] newParameterValue The Parameter's new value.
        */
        void setParameterValue(unsigned short rackNumber, StringView parameterIdentifier, floating_type newParameterValue);

        /**
        * Renders the next audio block, using the internal MIDIBuffer as a source
        * of MIDI messages, and the provided parameters for computing and exporting
        * the output result.
        * @param[in] audioBlockToGenerate The memory location that will be used to
        *   output the audio data generated by the Renderer. This should correspond
        *   to an audio buffer of at least \p numChannels audio channels and \p
        *   numSamples audio samples.
        * @param[in] numChannels The number of channels available at the memory
        *   location \p audioBlockToGenerate. Note that ANGLECORE's engine may
        *   generate more channels than this number, and eventually merge them all
        *   to match that number.
        * @param[in] numSamples The number of samples to generate and write into
        *   \p audioBlockToGenerate.
        */
        void renderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples);

        template<class InstrumentType>
        bool addInstrument();

    protected:

        /**
        * \struct StopTracker Master.h
        * A StopTracker tracks a Voice's position while it stops playing and renders
        * its audio tail. This structure can store how close the Voice is to the end
        * of its tail in terms of remaining samples.
        */
        struct StopTracker
        {
            uint32_t stopDurationInSamples;
            uint32_t position;
        };

        /**
        * Renders the next audio block by splitting it into smaller audio chunks
        * that are all smaller than ANGLECORE's Stream fixed buffer size. This
        * method is the one that guarantees the Renderer only receives a valid
        * number of samples to render.
        * @param[in] audioBlockToGenerate The memory location that will be used to
        *   output the audio data generated by the Renderer. This should correspond
        *   to an audio buffer of at least \p numChannels audio channels and \p
        *   numSamples audio samples.
        * @param[in] numChannels The number of channels available at the memory
        *   location \p audioBlockToGenerate. Note that ANGLECORE's engine may
        *   generate more channels than this number, and eventually merge them all
        *   to match that number.
        * @param[in] numSamples The number of samples to generate and write into
        *   \p audioBlockToGenerate.
        * @param[in] startSample The position within the \p audioBlockToGenerate to
        *   start writing from.
        */
        void splitAndRenderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples, uint32_t startSample);

        /** Processes the requests received in its internal queues. */
        void processRequests();

        /** Processes the given MIDIMessage. */
        void processMIDIMessage(const MIDIMessage& message);

        /**
        * Updates the Master's internal stop trackers corresponding to each Voice,
        * after \p numSamples were rendered. This method detects if it is necessary
        * to turn some voices off, and do so if needed.
        * @param[in] numSamples Number of samples that were just rendered, and that
        *   should be used to increment the stop trackers.
        */
        void updateStopTrackersAfterRendering(uint32_t numSamples);

    private:
        AudioWorkflow m_audioWorkflow;
        Renderer m_renderer;
        MIDIBuffer m_midiBuffer;

        /** Queue for pushing and receiving instrument requests. */
        farbot::fifo<
            std::shared_ptr<InstrumentRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_instrumentRequests;

        bool m_voiceIsStopping[ANGLECORE_NUM_VOICES];
        StopTracker m_stopTrackers[ANGLECORE_NUM_VOICES];
    };

    template<class InstrumentType>
    bool Master::addInstrument()
    {
        std::lock_guard<std::mutex> scopedLock(m_audioWorkflow.getLock());

        /* Can we insert a new instrument? We need to find an empty spot first */
        unsigned short emptyRackNumber = m_audioWorkflow.findEmptyRack();
        if (emptyRackNumber >= ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)

            /*
            * There is no empty spot, so we stop here and return false. We cannot
            * insert a new instrument to the workflow.
            */
            return false;

        /*
        * Otherwise, if we can insert a new instrument, then we need to prepare the
        * instrument's environment, as well as a new InstrumentRequest for bridging
        * the instrument with the real-time rendering pipeline.
        */

        std::shared_ptr<InstrumentRequest> request = std::make_shared<InstrumentRequest>(InstrumentRequest::Type::ADD, emptyRackNumber);
        ConnectionRequest& connectionRequest = request->connectionRequest;
        ConnectionPlan& connectionPlan = connectionRequest.plan;
        ParameterRegistrationPlan& parameterRegistrationPlan = request->parameterRegistrationPlan;

        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            /*
            * We create an Instrument of the given type, and then cast it to an
            * Instrument, to ensure type validity.
            */
            std::shared_ptr<Instrument> instrument = std::make_shared<InstrumentType>();

            /*
            * Then, we insert the Instrument into the Workflow and plan its bridging
            * to the real-time rendering pipeline.
            */
            m_audioWorkflow.addInstrumentAndPlanBridging(v, emptyRackNumber, instrument, connectionPlan, parameterRegistrationPlan);
        }

        /*
        * Once here, we have a ConnectionPlan and a ParameterRegisterPlan ready to
        * be used in our InstrumentRequest. We now need to precompute the
        * consequences of executing the ConnectionPlan and connecting all of the
        * instruments to the AudioWorkflow's real-time rendering pipeline.
        */

        /*
        * We first calculate the rendering sequence that will take effect right
        * after the connection plan is executed.
        */
        std::vector<std::shared_ptr<Worker>> newRenderingSequence = m_audioWorkflow.buildRenderingSequence(connectionPlan);

        /*
        * And from that sequence, we can precompute and assign the rest of the
        * request properties:
        */
        connectionRequest.newRenderingSequence = newRenderingSequence;
        connectionRequest.newVoiceAssignments = m_audioWorkflow.getVoiceAssignments(newRenderingSequence);
        connectionRequest.oneIncrements.resize(newRenderingSequence.size(), 1);

        /*
        * Finally, we need to send the InstrumentRequest to the Renderer. To avoid
        * any memory deallocation by the real-time thread after it is done with the
        * request, we do not pass the request straight to the Renderer. Instead, we
        * create a copy and send that copy to the latter. Therefore, when the
        * Renderer is done with the request, it will only delete a copy of a shared
        * pointer and decrement its reference count by one, signaling to the Master
        * the request has been processed (and either succeeded or failed), and that
        * it can be safely destroyed by the non real-time thread that created it.
        */

        /* We copy the InstrumentRequest... */
        std::shared_ptr<InstrumentRequest> requestCopy = request;

        /* ... And post the copy:*/
        m_instrumentRequests.push(std::move(requestCopy));

        /*
        * From now on, the InstrumentRequest is in the hands of the real-time
        * thread. We cannot access any member of 'request'. We will still use the
        * reference count of the shared pointer as an indicator of when the
        * real-time thread is done with the request (although it is not guaranteed
        * to be safe by the standard). As long as that number is greater than 1
        * (and, normally, equal to 2), the real-time thread is still in possession
        * of the copy, and possibly processing it. So the non real-time thread
        * should wait. When this number reaches 1, it means the real-time thread is
        * done with the request and the non real-time thread can safely delete it.
        */

        /*
        * To avoid infinite loops and therefore deadlocks while waiting for the
        * real-time thread, we introduce a timeout, using a number of attempts.
        */
        const unsigned short timeoutAttempts = 4;
        unsigned short attempt = 0;

        /*
        * We then wait for the real-time thread to finish, or for when we reach the
        * timeout.
        */
        while (request.use_count() > 1 && attempt++ < timeoutAttempts)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

        /*
        * Once here, we either reached the timeout, or the copy of the
        * InstrumentRequest has been destroyed, and only the original remains, and
        * can be safely deleted. In any case, the original request will be deleted,
        * so if the timeout is the reason for leaving the loop, then the copy will
        * outlive the original request in the real-time thread, which will trigger
        * memory deallocation upon destruction, and possibly provoke an audio glitch
        * (this should actually be very rare). If we left the loop because the copy
        * was destroyed (which is the common case), then the original will be safely
        * deleted here by the non real-time thread.
        */

        /*
        * We access the 'hasBeenSuccessfullyProcessed' variable that may have been
        * edited by the real-time thread, in order to determine whether or not the
        * request was been successfully processed.
        */
        return request->connectionRequest.hasBeenSuccessfullyProcessed.load();
    }
}