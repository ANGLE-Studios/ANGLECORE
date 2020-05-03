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
#include <vector>
#include <stdint.h>

#include "workflow/Stream.h"
#include "workflow/Worker.h"

namespace ANGLECORE
{
    /**
    * \struct Environment Builder.h
    * Collection of workflow items built by a Builder. All the elements from an
    * Environment are isolated from the real-time rendering pipeline at first, and
    * will be connected to the whole workflow by the real-time thread. This,
    * however, does not prevent these items from being connected between themselves.
    */
    struct Environment
    {
        std::vector<std::shared_ptr<Stream>> streams;
        std::vector<std::shared_ptr<Worker>> workers;
    };

    /**
    * \struct InstrumentEnvironment Builder.h
    * Environment of an Instrument.
    */
    struct InstrumentEnvironment :
        public Environment
    {
        bool shouldReceiveFrequency;
        uint32_t frequencyReceiverID;
        unsigned short frequencyPortNumber;
    };

    /**
    * \class Builder Builder.h
    * Abstract class representing an object that is able to build a set of worfklow
    * items, packed together into an Environment.
    */
    template<class EnvironmentType>
    class Builder
    {
    public:

        /**
        * Builds and returns an Environment for a Workflow to integrate. This method
        * should be overriden in each sub-class to construct the appropriate
        * Environment.
        */
        virtual std::shared_ptr<EnvironmentType> build() = 0;
    };
}