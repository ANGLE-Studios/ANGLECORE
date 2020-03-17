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

#include <algorithm>

#include "Worker.h"

namespace ANGLECORE
{
    Worker::Worker(unsigned short numInputs, unsigned short numOutputs) :
        WorkflowItem(),

        /*
        * m_hasInputs can be determined upon construction, which is why it is const.
        */
        m_hasInputs(numInputs > 0)
    {

        /*
        * We resize the buses and ensure all the pointers are initialized to nullptr
        */
        m_inputBus.resize(numInputs);
        std::fill(m_inputBus.begin(), m_inputBus.end(), nullptr);
        m_outputBus.resize(numOutputs);
        std::fill(m_outputBus.begin(), m_outputBus.end(), nullptr);
    }

    void Worker::connectInput(unsigned short index, std::shared_ptr<const Stream> newInputStream)
    {
        /*
        * We simply replace the pointer in the input bus by the new address. We do
        * not check if we are out of range. Note that this method can implicitely
        * call the previous stream's destructor, and therefore provoke memory
        * deallocation. Consequently, this method should not be called by the
        * real-time thread.
        */
        m_inputBus[index] = newInputStream;
    }

    void Worker::connectOutput(unsigned short index, std::shared_ptr<Stream> newOutputStream)
    {
        /*
        * We simply replace the pointer in the output bus by the new address. We do
        * not check if we are out of range. Note that this method can implicitely
        * call the previous stream's destructor, and therefore provoke memory
        * deallocation. Consequently, this method should not be called by the
        * real-time thread.
        */
        m_outputBus[index] = newOutputStream;
    }

    const double* const Worker::getInputStream(unsigned short index) const
    {
        return m_inputBus[index]->getDataForReading();
    }

    double* Worker::getOutputStream(unsigned short index) const
    {
        return m_outputBus[index]->getDataForWriting();
    }

    const std::vector<std::shared_ptr<const Stream>>& Worker::getInputBus() const
    {
        return m_inputBus;
    }

    bool Worker::hasInputs() const
    {
        return m_hasInputs;
    }
}