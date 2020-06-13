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

#include <algorithm>

#include "GlobalContext.h"
#include "../../config/AudioConfig.h"
#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    GlobalContext::GlobalContext() :
        m_currentSampleRate(1.0)
    {
        /*
        * We create a stream for the sample rate as well as one fore its reciprocal
        */
        sampleRateStream = std::make_shared<Stream>();
        sampleRateReciprocalStream = std::make_shared<Stream>();

        /* And then fill them up with an initial value of 1.0 */
        floating_type* sampleRateRawData = sampleRateStream->getDataForWriting();
        floating_type* sampleRateReciprocalRawData = sampleRateReciprocalStream->getDataForWriting();
        for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
        {
            sampleRateRawData[i] = static_cast<floating_type>(1.0);
            sampleRateReciprocalRawData[i] = static_cast<floating_type>(1.0);
        }
    }

    void GlobalContext::setSampleRate(floating_type sampleRate)
    {
        /*
        * The given sample rate is clamped between 1.0 and ANGLECORE_MAX_SAMPLE_RATE
        * to ensure it is never negative, nor equal to zero (so we can invert it).
        */
        floating_type clampedSampleRate = std::max(static_cast<floating_type>(1.0), std::min(sampleRate, static_cast<floating_type>(ANGLECORE_MAX_SAMPLE_RATE)));

        /*
        * To save some computation time, we will only change the sample rate stream
        * when necessary, which is when we receive a new value.
        */
        if (clampedSampleRate != m_currentSampleRate)
        {
            /* We fill the sample rate stream with the new sample rate value. */
            floating_type* sampleRateRawData = sampleRateStream->getDataForWriting();
            for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
                sampleRateRawData[i] = clampedSampleRate;

            /*
            * Then we compute the reciprocal of the sample rate, and fill the
            * corresponding stream accordingly.
            */
            floating_type clampedSampleRateReciprocal = static_cast<floating_type>(1.0) / clampedSampleRate;
            floating_type* sampleRateReciprocalRawData = sampleRateReciprocalStream->getDataForWriting();
            for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
                sampleRateReciprocalRawData[i] = clampedSampleRateReciprocal;

            /* And finally, we update the current sample rate value */
            m_currentSampleRate = clampedSampleRate;
        }
    }
}