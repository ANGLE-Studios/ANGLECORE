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
    };
}