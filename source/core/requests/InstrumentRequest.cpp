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

#include "InstrumentRequest.h"
#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    InstrumentRequest::Result::Result() :
        Result(false, ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)
    {}

    InstrumentRequest::Result::Result(bool success, unsigned short rackNumber) :
        success(success),
        rackNumber(rackNumber)
    {}

    InstrumentRequest::InstrumentRequest(Type type, unsigned short rackNumber) :
        type(type),
        rackNumber(rackNumber)
    {}
}