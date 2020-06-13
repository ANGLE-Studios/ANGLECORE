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

#include "ConnectionRequest.h"

namespace ANGLECORE
{
    /**
    * \struct InstrumentRequest InstrumentRequest.h
    * When the end-user adds a new Instrument or removes one from an AudioWorkflow,
    * an instance of this structure is created to request the Mixer of the
    * AudioWorkflow to either activate or deactivate the corresponding rack for its
    * mixing process, and to create or remove the necessary connections within the
    * AudioWorkflow.
    */
    struct InstrumentRequest
    {
        enum Type
        {
            ADD = 0,
            REMOVE,
            NUM_TYPES
        };

        Type type;
        unsigned short rackNumber;

        /**
        * ConnectionRequest that matches the InstrumentRequest, and which instructs
        * to add or remove connections from the AudioWorkflow the Instrument will be
        * inserted into or removed from.
        */
        ConnectionRequest connectionRequest;

        /**
        * Creates an InstrumentRequest.
        */
        InstrumentRequest(Type type, unsigned short rackNumber);
    };
}