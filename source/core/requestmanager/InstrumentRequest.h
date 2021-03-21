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
#include "../audioworkflow/ParameterRegistrationPlan.h"

namespace ANGLECORE
{
    /**
    * \struct InstrumentRequest InstrumentRequest.h
    * When the end-user adds a new Instrument or removes one from an AudioWorkflow,
    * an instance of this structure is created to request the Mixer of the
    * AudioWorkflow to either activate or deactivate the corresponding rack for its
    * mixing process, to create or remove the necessary connections within the
    * AudioWorkflow, and to add or remove entries from the corresponding
    * ParameterRegister.
    */
    struct InstrumentRequest :
        public Request
    {
        /**
        * \struct Result InstrumentRequest.h
        * This structure is used to store the result of an InstrumentRequest.
        */
        struct Result
        {
            bool success;
            unsigned short rackNumber;

            Result();

            Result(bool success, unsigned short rackNumber);
        };

        enum Type
        {
            ADD = 0,
            REMOVE,
            NUM_TYPES
        };

        Type type;
        unsigned short rackNumber;

        /**
        * ConnectionRequest that matches the InstrumentRequest, and that instructs
        * to add or remove connections from the AudioWorkflow the Instrument will be
        * inserted into or removed from.
        */
        ConnectionRequest connectionRequest;

        /**
        * ParameterRegistrationPlan that matches the InstrumentRequest, and that
        * instructs to add or remove entries from the ParameterRegister of the
        * AudioWorkflow the Instrument will be inserted into or removed from.
        */
        ParameterRegistrationPlan parameterRegistrationPlan;

        /**
        * Creates an InstrumentRequest.
        */
        InstrumentRequest(Type type, unsigned short rackNumber);
    };
}