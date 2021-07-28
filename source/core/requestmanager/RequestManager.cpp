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

#include <thread>
#include <chrono>

#include "RequestManager.h"

/*
* Small time duration, in milliseconds, used to pause the asynchronous posting
* thread.
*/
#define ANGLECORE_REQUESTMANAGER_TIMER_DURATION 50

/*
* Maximum number of posted requests that the RequestManager will be able to handle
* at once.
*/
#define ANGLECORE_REQUESTMANAGER_QUEUE_SIZE 64

namespace ANGLECORE
{
    /* AsynchronousPostingThread
    ***************************************************/

    RequestManager::AsynchronousPostingThread::AsynchronousPostingThread(RequestQueue& asynchronousQueue, RequestQueue& synchronousQueue) :
        Thread(),
        m_asynchronousQueue(asynchronousQueue),
        m_synchronousQueue(synchronousQueue)
    {}

    void RequestManager::AsynchronousPostingThread::run()
    {
        std::shared_ptr<Request> request;

        while (!shouldStop())
        {
            /* Have we received any request in the asynchronous queue? ... */
            while (!shouldStop() && m_asynchronousQueue.pop(request) && request)
            {
                /*
                * ... YES! So we need to prepare and send that request to the
                * real-time thread through the synchronous queue.
                */

                /* We start by preprocessing the request */
                bool preprocessingSuccess = request->preprocess();
                request->hasBeenPreprocessed.store(true);

                /*
                * Did the preprocessing go well? We only post the request if the
                * preparation succeeded...
                */
                if (preprocessingSuccess)
                {

                    /*
                    * ... Yes, the preparation went well, so we can send the request
                    * to the real-time thread. To keep track of the request's status
                    * and wait for its complete execution, we do not pass the
                    * request straight to the real-time thread. Instead, we create a
                    * copy and send that copy to the latter.
                    */

                    /* We copy the request... */
                    std::shared_ptr<Request> requestCopy = request;

                    /* ... And post the copy: */
                    m_synchronousQueue.push(std::move(requestCopy));

                    /*
                    * From now on, the request is in the hands of the real-time
                    * thread, which at some point will send it back to the
                    * RequestManager on its PostProcessingThread. We cannot access
                    * any member of "request" except the boolean flags intended to
                    * provide information to non real-time threads. We use the
                    * "hasBeenPostprocessed" atomic boolean to detect when the
                    * PostProcessingThread receives the request from the real-time
                    * thread. In case the request is postprocessed but for some
                    * reason that boolean is never set to true, we add an extra
                    * layer of verification by checking the pointer's reference
                    * count (although it is not guaranteed to be safe by the
                    * standard). If that number is no longer greater than 1, then it
                    * means the PostProcessingThread has destroyed the copy of the
                    * shared pointer, which means we are done with the request.
                    */

                    while (!shouldStop() && !request->hasBeenPostprocessed.load() && request.use_count() > 1)
                        std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));

                    /*
                    * Once here, provided the current thread has not been instructed
                    * to stop (so shouldStop() should return false), we know the
                    * real-time thread has processed the request and sent it back to
                    * the RequestManager, but we do not know which indicator among
                    * the two above (the atomic boolean hasBeenPostprocessed and the
                    * reference count) has led us to that conclusion. In any case,
                    * we know the request is in the hand of a non real-time thread,
                    * be it the current AsynchronousPostingThread or the
                    * PostProcessingThread, so we know it is safe to delete the
                    * shared pointer that holds the request: il will not cause the
                    * real-time thread to perform any memory deallocation.
                    */
                }
                else
                {
                    request->postprocess();
                    request->hasBeenPostprocessed.store(true);
                }
            }

            /*
            * In case there are no requests in the asynchronous queue, or once we
            * finish taking care of all requests in that queue, we simply wait for a
            * small amount of time and check again later.
            */
            std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));
        }
    }

    /* PostProcessingThread
    ***************************************************/

    RequestManager::PostProcessingThread::PostProcessingThread(RequestQueue& processedRequests) :
        Thread(),
        m_processedRequests(processedRequests)
    {}

    void RequestManager::PostProcessingThread::run()
    {
        std::shared_ptr<Request> request;

        while (!shouldStop())
        {
            /* Have we received any processed request? ... */
            while (!shouldStop() && m_processedRequests.pop(request) && request)
            {
                /*
                * ... YES! So we need to post-process that request and then delete
                * it. Note that the deletion of an asynchronously posted request may
                * be performed by either the AsynchronousPostingThread or the
                * PostProcessingThread, depending on how fast the
                * AsynchronousPostingThread detects that the "hasBeenPostprocessed"
                * flag has been set to true.
                */

                request->postprocess();
                request->hasBeenPostprocessed.store(true);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));
        }
    }

    /* RequestManager
    ***************************************************/

    RequestManager::RequestManager() :
        m_synchronousQueue(ANGLECORE_REQUESTMANAGER_QUEUE_SIZE),
        m_asynchronousQueue(ANGLECORE_REQUESTMANAGER_QUEUE_SIZE),
        m_asynchronousPostingThread(m_asynchronousQueue, m_synchronousQueue),
        m_processedRequests(ANGLECORE_REQUESTMANAGER_QUEUE_SIZE),
        m_postProcessingThread(m_processedRequests)
    {
        /* We start the asynchronous posting thread */
        m_asynchronousPostingThread.start();

        /* We start the post-processing thread */
        m_postProcessingThread.start();
    }

    void RequestManager::postRequestSynchronously(std::shared_ptr<Request>&& request)
    {
        /* We first prepare the request */
        bool preprocessingSuccess = request->preprocess();
        request->hasBeenPreprocessed.store(true);

        /*
        * Did the preprocessing go well? We only post the request if the preparation
        * succeeded...
        */
        if (preprocessingSuccess)
        {
            /*
            * If the preparation succeeded, we then post the request to the
            * synchronous queue directly.
            */
            m_synchronousQueue.push(std::move(request));
        }
        else
        {
            request->postprocess();
            request->hasBeenPostprocessed.store(true);
        }
    }

    void RequestManager::postRequestAsynchronously(std::shared_ptr<Request>&& request)
    {
        /* We post the request to the asynchronous queue */
        m_asynchronousQueue.push(std::move(request));
    }

    bool RequestManager::popRequest(std::shared_ptr<Request>& result)
    {
        return m_synchronousQueue.pop(result);
    }

    void RequestManager::postProcessedRequest(std::shared_ptr<Request>&& request)
    {
        /* We post the request to the queue for processed requests */
        m_processedRequests.push(std::move(request));
    }
}
