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

#include "Request.h"
#include "ConnectionRequest.h"
#include "InstrumentRequest.h"
#include "../../dependencies/farbot/fifo.h"

namespace ANGLECORE
{
    /**
    * \struct SynchronousQueueCenter SynchronousQueueCenter.h
    * A SynchronousQueueCenter is a Request container for synchronous transfer. It
    * contains type-specific Request queues that a RequestManager can write into and
    * that the real-time thread can read. It is thread-safe, so multiple threads can
    * concurrently write into the center's internal queues through a RequestManager,
    * while the real-time thread retrieves requests from those same queues.
    * .
    * Note that a SynchronousQueueCenter only processes certain types of request,
    * namely ConnectionRequest and InstrumentRequest. Other types of request, such
    * as ParameterChangeRequest, should not be sent to a SynchronousQueueCenter and
    * should be sent directly to the real-time thread instead, as they were meant to
    * be. Requests of unsupported types will be ignored and thrown away if sent to a
    * SynchronousQueueCenter.
    */
    struct SynchronousQueueCenter
    {
        template <class RequestType>
        using RequestQueue = farbot::fifo<
            std::shared_ptr<RequestType>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::multiple,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        >;

        RequestQueue<ConnectionRequest> connectionRequests;
        RequestQueue<InstrumentRequest> instrumentRequests;

        SynchronousQueueCenter();

        /**
        * Posts the given Request into the proper queue according to the Request's
        * type, which is dynamically identified at runtime. Request of unsupported
        * types will be ignored and thrown away. Supported types are
        * ConnectionRequest and InstrumentRequest.
        * .
        * Note that the pointer \p request passed in argument will be moved
        * according to the C++ move semantics, so it will become empty once this
        * method is called.
        * @param[in] request The Request to be posted in the SynchronousQueueCenter
        *   for the real-time thread to read.
        */
        void push(std::shared_ptr<Request>&& request);
    };
}