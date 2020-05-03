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

#include "../audioworkflow/workflow/Workflow.h"
#include "../audioworkflow/voiceassigner/VoiceAssigner.h"
#include "../../config/RenderingConfig.h"
#include "../../dependencies/farbot/fifo.h"
#include "../requests/ConnectionRequest.h"

namespace ANGLECORE
{
    /**
    * \class Renderer Renderer.h
    * Agent responsible for actually performing the rendering of an audio block, by
    * calling the proper workers in its rendering sequence.
    */
    class Renderer
    {
    public:

        /**
        * Creates a Renderer with an empty rendering sequence. The Renderer will not
        * be ready to render anything upon creation: it will wait to be initialized
        * with a ConnectionRequest.
        * @param[in] workflow The Workflow to execute connection requests on
        */
        Renderer(Workflow& workflow);

        /**
        * Renders the given number of samples. This method constitutes the kernel of
        * ANGLECORE's real-time rendering pipeline. It will be repeatedly called by
        * the real-time thread, and is implemented to be the fastest possible.
        * @param[in] numSamplesToRender Number of samples to render. Every Worker in
        *   the rendering sequence that should be rendered will be instructed to
        *   render this number of samples.
        */
        void render(unsigned int numSamplesToRender);

        /**
        * Instructs the Renderer to turn a Voice on, and to recompute its increments
        * later, before rendering the next audio block. This method is really fast,
        * as it will only be called by the real-time thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Instructs the Renderer to turn a Voice off, and to recompute its
        * increments later, before rendering the next audio block. This method is
        * really fast, as it will only be called by the real-time thread.
        * @param[in] voiceNumber Number identifying the Voice to turn off
        */
        void turnVoiceOff(unsigned short voiceNumber);

        /**
        * Pushes the given request to the request queue. This method uses move
        * semantics, so it will take ownership of the pointer passed as argument. It
        * will never be called by the real-time thread, and will only be called by
        * the non real-time thread upon user request.
        * @param[in] request The ConnectionRequest to post to the Renderer. It will
        *   not be processed immediately, but before rendering the next audio block.
        */
        void postConnectionRequest(std::shared_ptr<ConnectionRequest>&& request);

    private:

        /**
        * Recomputes the Renderer's increments after a Voice has been turned on or
        * off, or after a ConnectionRequest has been processed. This method is fast,
        * as it will be called by the real-time thread.
        */
        void updateIncrements();

        /**
        * The Workflow the Renderer is working with. When processing connection
        * requests, this is the Workflow on which the request will be executed.
        */
        Workflow& m_workflow;

        /**
        * Boolean flag which signals when the rendering sequence and voice
        * assignments are valid, i.e. when they are both non-empty and of the same
        * size. It is initialized to false, and the Renderer expects an empty
        * ConnectionRequest to start up. Once set to true, this boolean will never
        * be set to false again, as only valid connection requests will be accepted.
        */
        bool m_isReadyToRender;

        /**
        * The current rendering sequence used by the renderer. It should always be
        * of the same length as m_voiceAssignments and m_increments.
        */
        std::vector<std::shared_ptr<Worker>> m_renderingSequence;

        /**
        * The voice assignments corresponding to each Worker in the current
        * rendering sequence. It should always be of the same length as
        * m_renderingSequence and m_increments.
        */
        std::vector<VoiceAssignment> m_voiceAssignments;

        /**
        * Position to start from in the rendering sequence. This may vary depending
        * on which Voice is on and off, and depending on the Workflow's connections.
        */
        uint32_t m_start;

        /**
        * Jumps to perform in the rendering sequence, in order to avoid Workers that
        * should not be called. It should always be of the same length as
        * m_renderingSequence and m_voiceAssignments.
        */
        std::vector<uint32_t> m_increments;

        /** Tracks the on/off status of every Voice */
        bool m_voiceIsOn[ANGLECORE_NUM_VOICES];
        
        /**
        * Flag that indicates whether to recompute the increments or not in the next
        * rendering session. This flag is useful for preventing the Renderer from
        * updating its increments twice, after a Voice has been turned on or off and
        * a new ConnectionRequest has been received.
        */
        bool m_shouldUpdateIncrements;

        /** Queue for receiving connection requests. */
        farbot::fifo<
            std::shared_ptr<ConnectionRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_connectionRequests;
    };
}