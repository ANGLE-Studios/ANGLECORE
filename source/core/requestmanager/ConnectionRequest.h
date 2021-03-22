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
#include <atomic>

#include "Request.h"
#include "../audioworkflow/workflow/ConnectionPlan.h"
#include "../audioworkflow/workflow/Worker.h"
#include "../audioworkflow/voiceassigner/VoiceAssigner.h"

namespace ANGLECORE
{
    /**
    * \struct ConnectionRequest ConnectionRequest.h
    * Request to execute a ConnectionPlan on a Workflow. A ConnectionRequest
    * contains both the ConnectionPlan and its consequences (the new rendering
    * sequence and voice assignments after the plan is executed), which should be
    * computed in advance for the Renderer. To be valid, a ConnectionRequest should
    * verify the following two properties:
    * 1. None of its three vectors newRenderingSequence, newVoiceAssignments, and
    * oneIncrements should be empty;
    * 2. All of those three vectors should be of the same length.
    * .
    * To be consistent, both vectors newRenderingSequence and newVoiceAssignments
    * should be computed from the same ConnectionPlan and by the same AudioWorkflow.
    */
    struct ConnectionRequest :
        public Request
    {
        ConnectionPlan plan;
        std::vector<std::shared_ptr<Worker>> newRenderingSequence;
        std::vector<VoiceAssignment> newVoiceAssignments;

        /**
        * This vector provides pre-allocated memory for the Renderer's increment
        * computation. It should always be of the same size as newRenderingSequence
        * and newVoiceAssignments, and should always end with the number 1.
        */
        std::vector<uint32_t> oneIncrements;

        ConnectionRequest();
    };
}