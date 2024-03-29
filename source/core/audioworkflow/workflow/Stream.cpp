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

#include "Stream.h"

#include "../../../config/RenderingConfig.h"

namespace ANGLECORE
{
    Stream::Stream() :
        WorkflowItem()
    {
        data = new floating_type[ANGLECORE_FIXED_STREAM_SIZE];

        /* We initialize a Stream by filling it with zeros */
        for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
            data[i] = static_cast<floating_type>(0.0);
    }

    Stream::~Stream()
    {
        delete[] data;
    }

    const floating_type* Stream::getDataForReading() const
    {
        return data;
    }

    floating_type* Stream::getDataForWriting()
    {
        return data;
    }
}