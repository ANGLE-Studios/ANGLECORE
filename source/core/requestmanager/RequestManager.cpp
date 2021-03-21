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
* Small time duration, in milliseconds, used to pause the asynchronous thread.
*/
#define ANGLECORE_REQUESTMANAGER_TIMER_DURATION 50

/*
* Maximum number of asynchronously posted requests that the RequestManager will be
* able to handle at once.
*/
#define ANGLECORE_REQUESTMANAGER_QUEUE_SIZE 64

namespace ANGLECORE
{
    /* AsynchronousThread
    ***************************************************/

    RequestManager::AsynchronousThread::AsynchronousThread(RequestQueue& asynchronousQueue, SynchronousQueueCenter& synchronousQueueCenter) :
        m_asynchronousQueue(asynchronousQueue),
        m_synchronousQueueCenter(synchronousQueueCenter)
    {
        m_shouldStop.store(false);

        /*
        * By default, we initialize m_hasStopped to true, so that if for some reason
        * the AsynchronousThread object is destroyed without the start() method
        * being called beforehand, the destructor can safely and properly exit.
        */
        m_hasStopped.store(true);
    }

    RequestManager::AsynchronousThread::~AsynchronousThread()
    {
        /* We instruct the thread to stop: */
        stop();

        /* And we wait for the thread to effectively stop... */
        while (!m_hasStopped.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));
    }

    void RequestManager::AsynchronousThread::start()
    {
        std::thread thread(&AsynchronousThread::run, this);
        thread.detach();
    }

    void RequestManager::AsynchronousThread::stop()
    {
        m_shouldStop.store(true);
    }

    void RequestManager::AsynchronousThread::run()
    {
        /* The thread has just started, so we set the m_hasStopped flag to false: */
        m_hasStopped.store(false);

        std::shared_ptr<Request> request;

        while (!m_shouldStop.load())
        {
            /* Have we received any request in the asynchronous queue? ... */
            while (!m_shouldStop.load() && m_asynchronousQueue.pop(request) && request)
            {
                /*
                * ... YES! So we need to send that request to the real-time thread
                * through the synchronous queue. To avoid any memory deallocation by
                * the real-time thread after it is done with the request, we do not
                * pass the request straight to the real-time thread. Instead, we
                * create a copy and send that copy to the latter. Therefore, when
                * the real-time thread is done with the request, it will only delete
                * a copy of a shared pointer, and decrement its reference count by
                * one, signaling to the current non real-time thread the request has
                * been processed (and either succeeded or failed), and that it can
                * be safely destroyed by the non real-time thread that created it.
                */

                /* We copy the request... */
                std::shared_ptr<Request> requestCopy = request;

                /* ... And post the copy: */
                m_synchronousQueueCenter.push(std::move(requestCopy));

                /*
                * From now on, the request is in the hands of the real-time thread.
                * We cannot access any member of "request" except the boolean flags
                * intended to provide information to the non real-time thread. We
                * use the "hasBeenProcessed" atomic boolean to detect when the
                * real-time thread is done with the request. In case the request is
                * processed but for some reason that boolean is never set to true,
                * we add an extra layer of verification by checking the pointer's
                * reference count (although it is not guaranteed to be safe by the
                * standard). If that number is no longer greater than 1, then it
                * means the real-time thread has processed and destroyed the copy of
                * the shared pointer, which means we are done with the request.
                */

                while (!m_shouldStop.load() && !request->hasBeenProcessed.load() && request.use_count() > 1)
                    std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));

                /*
                * Once here, provided the current thread has not been instructed to
                * stop (so m_shouldStop is false), we know the real-time thread has
                * processed the request, but we do not know which indicator among
                * the two above (the atomic boolean hasBeenProcessed and the
                * reference count) has led us to that conclusion. To mitigate the
                * risk of the real-time thread performing some memory deallocation,
                * and assuming no extra copy of the request pointer was made
                * beforehand, we add an extra check on the pointer's reference
                * count: if that number is still greater than 1 (and, normally,
                * equal to 2), then it means the real-time thread has not destroyed
                * the copy yet, so we give it some extra time to delete the shared
                * pointer. If it is not greater than 1, then we can directly move on
                * and delete the shared pointer here, which will deallocate memory
                * on this thread rather than on the real-time thead. This technique
                * does not guarantee that no memory deallocation will occur on the
                * real-time thread, though, as we only give a limited amount of time
                * to the latter for deleting the pointer's copy. But such unwanted
                * event should be very rare, as deleting a copy of a shared pointer
                * is a fast operation.
                */
                if (request.use_count() > 1)
                    std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));
            }

            /*
            * In case there are no requests in the asynchronous queue, or once we
            * finish taking care of all requests in that queue, we simply wait for a
            * small amount of time and check again later.
            */
            std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_REQUESTMANAGER_TIMER_DURATION));
        }

        /*
        * If we arrive here, it means the thread is being stopped, and we stopped
        * processing requests. Therefore, we need to indicate the AsynchronousThread
        * handler that it can delete its resources if needed, using the atomic
        * boolean flag "m_hasStopped":
        */
        m_hasStopped.store(true);
    }

    /* RequestManager
    ***************************************************/

    RequestManager::RequestManager() :
        m_asynchronousQueue(ANGLECORE_REQUESTMANAGER_QUEUE_SIZE),
        m_asynchronousThread(m_asynchronousQueue, m_synchronousQueueCenter)
    {
        /* We start the asynchronous thread */
        m_asynchronousThread.start();
    }

    void RequestManager::postRequestSynchronously(std::shared_ptr<Request>&& request)
    {
        /* We post the request to the synchronous queue directly */
        m_synchronousQueueCenter.push(std::move(request));
    }

    void RequestManager::postRequestAsynchronously(std::shared_ptr<Request>&& request)
    {
        /* We post the request to the asynchronous queue */
        m_asynchronousQueue.push(std::move(request));
    }

    bool RequestManager::popConnectionRequest(std::shared_ptr<ConnectionRequest>& result)
    {
        return m_synchronousQueueCenter.connectionRequests.pop(result);
    }

    bool RequestManager::popInstrumentRequest(std::shared_ptr<InstrumentRequest>& result)
    {
        return m_synchronousQueueCenter.instrumentRequests.pop(result);
    }
}
