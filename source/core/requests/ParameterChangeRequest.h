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

#include <stdint.h>

#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /**
    * \struct ParameterChangeRequest ParameterChangeRequest.h
    * When the end-user asks to change the value of a Parameter within an Instrument
    * (volume, etc.) or the entire AudioWorkflow (sample rate, etc.), an instance of
    * this structure is created to store all necessary information about that
    * request.
    */
    struct ParameterChangeRequest
    {
        floating_type newValue;
        uint32_t durationInSamples;
    };
}