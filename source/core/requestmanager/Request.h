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
    * \struct Request Request.h
    * When the end-user instructs to change something in the AudioWorkflow, an
    * instance of this structure is created to store all necessary information
    * about that request.
    * .
    * The Request structure provides two virtual methods that can be overriden in
    * derived classes: prepare() and process(). The prepare() method should take
    * preparation actions and must return a boolean indicating if the preparation
    * went well. It is always executed on a non real-time thread. The process()
    * method should contain the core implementation of the Request's execution.
    * It is a pure virtual method, so it must be implemented in all derived classes.
    * It is always executed on the real-time thread, so it must be really fast.
    * .
    * The prepare() method is guaranteed to always be executed before the process()
    * method, but the the prepare() method may be executed without the process()
    * method being called subsequently, typically when the preparation fails.
    * .
    * For two different requests both posted asynchronously to the RequestManager,
    * the prepare() method is guaranteed to always be executed separately and never
    * concurrently. That guarantee no longer holds for requests posted
    * synchronously: a request posted synchronously could be prepared in parallel
    * with another request posted synchronously, or with another request posted
    * asynchronously, since synchronous posting aims at preparing and transferring
    * requests upon reception. If concurrency is an issue, a proper locking or sharing
    * mechanism should be implemented in the prepare() method, or the Request in
    * question should be posted asynchronously.
    */
    struct Request
    {
        /**
        * Indicates whether or not the request was processed, regardless of the
        * success of its processing. This atomic boolean must always be set to true
        * once a Request is processed; otherwise, if the Request has been posted
        * asynchronously to the RequestManager, then the latter may not detect the
        * end of the Request's execution and indefinitely wait for it.
        */
        std::atomic<bool> hasBeenProcessed;

        /**
        * Indicates whether the request was successfully processed. Must be read
        * after hasBeenProcessed and wrote to before hasBeenProcessed when the
        * request comes near its final processing step.
        */
        std::atomic<bool> success;

        Request();

        /**
        * Virtual destructor to properly handle polymorphism and provide access to
        * dynamic casting.
        */
        virtual ~Request();

        /**
        * This method is called by the RequestManager on a non real-time thread to
        * prepare the Request for its execution, before sending it to the real-time
        * thread. It must return true if the preparation went well, and false
        * otherwise, in which case the Request will not be executed and the
        * process() method will not be called by the RequestManager. By default,
        * this method returns true.
        * .
        * If the Request is posted synchronously, then this method will be called by
        * the RequestManager on the non real-time thread upon reception, which will
        * generally give the same result as if the content of the prepare() method
        * was implemented directly into the process() method. Otherwise, if the
        * Request is posted asynchronously, then this method will be called by the
        * RequestManager's asynchronous, non real-time thread.
        * .
        * The prepare() method is mostly useful for asynchronously posted requests:
        * since the RequestManager's asynchronous thread treats only one request at
        * a time, the prepare() method is guaranteed to always be executed
        * separately and never concurrently for two different requests both posted
        * asynchronously.
        */
        virtual bool prepare();

        /**
        * This method is called by the Master on the real-time thread to process the
        * request. Derived classes of the Request class must implement this method.
        */
        virtual void process() = 0;
    };
}