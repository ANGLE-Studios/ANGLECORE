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

#include <algorithm>

#include "VoiceAssigner.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    /* VoiceAssignment
    ***************************************************/

    VoiceAssignment::VoiceAssignment(bool isNull, unsigned short voiceNumber) :
        isNull(isNull),
        voiceNumber(voiceNumber)
    {}

    /* VoiceAssigner
    ***************************************************/

    void VoiceAssigner::assignVoiceToWorker(unsigned short voiceNumber, uint32_t workerID)
    {
        /* We simply register a new pair in the parent voices map: */
        m_voiceAssignments[workerID] = voiceNumber;
    }

    void VoiceAssigner::revokeAssignments(uint32_t workerID)
    {
        /* We simply delete the related pair in the parent voices map: */
        m_voiceAssignments.erase(workerID);
    }

    std::vector<VoiceAssignment> VoiceAssigner::getVoiceAssignments(const std::vector<std::shared_ptr<Worker>>& workers) const
    {
        std::vector<VoiceAssignment> voiceAssignments;
        voiceAssignments.reserve(workers.size());

        /*
        * We assume the rendering sequence has been constructed from a workflow,
        * which implies it only contains non-null pointers.
        */
        for (const std::shared_ptr<const Worker>& worker : workers)
        {
            auto voiceNumberIterator = m_voiceAssignments.find(worker->id);
            if (voiceNumberIterator != m_voiceAssignments.end())

                /*
                * If the worker was assigned a voice, we append a matching
                * VoiceAssignment.
                */
                voiceAssignments.emplace_back(false, voiceNumberIterator->second);
            else

                /*
                * Otherwise, if the worker was not assigned any voice, we append
                * a null assignment.
                */
                voiceAssignments.emplace_back(true, 0);
        }
        return voiceAssignments;
    }
}