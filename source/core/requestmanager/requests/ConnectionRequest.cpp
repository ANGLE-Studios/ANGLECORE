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

#include "ConnectionRequest.h"
#include "../../renderer/Renderer.h"

namespace ANGLECORE
{
    ConnectionRequest::ConnectionRequest(AudioWorkflow& audioWorkflow, Renderer& renderer) :
        Request(),
        m_audioWorkflow(audioWorkflow),
        m_renderer(renderer)
    {}

    void ConnectionRequest::process()
    {
        /* We first test if the connection request is valid... */
        uint32_t size = newRenderingSequence.size();
        if (size > 0 && newVoiceAssignments.size() == size && oneIncrements.size() == size)
        {
            /* The request is valid, so we execute the ConnectionPlan... */
            bool successAfterExecution = m_audioWorkflow.executeConnectionPlan(plan);

            /*
            * ... And we trace if the ConnectionRequest's execution was a success,
            * using the "success" boolean flag. This flag will equal true if the
            * request has been successfully executed by the AudioWorkflow. It will
            * equal false if the request is not valid (its argument do not verify
            * the two properties described in the ConnectionRequest documentation)
            * and was therefore ignored by the Master, or if at least one of the
            * ConnectionInstruction failed when the ConnectionPlan was executed.
            */
            success.store(successAfterExecution);

            /*
            * If the connection request has failed, it could mean the
            * connection plan has only been partially executed, in which
            * case we will still want the renderer to update its internal
            * rendering sequence, so we call the processConnectionRequest()
            * method on the renderer, independently from the value of the
            * 'success' boolean.
            */
            m_renderer.processConnectionRequest(*this);
        }
    }
}