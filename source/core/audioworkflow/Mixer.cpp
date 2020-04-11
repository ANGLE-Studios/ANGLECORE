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

#include "Mixer.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    Mixer::Mixer() :
        Worker(ANGLECORE_AUDIOWORKFLOW_MAX_NUM_VOICES * ANGLECORE_AUDIOWORKFLOW_MAX_NUM_INSTRUMENTS_PER_VOICE * ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS, ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS),
        m_totalNumInstruments(ANGLECORE_AUDIOWORKFLOW_MAX_NUM_VOICES * ANGLECORE_AUDIOWORKFLOW_MAX_NUM_INSTRUMENTS_PER_VOICE)
    {}

    void Mixer::work(unsigned int numSamplesToWorkOn)
    {

        for (unsigned short c = 0; c < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS; c++)
        {
            double* output = getOutputStream(c);

            /* We check if an output stream is plugged in */
            if (output)
            {
                /* We first clear the output buffer: */
                for (unsigned int s = 0; s < numSamplesToWorkOn; s++)
                    output[s] = 0.0;

                /* 
                * And then we sum each input channel into its corresponding output
                * channel.
                */
                for (unsigned short i = 0; i < m_totalNumInstruments; i++)
                {
                    const double* input = getInputStream(i * ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS + c);

                    /* We check if an input stream is plugged in before summing */
                    if (input)
                        for (unsigned int s = 0; s < numSamplesToWorkOn; s++)
                            output[s] += input[s];
                }
            }
        }
    }
}