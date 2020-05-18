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

#include "ParameterGenerator.h"

namespace ANGLECORE
{
    ParameterGenerator::ParameterGenerator(const Parameter& parameter) :

        /* A ParameterGenerator has no input and only one output */
        Worker(0, 1),

        m_parameter(parameter),

        /*
        * In case multiple requests are received before a new rendering session
        * begins, only the most recent will be processed. Therefore, the request
        * queue need no more space than one slot.
        */
        m_requestQueue(1)
    {}

    void ParameterGenerator::postParameterChangeRequest(std::shared_ptr<ParameterChangeRequest>&& request)
    {
        m_requestQueue.push(std::move(request));
    }

    void ParameterGenerator::work(unsigned int numSamplesToWorkOn)
    {
        // TO DO
    }
}