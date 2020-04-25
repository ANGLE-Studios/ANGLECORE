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

#include "Renderer.h"

namespace ANGLECORE
{
    Renderer::Renderer(Workflow& workflow) :
        m_workflow(workflow),
        m_isReadyToRender(false),
        m_start(0),
        m_shouldUpdateIncrements(false),

        /*
        * The request queue should only handle one request at a time, so there is no
        * need to reserve more space than one slot.
        */
        m_connectionRequests(1)
    {}

    void Renderer::render(unsigned int numSamplesToRender)
    {
        /*
        * This method should be the fastest possible, so it does as little checks as
        * it can. In particular, it does not check if the renderer is in its initial
        * state (with no rendering sequence), which means the code implemented here
        * should always be valid, even when the renderer has just been created, and
        * all the variables have been set to their initial value.
        */

        /*=============================
        * STEP 1/3: CONNECTION REQUESTS
        ===============================*/

        {
            /*
            * We define a scope here so that the following pointer is deleted as
            * soon as possible. This pointer will hold a copy of any
            * ConnectionRequest that has been sent by the Master to the Renderer.
            * When it is deleted, its reference count will decrement, and hereby
            * signal to the Master the ConnectionRequest has been processed.
            */
            std::shared_ptr<ConnectionRequest> request;

            /* Has a ConnectionRequest been received? ... */
            if (m_connectionRequests.pop(request) && request)
            {
                /*
                * ... YES! So we need to process the request first before doing any
                * rendering call. Note that null pointers are ignored.
                */

                /*
                * Once here, the variable 'request' should be the second and last
                * pointer to the current ConnectionRequest, together with the
                * Master's initial pointer. This is because the pop operation
                * actually moves the request into the variable 'request', avoiding
                * copies.
                */

                /* We only execute the request if it is valid, i.e. if it contains a
                * non-empty rendering sequence, and if its voice assigments and one
                * increments vector are of the same size as the sequence.
                */
                uint32_t size = request->newRenderingSequence.size();
                if (size > 0 && request->newVoiceAssignments.size() == size && request->oneIncrements.size() == size)
                {
                    /* The request is valid, so execute the ConnectionPlan... */
                    m_workflow.executeConnectionPlan(request->plan);

                    /*
                    * ... And then we move every vector from the request to the
                    * renderer
                    */
                    m_renderingSequence = std::move(request->newRenderingSequence);
                    m_voiceAssignments = std::move(request->newVoiceAssignments);
                    m_increments = std::move(request->oneIncrements);

                    /*
                    * Note that once here, after the move operations, the three
                    * vectors newRenderingSequence, newVoiceAssignments, and
                    * oneIncrements are in a valid but unspecified state. So the
                    * Master should never try to access these vectors once it has
                    * posted a ConnectionRequest. The Master can still access the
                    * hasBeenProcessed atomic variable though, and use it to detect
                    * when the real-time thread is done with the ConnectionRequest.
                    */

                    m_isReadyToRender = true;
                    m_shouldUpdateIncrements = true;
                }
            }

            /*
            * Once here, the 'request' pointer will be deleted, hereby decrementing
            * its reference count by one. Since connection requests are supposed to
            * be hold by the Master and copied before being sent to the Renderer,
            * no memory deallocation should occur here.
            */
        }
        
        /*===============================
        * STEP 2/3: UPDATE THE INCREMENTS
        =================================*/

        if (m_shouldUpdateIncrements)
        {
            updateIncrements();
            m_shouldUpdateIncrements = false;
        }

        /*=========================
        * STEP 3/3: RENDER SEQUENCE
        ===========================*/

        if (m_isReadyToRender)
            for (uint32_t i = m_start; i < m_renderingSequence.size(); i += m_increments[i])
                m_renderingSequence[i]->work(numSamplesToRender);
    }


    void Renderer::turnVoiceOn(unsigned short voiceNumber)
    {
        m_voiceIsOn[voiceNumber] = true;

        if (m_isReadyToRender)
            m_shouldUpdateIncrements = true;
    }

    void Renderer::turnVoiceOff(unsigned short voiceNumber)
    {
        m_voiceIsOn[voiceNumber] = false;

        if (m_isReadyToRender)
            m_shouldUpdateIncrements = true;
    }

    void Renderer::postConnectionRequest(std::shared_ptr<ConnectionRequest>&& request)
    {
        m_connectionRequests.push(std::move(request));
    }

    void Renderer::updateIncrements()
    {
        /*
        * We only enter this function if the renderer already works on a valid, non-
        * empty sequence to render. Therefore we are ensured m_renderingSequence,
        * m_voiceAssignments, and m_increments are all of equal length, and that
        * none is empty. So we do not need to make any out-of-range checks here.
        */

        /*
        * The algorithm used here is a fast, backward algorithm that computes the
        * number of jumps the renderer should make after calling a worker from the
        * rendering sequence. Note that the last value of m_increments is always
        * equal to 1, because m_increments is always initialized to a valid state
        * by capturing the content of a ConnectionRequest's oneIncrements vector.
        */

        for (uint32_t i = m_renderingSequence.size() - 1; i >= 1; i--)

            /*
            * We only render a worker if it is assigned to a voice that is on, or if
            * it was not assigned to any voice and is completely free. Therefore, we
            * test each voice assignment to see if it is null, or if it refers to a
            * voice which is currently playing. If it is, then the corresponding
            * worker should be called for rendering, so we fix an increment of 1
            * after the previous worker. Otherwise, we should skip the worker, so we
            * fix an increment that ensures we bypass the worker when traversing the
            * rendering sequence.
            */
            m_increments[i - 1] = m_voiceAssignments[i].isNull || m_voiceIsOn[m_voiceAssignments[i].voiceNumber] ? 1 : m_increments[i] + 1;

        /*
        * Finally, using the same test as before, we compute the start position
        * within the rendering sequence, which may vary depending on the voices'
        * on/off status. The start position can be 0, if the first worker should be
        * rendered. Otherwise, we use the precomputed increments to determine the
        * position of the first worker to render.
        */
        m_start = m_voiceAssignments[0].isNull || m_voiceIsOn[m_voiceAssignments[0].voiceNumber] ? 0 : m_increments.front();
    }
}