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

#include "SynchronousQueueCenter.h"

/*
* This is the maximum number of queued requests that the SynchronousQueueCenter will
* be able to handle between two rendering sessions.
*/
#define ANGLECORE_SYNCHRONOUSQUEUECENTER_SIZE 64

namespace ANGLECORE
{
    SynchronousQueueCenter::SynchronousQueueCenter() :
        connectionRequests(ANGLECORE_SYNCHRONOUSQUEUECENTER_SIZE),
        instrumentRequests(ANGLECORE_SYNCHRONOUSQUEUECENTER_SIZE)
    {}

    void SynchronousQueueCenter::push(std::shared_ptr<Request>&& request)
    {
        /*
        * We push the given request into the right queue, based on the request's
        * type. We use dynamic_pointer_cast to determine the request type, and we
        * ignore any request whose type does not match those supported in the
        * internal queues.
        */

        if (std::shared_ptr<ConnectionRequest> connectionRequest = std::dynamic_pointer_cast<ConnectionRequest>(request))
            connectionRequests.push(std::move(connectionRequest));
        else if (std::shared_ptr<InstrumentRequest> instrumentRequest = std::dynamic_pointer_cast<InstrumentRequest>(request))
            instrumentRequests.push(std::move(instrumentRequest));
    }
}
