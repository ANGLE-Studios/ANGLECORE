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

#include <stdint.h>
#include <memory>
#include <vector>
#include <unordered_map>

#include "../audioworkflow/workflow/Worker.h"

namespace ANGLECORE
{
    /**
    * \struct VoiceAssignment VoiceAssigner.h
    * Represents an item in a voice sequence.
    */
    struct VoiceAssignment
    {
        bool isNull;
        unsigned short voiceNumber;

        VoiceAssignment(bool isNull, unsigned short voiceNumber);
    };

    /**
    * \class VoiceAssigner VoiceAssigner.h
    * Entity that assigns workers to voices, and keep track of its assingments
    * internally.
    */
    class VoiceAssigner
    {
    public:

        /**
        * Maps a Worker to a Voice, so that the Worker will only be called if the
        * Voice is on at runtime. If a Worker is not part of any Voice, it will be
        * considered global, and will always be rendered. Note that the
        * \p voiceNumber is expected to be in-range. No safety checks will be
        * performed, mostly to increase speed.
        * @param[in] voiceNumber Unique number that identifies the Voice \p worker
        *   should be mapped to
        * @param[in] workerID ID of the Worker to map
        */
        void assignVoiceToWorker(unsigned short voiceNumber, uint32_t workerID);

        /**
        * Revokes every assignment the given Worker was related to. If the Worker
        * was not assigned a Voice, this method will simply return without any side
        * effect.
        * @param[in] workerID ID of the Worker to unmap
        */
        void revokeAssignments(uint32_t workerID);

        /**
        * Returns the voices assigned to each Worker in the \p workers vector. The
        * vector returned will be of the same size as the input vector. If a Worker
        * was not assigned any Voice, its corresponding VoiceAssignment will be
        * null. Note that this method relies on move semantics for efficiency.
        * @param[in] workers Vector of workers to retrieve the assigned voices from.
        *   The vector should not contain any null pointer, and should preferably be
        *   the rendering sequence of an AudioWorkflow.
        */
        std::vector<VoiceAssignment> getVoiceAssignments(const std::vector<std::shared_ptr<Worker>>& workers) const;

    private:

        /**
        * Maps a Worker to its assigned Voice, if relevant. Workers are referred to
        * with their ID, and voices with their number.
        */
        std::unordered_map<uint32_t, unsigned short> m_voiceAssignments;
    };
}