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
#include "../../dependencies/farbot/fifo.h"
#include "../../utility/Thread.h"

namespace ANGLECORE
{
    /**
    * \class RequestManager RequestManager.h
    * A RequestManager handles requests before they are sent to the real-time thread
    * to be processed. It can either pass requests directly to the real-time thread,
    * or instead queue requests into a waiting line so that only one request at a
    * time is processed. Requests that fall into the first category are referred to
    * as "synchronously" posted requests, while others are "asynchronously" posted.
    * 
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
        * Creates a RequestManager, and launches two non real-time threads: one
        * AsynchronousPostingThread for handling asynchronously posted requests, and
        * one PostProcessingThread for receiving requests from the real-time thread
        * and postprocessing them.
        */
        RequestManager();

        /**
        * Takes the Request passed in argument and directly pushes it into a queue
        * for the real-time thread to read. Note that this method does not provide
        * any guarantee that the request will be executed instantly: the request
        * will be merely synchronously transferred to the real-time thread. The
        * latter will process it as soon as it can.
        * 
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
        * 
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
        * Tries to retrieve one Request if available for the real-time thread to
        * handle. If a Request is effectively available, then this method will
        * return true, and the available request will be moved from its container
        * into the \p result pointer reference passed in argument. Otherwise, this
        * method will return false, and the argument \p result will be left
        * untouched.
        * 
        * This method must only be called by the real-time thread.
        * @param[out] result A valid pointer to a Request if available for the
        *   real-time thread to read, and the same pointer object as passed in
        *   argument otherwise.
        */
        bool popRequest(std::shared_ptr<Request>& result);

        /**
        * Takes the Request passed in argument and sends it to the non real-time
        * PostProcessingThread for final processing and deletion. The Request passed
        * in argument must have already been processed, that is its process() method
        * should have been called before.
        * 
        * Note that the pointer \p request passed in argument will be moved
        * according to the C++ move semantics, so it will become empty once this
        * method is called.
        * @param[in] request The processed Request, on which the process() method
        *   must have been called before, to be posted to the non real-time
        *   PostProcessingThread.
        */
        void postProcessedRequest(std::shared_ptr<Request>&& request);

    protected:

        typedef farbot::fifo<
            std::shared_ptr<Request>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::multiple,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > RequestQueue;

        /**
        * \class AsynchronousPostingThread RequestManager.h
        * An AsynchronousPostingThread is a thread handler that manages
        * asynchronously posted requests. It provides control over the non real-time
        * thread that receives and sends requests asynchronously through a waiting
        * line. It is this object that is responsible for processing only one
        * Request at a time.
        */
        class AsynchronousPostingThread :
            public Thread
        {
        public:

            /**
            * Creates an thread handler for asynchronously posting requests, and
            * stores references of the request queues that the thread will
            * manipulate, but does not effectively spawn the thread. The start()
            * method must be called for that to happen.
            * @param[in] asynchronousQueue A reference to the RequestQueue where to
            *   pick requests from for sending them to the real-time thread.
            * @param[in] synchronousQueue A reference to the RequestQueue where to
            *   push requests into for the real-time thread.
            */
            AsynchronousPostingThread(RequestQueue& asynchronousQueue, RequestQueue& synchronousQueue);

        protected:

            /**
            * Core routine of the non real-time thread that handles asynchronously
            * posted requests.
            */
            void run();

        private:
            RequestQueue& m_asynchronousQueue;
            RequestQueue& m_synchronousQueue;
        };

        /**
        * \class PostProcessingThread RequestManager.h
        * A PostProcessingThread is a thread handler that receives requests from the
        * real-time thread after they are processed. It provides control over the
        * non real-time thread that handles these processed requests and call their
        * postprocess() method before deleting them.
        */
        class PostProcessingThread :
            public Thread
        {
        public:

            /**
            * Creates an thread handler for processed requests, and stores
            * references of the request queue that will receive those requests from
            * the real-time thread, but does not effectively spawn the thread. The
            * start() method must be called for that to happen.
            * @param[in] processedRequests A reference to the RequestQueue where to
            *   pick requests from for postprocessing.
            */
            PostProcessingThread(RequestQueue& processedRequests);

        protected:

            /**
            * Core routine of the non real-time thread that handles processed
            * requests.
            */
            void run();

        private:
            RequestQueue& m_processedRequests;
        };

    private:

        /** Queues for pushing synchronously posted requests. */
        RequestQueue m_synchronousQueue;

        /** Queue for pushing and retrieving asynchronously posted requests. */
        RequestQueue m_asynchronousQueue;

        AsynchronousPostingThread m_asynchronousPostingThread;

        /** Queues for already processed requests. */
        RequestQueue m_processedRequests;

        PostProcessingThread m_postProcessingThread;
    };
}