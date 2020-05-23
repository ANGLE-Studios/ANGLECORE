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
#include "../../config/RenderingConfig.h"
#include "../../config/MathConfig.h"
#include "../../config/AudioConfig.h"

#define ANGLECORE_FREQUENCY_PARAMETER_ID "ANGLECORE_FREQUENCY_PARAMETER_ID"
#define ANGLECORE_VELOCITY_PARAMETER_ID "ANGLECORE_VELOCITY_PARAMETER_ID"

namespace ANGLECORE
{
    /* Divider
    ***************************************************/

    VoiceContext::Divider::Divider() :
        Worker(Input::NUM_INPUTS, 1)
    {}

    void VoiceContext::Divider::work(unsigned int /* numSamplesToWorkOn */)
    {
        const floating_type* frequencyInput = getInputStream(Input::FREQUENCY);
        const floating_type* sampleRateInput = getInputStream(Input::SAMPLE_RATE);
        floating_type* output = getOutputStream(0);
        for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)

            /*
            * Note that the sample rate can never reach zero, so we do not need to
            * check for a possible division by zero here.
            */
            output[i] = frequencyInput[i] / sampleRateInput[i];
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
        divider = std::make_shared<Divider>();
        frequencyOverSampleRateStream = std::make_shared<Stream>();

        velocityStream = std::make_shared<Stream>();
    }
}