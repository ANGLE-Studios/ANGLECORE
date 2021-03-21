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
#include <atomic>

#include "Request.h"
#include "SynchronousQueueCenter.h"

namespace ANGLECORE
{
    /**
    * \class RequestManager RequestManager.h
    * A RequestManager handles requests before they are sent to the real-time thread
    * to be processed. It can either pass requests directly to the real-time thread,
    * or instead queue requests into a waiting line so that only one request at a
    * time is processed. Requests that fall into the first category are referred to
    * as "synchronously" posted requests, while others are "asynchronously" posted.
    * .
    * Note that the term "synchronous" here qualifies a request's transfer and not
    * its execution. In other words, synchronously posted requests have no
    * guarantee to be executed instantly: they are merely synchronously transferred
    * to the real-time thread, which will then process it as soon as it can, and not
    * necessarily right upon reception. In contrast, "asynchronously" posted
    * requests are transferred to an intermediate, non real-time thread that ensures
    * all requests have been entirely executed before processing a new one.
    */
    class RequestManager
    {
    public:

        /**
        * Creates a RequestManager, and launches a non real-time thread to handle
        * asynchronous requests.
        */
        RequestManager();

        /**
        * Takes the Request passed in argument and directly pushes it into a queue
        * for the real-time thread to read. Note that this method does not provide
        * any guarantee that the request will be executed instantly: the request
        * will be merely synchronously transferred to the real-time thread. The
        * latter will process it as soon as it can.
        * .
        * Note that the pointer \p request passed in argument will be moved
        * according to the C++ move semantics, so it will become empty once this
        * method is called. It is highly recommended to gather all necessary
        * information on the Request before this method is called, and it is also
        * strongly advised to avoid trying to access the request pointer later by
        * keeping a copy of it. Such workaround would interfere with the pointer's
        * internal reference count, which is used by this method to prevent memory
        * deallocation from the real-time thread.
        * @param[in] request The Request to be posted synchronously to the real-time
        *   thread.
        */
        void postRequestSynchronously(std::shared_ptr<Request>&& request);

        /**
        * Takes the Request passed in argument and moves it into a waiting line for
        * later processing. The given request will only be processed once all its
        * predecessors already in the waiting line are executed by the real-time
        * thread. Although its synchronous counterpart introduces less delay before
        * processing, this method helps better mitigate risks of failure in the
        * request processing pipeline.
        * .
        * Note that the pointer \p request passed in argument will be moved
        * according to the C++ move semantics, so it will become empty once this
        * method is called. It is highly recommended to gather all necessary
        * information on the Request before this method is called, and it is also
        * strongly advised to avoid trying to access the request pointer later by
        * keeping a copy of it. Such workaround would interfere with the pointer's
        * internal reference count, which is used by this method to prevent memory
        * deallocation from the real-time thread.
        * @param[in] request The Request to be posted asynchronously to the
        *   real-time thread.
        */
        void postRequestAsynchronously(std::shared_ptr<Request>&& request);

        /**
        * Tries to retrieve one ConnectionRequest if available for the real-time
        * thread to handle. If a ConnectionRequest is effectively available, then
        * this method will return true, and the available request will be moved from
        * its container into the \p result pointer reference passed in argument.
        * Otherwise, this method will return false, and the argument \p result will
        * be left untouched.
        * .
        * This method must only be called by the real-time thread.
        * @param[out] result A valid pointer to a ConnectionRequest if available for
        *   the real-time thread to read, and the same pointer object as passed in
        *   argument otherwise.
        */
        bool popConnectionRequest(std::shared_ptr<ConnectionRequest>& result);

        /**
        * Tries to retrieve one InstrumentRequest if available for the real-time
        * thread to handle. If an InstrumentRequest is effectively available, then
        * this method will return true, and the available request will be moved from
        * its container into the \p result pointer reference passed in argument.
        * Otherwise, this method will return false, and the argument \p result will
        * be left untouched.
        * .
        * This method must only be called by the real-time thread.
        * @param[out] result A valid pointer to an InstrumentRequest if available
        *   for the real-time thread to read, and the same pointer object as passed
        *   in argument otherwise.
        */
        bool popInstrumentRequest(std::shared_ptr<InstrumentRequest>& result);

    protected:

        typedef farbot::fifo<
            std::shared_ptr<Request>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::multiple,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > RequestQueue;

        /**
        * \class AsynchronousThread RequestManager.h
        * An AsynchronousThread is a thread handler that manages asynchronously
        * posted requests. It provides control over the non real-time thread that
        * receives and sends requests asynchronously through a waiting line. It is
        * this object that is responsible for processing only one Request at a time.
        * .
        * This class follows the RAII paradigm: when an AsynchronousThread object is
        * destroyed, its underlying thread is automatically and safely stopped.
        */
        class AsynchronousThread
        {
        public:

            /**
            * Creates an AsynchronousThread handler, and stores references of the
            * request queues that the asynchronous thread will manipulate, but does
            * not effectively spawn the thread. The start() method must be called
            * for that to happen.
            * @param[in] asynchronousQueue A reference to the RequestQueue where to
            *   pick requests from for sending them to the real-time thread.
            * @param[in] synchronousQueueCenter A reference to the
            *   SynchronousQueueCenter where to push requests into for the real-time
            *   thread.
            */
            AsynchronousThread(RequestQueue& asynchronousQueue, SynchronousQueueCenter& synchronousQueueCenter);

            ~AsynchronousThread();

            /**
            * Instructs to spawn and start the non real-time thread that will handle
            * asynchronously posted requests.
            */
            void start();

            /**
            * Instructs to stop the non real-time thread that handles asynchronously
            * posted requests. This method guarantees that the thread will stop in
            * finite time, but in doing so, it may cause the thread to stop even if
            * a request is being processed by the real-time thread, thus living it
            * it into the hands of the latter.
            */
            void stop();

        protected:

            /**
            * Core routine of the non real-time thread that handles asynchronously
            * posted requests.
            */
            void run();

        private:
            RequestQueue& m_asynchronousQueue;
            SynchronousQueueCenter& m_synchronousQueueCenter;
            std::atomic<bool> m_shouldStop;
            std::atomic<bool> m_hasStopped;
        };

    private:

        /** Queues for pushing and receiving synchronous requests. */
        SynchronousQueueCenter m_synchronousQueueCenter;

        /** Queue for pushing and receiving asynchronous requests. */
        RequestQueue m_asynchronousQueue;

        AsynchronousThread m_asynchronousThread;
    };
}