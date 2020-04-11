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

#include "workflow/Stream.h"
#include "workflow/Worker.h"

namespace ANGLECORE
{
    /**
    * \class Builder Builder.h
    * Abstract class representing an object that is able to build worfklow items.
    */
    class Builder
    {
        /**
        * \struct Island Builder.h
        * Isolated part of a workflow, which is not connected to the real-time
        * rendering pipeline yet, but will be connected to the whole workflow by the
        * real-time thread.
        */
        struct Island
        {
            std::vector<std::shared_ptr<Stream>> streams;
            std::vector<std::shared_ptr<Worker>> workers;
        };

        /* We rely on the default constructor */
        
        /**
        * Builds and returns an Island for a Workflow to integrate. This method
        * should be overriden in each sub-class to construct the appropriate Island.
        */
        virtual std::shared_ptr<Island> build() = 0;
    };
}