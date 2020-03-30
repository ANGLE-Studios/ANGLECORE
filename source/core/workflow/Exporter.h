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

#include "Worker.h"
#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    /**
    * \class Exporter Exporter.h
    * Worker that exports data from its input streams into an output buffer sent to
    * the host. An exporter applies a gain for calibrating its output level.
    */
    template<typename T>
    class Exporter :
        public Worker
    {
    public:

        /**
        * Creates a Worker with zero output.
        */
        Exporter() :
            Worker(ANGLECORE_AUDIOWORKFLOW_MAX_NUM_CHANNELS, 0),
            m_outputBuffer(nullptr)
        {}

        /**
        * Sets the new memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        */
        void setOutputBuffer(T** buffer)
        {
            m_outputBuffer = buffer;
        }

        void work(unsigned int numChannelsToWorkOn, unsigned int numSamplesToWorkOn)
        {
            /*
            * It is assumed that both numChannelsToWorkOn and numSamplesToWorkOn are
            * in-range, i.e. less than or equal to the maximal number of channels
            * and the stream buffer size respectively. It is also assumed the output
            * buffer has been properly set to a valid memory location.
            */
            for (unsigned int c = 0; c < numChannelsToWorkOn; c++)
                for (unsigned int i = 0; i < numSamplesToWorkOn; i++)
                    m_outputBuffer[c][i] = static_cast<T>(getInputStream(c)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
        }

    private:
        T** m_outputBuffer;
    };
}