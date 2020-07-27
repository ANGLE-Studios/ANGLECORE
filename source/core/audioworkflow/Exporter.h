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

#include "workflow/Worker.h"

#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /**
    * \class Exporter Exporter.h
    * Worker that exports data from its input streams into an output buffer sent
    * to the host. An exporter applies a gain for calibrating its output level.
    */
    class Exporter :
        public Worker
    {
    public:

        /**
        * Creates a Worker with zero output.
        */
        Exporter();

        /**
        * Sets the new memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        * @param[in] numChannels Number of output channels.
        * @param[in] startSample Position to start from in the buffer.
        */
        void setOutputBuffer(export_type** buffer, unsigned short numChannels, uint32_t startSample);

        /**
        * Exports its input streams into the memory location that was given by the
        * setOutputBuffer() method. The Exporter will take care of any possible
        * mismatch between the number of channels rendered by ANGLECORE and the
        * number of channels requested by the host.
        * @param[in] numSamplesToWorkOn Number of samples to export.
        */
        void work(unsigned int numSamplesToWorkOn);

        /**
        * Increments the voice count. The Exporter will only export data if its
        * internal voice count is positive (> 0). Note that the voice count will
        * be clipped before exceeding the maximum number of voices available if this
        * method is called repeatedly.
        */
        void incrementVoiceCount();

        /**
        * Decrements the voice count. The Exporter will stop exporting data when its
        * internal voice count reaches the value of 0. Note that the voice count
        * will be clipped so it does not become negative if this method is called
        * repeatedly.
        */
        void decrementVoiceCount();

    private:
        export_type** m_outputBuffer;
        unsigned short m_numOutputChannels;
        uint32_t m_startSample;
        unsigned short m_numVoicesOn;
    };
}