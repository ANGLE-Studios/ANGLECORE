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
    * 
    * The Request structure provides three virtual methods that can be overriden in
    * derived classes: preprocess(), process(), and postprocess(). The preprocess()
    * method should prepare everything for the Request's execution and must return a
    * boolean indicating if the preparation went well. The process() method should
    * contain the core implementation of the Request's execution. It is a pure
    * virtual method, so it must be implemented in all derived classes. Finally, the
    * postprocess() method is called right before the deletion of the Request
    * object, in order to do some final processing, like calling listeners to inform
    * them about how the Request's execution went.
    * 
    * Both preprocess() and postprocess() methods are called on a non real-time
    * thread, whereas the process() method is always executed on the real-time
    * thread, so it must be really fast.
    * 
    * The preprocess() method is guaranteed to always be executed before the
    * process() method, which in turn is guaranteed to always be executed before the
    * postprocess() method. However, if the preparation stage goes wrong and the
    * preprocess() method returns false, then the process() method will not be
    * called (the Request will not even be sent to the real-time thread), and the
    * RequestManager will jump directly to the postprocess() method instead.
    * 
    * For two different requests both posted asynchronously to the RequestManager,
    * the preprocess() method is guaranteed to always be executed separately and
    * never concurrently. That guarantee no longer holds for requests posted
    * synchronously: a request posted synchronously could be preprocessed in
    * parallel with another request posted synchronously, or with another request
    * posted asynchronously, since synchronous posting aims at preprocessing and
    * transferring requests upon reception. If concurrency is an issue, a proper
    * locking or sharing mechanism should be implemented in the preprocess() and
    * postprocess() methods, or the Request in question should be posted
    * asynchronously.
    */
    struct Request
    {
        /**
        * Indicates whether or not the request was preprocessed, that is if the
        * preprocess() method was called, regardless of the success of the
        * preprocessing.
        */
        std::atomic<bool> hasBeenPreprocessed;

        /**
        * Indicates whether or not the request was processed, that is if the
        * process() method was called, regardless of the success of the processing.
        */
        std::atomic<bool> hasBeenProcessed;

        /**
        * Indicates whether or not the request was postprocessed, that is if the
        * postprocess() method was called. This atomic boolean must always be set to
        * true once a Request is postprocessed; otherwise, if the Request has been
        * posted asynchronously to the RequestManager, then the latter may not
        * detect the end of the Request's execution and indefinitely wait for it.
        */
        std::atomic<bool> hasBeenPostprocessed;

        /**
        * Handy flag that can be used to indicate whether the request was
        * successfully processed or not.
        */
        std::atomic<bool> success;

        Request();

        /**
        * This method is called by the RequestManager on a non real-time thread to
        * preprocess the Request for its execution, before sending it to the
        * real-time thread. It must return true if the preparation went well, and
        * false otherwise, in which case the Request will not be executed on the
        * real-time thread and the process() method will not be called by the
        * RequestManager. By default, this method returns true.
        * 
        * If the Request is posted synchronously, then this method will be called by
        * the RequestManager on the non real-time thread upon reception. Otherwise,
        * if the Request is posted asynchronously, then this method will be called
        * by the RequestManager's asynchronous, non real-time thread.
        * 
        * The preprocess() method is mostly useful for asynchronously posted
        * requests: since the RequestManager's asynchronous thread treats only one
        * request at a time, the preprocess() method is guaranteed to always be
        * executed separately and never concurrently for two different requests both
        * posted asynchronously.
        */
        virtual bool preprocess();

        /**
        * This method is called by the Master on the real-time thread to process the
        * request. Derived classes of the Request class must implement this method.
        */
        virtual void process() = 0;

        /**
        * This method is called by the RequestManager on a non real-time thread to
        * make any final processing before the Request object is deleted. It is
        * always executed, even if the preprocessing failed and the process() method
        * was not called before. It will therefore give the same result as if its
        * content was implemented into the ~Request() destructor.
        * 
        * The postprocess() method is suitable for broadcasting information about
        * the Request's success to listeners, using the provided atomic flags and,
        * if necessary, other additional status variables defined in derived
        * classes.
        */
        virtual void postprocess();
    };
}