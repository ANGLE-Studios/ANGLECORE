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

#include "GlobalContext.h"
#include "../../config/RenderingConfig.h"
#include "../../config/AudioConfig.h"

#define ANGLECORE_SAMPLE_RATE_PARAMETER_ID "ANGLECORE_SAMPLE_RATE_PARAMETER_ID"

namespace ANGLECORE
{
    /* SampleRateInverter
    ***************************************************/

    GlobalContext::SampleRateInverter::SampleRateInverter() :
        Worker(1, 1),

        /*
        * The initial value of the internal copy of the sample rate should match the
        * default value of the sample rate parameter.
        */
        m_oldSampleRate(1.0)
    {}

    void GlobalContext::SampleRateInverter::work(unsigned int /* numSamplesToWorkOn */)
    {
        const floating_type* sampleRate = getInputStream(0);

        /*
        * To save time, we will only invert the sample rate stream when necessary,
        * which is when we receive a new value. We use the first value of the sample
        * rate stream to check for that.
        */
        if (sampleRate[0] != m_oldSampleRate)
        {
            /*
            * If the sample rate has changed, we first register its new value, and
            * then compute the reciprocal on the entire stream, for later use by all
            * the items of the AudioWorkflow.
            */
            m_oldSampleRate = sampleRate[0];
            floating_type* output = getOutputStream(0);
            for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
                output[i] = static_cast<floating_type>(1.0) / sampleRate[i];
        }
    }

    /* GlobalContext
    ***************************************************/

    GlobalContext::GlobalContext() :

        /*
        * We initialize the sampleRate parameter with a minimal value, to ensure it
        * is never negative, nor equal to zero (so we can invert it).
        */
        sampleRate(ANGLECORE_SAMPLE_RATE_PARAMETER_ID, 1.0, 1.0, ANGLECORE_MAX_SAMPLE_RATE, Parameter::SmoothingMethod::MULTIPLICATIVE, false, 0)
    {
        sampleRateGenerator = std::make_shared<ParameterGenerator>(sampleRate);
        sampleRateStream = std::make_shared<Stream>();
        sampleRateInverter = std::make_shared<SampleRateInverter>();
        sampleRateReciprocalStream = std::make_shared<Stream>();
    }
}