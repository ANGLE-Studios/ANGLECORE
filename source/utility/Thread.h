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

#include <atomic>

namespace ANGLECORE
{
    /**
    * \class Thread Thread.h
    * The Thread class provides a handy interface for derived thread handler
    * classes.
    
    * The interface consists of mainly three methods: start(), run(), and
    * stop(). When the start() method of a Thread object is called, the latter
    * spawns a thread that will execute the run() method. The stop() method can then
    * be called to ask the internal thread to stop.
    *.
    * This class follows the RAII paradigm: when a Thread object is destroyed, the
    * stop() method is automatically called, and the thread that called the
    * destructor will wait for the internal thread to finish.
    * .
    * Note that the stop() method only makes a request and does not kill the
    * internal thread. More precisely, the stop() method updates an internal boolean
    * flag to make that request, and the flag's value can be easily accessed through
    * the thread-safe auxiliary shouldStop() method, which returns true if the
    * request was effectively made, and false otherwise.
    * .
    * The run() method is a pure virtual method, so it must be implemented in
    * derived classes. It must either terminate in finite time, or regularly call
    * the auxiliary shouldStop() method and ensure that, if shouldStop() returns
    * true at any time, then it stops in a finite and preferably short time. If the
    * run() method does not respect this rule, then the Thread destructor may enter
    * an infinite loop while waiting for the internal thread to finish, and
    * eventually crash the application.
    */
    class Thread
    {
    public:

        /**
        * Creates a Thread object, but does not effectively spawn the thread. The
        * start() method must be called for that to happen.
        */
        Thread();

        ~Thread();

        /**
        * Instructs to spawn and start a thread that will run the callback() method.
        */
        void start();

        /**
        * Instructs to stop the thread using an atomic boolean flag. The run()
        * method should check for that flag using the shouldStop() method. If the
        * shouldStop() method returns true, then the run method should terminate.
        */
        void stop();

    protected:
        void callback();
        bool shouldStop() const;

        /**
        * Core routine of the thread. If the run() method consists of a loop, it
        * should regularly check whether it should stop using the shouldStop()
        * method. If the shouldStop() method returns true at any point, then the
        * run() method should terminate.
        */
        virtual void run() = 0;

    private:
        std::atomic<bool> m_shouldStop;
        std::atomic<bool> m_hasStopped;
    };
}