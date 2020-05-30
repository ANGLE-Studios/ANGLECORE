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

#include "VoiceContext.h"
#include "../../config/MathConfig.h"
#include "../../config/AudioConfig.h"

#define ANGLECORE_FREQUENCY_PARAMETER_ID "ANGLECORE_FREQUENCY_PARAMETER_ID"
#define ANGLECORE_VELOCITY_PARAMETER_ID "ANGLECORE_VELOCITY_PARAMETER_ID"

namespace ANGLECORE
{
    /* Divider
    ***************************************************/

    VoiceContext::RatioCalculator::RatioCalculator() :
        Worker(Input::NUM_INPUTS, 1)
    {}

    void VoiceContext::RatioCalculator::work(unsigned int numSamplesToWorkOn)
    {
        /*
        * To compute the division with better speed, we use the reciprocal of the
        * sample rate which has already been computed for us, and compute a
        * multiplication instead of a division:
        */
        const floating_type* frequency = getInputStream(Input::FREQUENCY);
        const floating_type* sampleRateReciprocal = getInputStream(Input::SAMPLE_RATE_RECIPROCAL);
        floating_type* output = getOutputStream(0);
        for (unsigned int i = 0; i < numSamplesToWorkOn; i++)
            output[i] = frequency[i] * sampleRateReciprocal[i];
    }

    /* VoiceContext
    ***************************************************/

    VoiceContext::VoiceContext() :

        /*
        * We initialize the frequency parameter with a minimal value of
        * ANGLECORE_EPSILON to ensure it is never negative, nor equal to zero (just
        * to be on the safe side).
        */
        frequency(ANGLECORE_FREQUENCY_PARAMETER_ID, ANGLECORE_EPSILON, ANGLECORE_EPSILON, ANGLECORE_MAX_SAMPLE_RATE, Parameter::SmoothingMethod::MULTIPLICATIVE, false, 0),

        velocity(ANGLECORE_VELOCITY_PARAMETER_ID, 0.0, 0.0, 1.0, Parameter::SmoothingMethod::ADDITIVE, false, 0)
    {
        frequencyGenerator = std::make_shared<ParameterGenerator>(frequency);
        frequencyStream = std::make_shared<Stream>();
        ratioCalculator = std::make_shared<RatioCalculator>();
        frequencyOverSampleRateStream = std::make_shared<Stream>();

        velocityStream = std::make_shared<Stream>();
    }
}