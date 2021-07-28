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

#include "Thread.h"

/*
* Small time duration, in milliseconds, used to pause the calling thread when
* spinning and waiting for the internal thread to terminate.
*/
#define ANGLECORE_THREAD_WAITING_LOOP_DURATION 50

namespace ANGLECORE
{
    Thread::Thread()
    {
        m_shouldStop.store(false);

        /*
        * By default, we initialize m_hasStopped to true, so that if for some reason
        * the Thread object is destroyed without the start() method being called
        * beforehand, the destructor can safely and properly exit.
        */
        m_hasStopped.store(true);
    }

    Thread::~Thread()
    {
        /* We instruct the thread to stop: */
        stop();

        /* And we wait for the thread to effectively stop... */
        while (!m_hasStopped.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(ANGLECORE_THREAD_WAITING_LOOP_DURATION));
    }

    void Thread::start()
    {
        std::thread thread(&Thread::callback, this);
        thread.detach();
    }

    void Thread::stop()
    {
        m_shouldStop.store(true);
    }

    void Thread::callback()
    {
        /* The thread has just started, so we set the m_hasStopped flag to false: */
        m_hasStopped.store(false);

        run();

        /*
        * If we arrive here, it means the thread is being stopped, and we stopped
        * processing requests. Therefore, we need to indicate the Thread handler
        * that it can delete its resources if needed, using the atomic boolean flag
        * "m_hasStopped":
        */
        m_hasStopped.store(true);
    }

    bool Thread::shouldStop() const
    {
        return m_shouldStop.load();
    }
}