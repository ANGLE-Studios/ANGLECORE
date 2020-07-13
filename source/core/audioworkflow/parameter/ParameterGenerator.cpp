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

#include "ParameterGenerator.h"
#include "../../../config/MathConfig.h"
#include "../../../math/FastMath.h"
#include "../../../config/RenderingConfig.h"

namespace ANGLECORE
{
    ParameterGenerator::ParameterGenerator(const Parameter& parameter) :

        /* A ParameterGenerator has no input and only one output */
        Worker(0, 1),

        m_parameter(parameter),
        m_currentValue(parameter.defaultValue),

        /*
        * By initializing the state to TRANSIENT_TO_STEADY, we actually instruct the
        * generator to fill in the output stream with the parameter's default value
        * on the first call to the work() method. This ensures 
        */
        m_currentState(State::TRANSIENT_TO_STEADY),

        /*
        * In case multiple requests are received before a new rendering session
        * begins, only the most recent will be processed. Therefore, the request
        * queue needs no more space than one slot.
        */
        m_requestQueue(1)
    {}

    void ParameterGenerator::postParameterChangeRequest(std::shared_ptr<ParameterChangeRequest>&& request)
    {
        m_requestQueue.push(std::move(request));
    }

    void ParameterGenerator::work(unsigned int numSamplesToWorkOn)
    {
        /*
        * ===================================
        * STEP 1/2: PARAMETER CHANGE REQUESTS
        * ===================================
        */

        {
            /*
            * We define a scope here so that the following pointer is deleted as
            * soon as possible. This pointer will hold a copy of any
            * ParameterChangeRequest that has been sent by the Master to the
            * ParameterGenerator. When it is deleted, its reference count will
            * decrement, and hereby signal to the Master the ParameterChangeRequest
            * has been processed.
            */
            std::shared_ptr<const ParameterChangeRequest> request;

            /* Has a ParameterChangeRequest been received? ... */
            if (m_requestQueue.pop(request) && request)
            {
                /*
                * ... YES! So we need to process the request first. Note that null
                * pointers are ignored.
                */

                uint32_t durationInSamples = m_parameter.minimalSmoothingEnabled ? std::max(request->durationInSamples, m_parameter.minimalSmoothingDurationInSamples) : request->durationInSamples;

                /*
                * If the requested value exceeds the range of the parameter, we need
                * to clamp it into the right interval. Although the std::clamp
                * function would have been perfect for that, it is only available in
                * C++17 and forth, so for backward compatibility, we will simply use
                * a combination of std::min and std::max:
                */
                floating_type targetValue = std::max(m_parameter.minimalValue, std::min(request->newValue, m_parameter.maximalValue));

                /* Is this a smooth change? ... */
                if (durationInSamples > 0)
                {
                    /*
                    * ... YES! A smooth change should be performed. Therefore, we
                    * enter the TRANSIENT state.
                    */
                    m_currentState = State::TRANSIENT;

                    /*
                    * And we fill in the transient tracker. The first three
                    * attributes are easy to fill:
                    */
                    m_transientTracker.targetValue = targetValue;
                    m_transientTracker.transientDurationInSamples = durationInSamples;
                    m_transientTracker.position = 0;

                    /*
                    * Only the 'increment' requires a bit more calculations, as it
                    * depends on the parameter's smoothing method. We first
                    * initialize it to a default value:
                    */
                    m_transientTracker.increment = m_parameter.smoothingMethod == Parameter::SmoothingMethod::MULTIPLICATIVE ? 1.0 : 0.0;

                    /*
                    * And then we compute it depending on the smoothing technique.
                    */
                    switch (m_parameter.smoothingMethod)
                    {
                    case Parameter::SmoothingMethod::ADDITIVE:

                        /*
                        * We know that durationInSamples > 0, so there is no need to
                        * test for a division by 0 here.
                        */
                        m_transientTracker.increment = (targetValue - m_currentValue) / durationInSamples;

                        break;

                    case Parameter::SmoothingMethod::MULTIPLICATIVE:

                        /*
                        * To generate a geometric sequence from the parameter's
                        * current value to its next, we must ensure both values at
                        * the beginning and end of the sequence are positive
                        * (otherwise, no such sequence exists). Therefore, we use
                        * two low-bounded auxiliary variables, 'startValue' and
                        * 'endValue', to compute the increment. To declare and use
                        * those two, we need to open a new scope within the case
                        * statement:
                        */
                        {
                            floating_type startValue = std::max(m_currentValue, static_cast<floating_type>(ANGLECORE_EPSILON));
                            floating_type endValue = std::max(targetValue, static_cast<floating_type>(ANGLECORE_EPSILON));

                            /*
                            * We now know for sure that both 'startValue' and
                            * 'endValue' are positive, so we can call the log
                            * function on them:
                            */
                            floating_type epsilon = (log(endValue) - log(startValue)) / durationInSamples;

                            /*
                            * Here we use FastMath::exp to compute the increment,
                            * which is a lot faster than using its C counterpart,
                            * assuming: -0.5 <= epsilon <= 0.5.
                            */
                            m_transientTracker.increment = FastMath::exp(epsilon);
                        }
                        break;
                    }

                    /*
                    * Finally, if we are performing a multiplicative transient which
                    * starts on the value 0, then we need to change the parameter's
                    * current value to ANGLECORE_EPSILON in order for the geometric
                    * sequence to start and render properly.
                    */
                    if (m_parameter.smoothingMethod == Parameter::SmoothingMethod::MULTIPLICATIVE)
                        m_currentValue = std::max(m_currentValue, static_cast<floating_type>(ANGLECORE_EPSILON));
                }

                /*
                * ... NO! The change should be instantaneous. Therefore, we need to
                * enter the TRANSIENT_TO_STEADY state, as if we had just completed
                * an instantaneous transient.
                */
                else
                {
                    m_currentState = State::TRANSIENT_TO_STEADY;
                    m_currentValue = targetValue;
                }
            }

            /*
            * Once here, the 'request' pointer will be deleted, hereby decrementing
            * its reference count by one. Since parameter change requests are
            * supposed to be hold by the Master and copied before being sent to the
            * ParameterGenerator, no memory deallocation should occur here.
            */
        }

        /*
        * ===================
        * STEP 2/2: RENDERING
        * ===================
        */

        floating_type* output = getOutputStream(0);

        /*
        * Since a Worker is only called within a consistent rendering sequence, a
        * ParameterGenerator will only be called if its unique output stream is
        * connected to rendering pipeline. Therefore, the 'output' pointer should
        * never be null, and we will not verify that assertion in the code.
        */

        /*
        * If we are in a STEADY state, we actually have nothing to do. So we only
        * render samples if we are in a TRANSIENT or TRANSIENT_TO_STEADY state.
        */
        switch (m_currentState)
        {
        case State::TRANSIENT:

            {
                /*
                * To detect the case of an ending transient, we need to compute the
                * transient's remaining samples. We use a substraction of two uint32
                * for that, which should not overflow, as a parameter still in
                * TRANSIENT state at this stage should always verify:
                * position < transientDurationInSamples.
                */
                uint32_t remainingSamples = m_transientTracker.transientDurationInSamples - m_transientTracker.position;

                switch (m_parameter.smoothingMethod)
                {
                case Parameter::SmoothingMethod::ADDITIVE:

                    output[0] = m_currentValue + m_transientTracker.increment;

                    /* We need to check for an ending transient, which would use
                    * less than 'numSamplesToWorkOn' samples to terminate:
                    */
                    if (remainingSamples < numSamplesToWorkOn)
                    {
                        /*
                        * If the transient is very short, then we first fill the
                        * transient samples (from index 0 to remainingSamples-1),
                        * and then we fill the rest of the curve with the end value.
                        * And since the first value of the transient curve has
                        * already been set at index 0, we start the counter from 1:
                        */
                        for (uint32_t i = 1; i < remainingSamples; i++)
                            output[i] = output[i - 1] + m_transientTracker.increment;
                        for (uint32_t i = remainingSamples; i < numSamplesToWorkOn; i++)
                            output[i] = m_transientTracker.targetValue;
                    }
                    else
                    {
                        /*
                        * If the transient still needs more samples than
                        * 'numSamplesToWorkOn' to be complete, then we simply fill
                        * the samples in one row:
                        */
                        for (uint32_t i = 1; i < numSamplesToWorkOn; i++)
                            output[i] = output[i - 1] + m_transientTracker.increment;
                    }
                    break;

                case Parameter::SmoothingMethod::MULTIPLICATIVE:

                    output[0] = m_currentValue * m_transientTracker.increment;

                    /* We need to check for an ending transient, which would use
                    * less than 'numSamplesToWorkOn' samples to terminate:
                    */
                    if (remainingSamples < numSamplesToWorkOn)
                    {
                        /*
                        * If the transient is very short, then we first fill the
                        * transient samples (from index 0 to remainingSamples-1),
                        * and then we fill the rest of the curve with the end value.
                        * And since the first value of the transient curve has
                        * already been set at index 0, we start the counter from 1:
                        */
                        for (uint32_t i = 1; i < remainingSamples; i++)
                            output[i] = output[i - 1] * m_transientTracker.increment;
                        for (uint32_t i = remainingSamples; i < numSamplesToWorkOn; i++)
                            output[i] = m_transientTracker.targetValue;
                    }
                    else
                    {
                        /*
                        * If the transient still needs more samples than
                        * 'numSamplesToWorkOn' to be complete, then we simply fill
                        * the samples in one row:
                        */
                        for (uint32_t i = 1; i < numSamplesToWorkOn; i++)
                            output[i] = output[i - 1] * m_transientTracker.increment;
                    }
                }
            }

            /*
            * We increment the tracker's position by 'numSamplesToWorkOn', since we
            * have just rendered that amount of samples:
            */
            m_transientTracker.position += numSamplesToWorkOn;

            /*
            * Immediately afterwards, we check if the transient has just ended. If
            * it did, then we set the final value of the parameter, and we change
            * state. If it did not, then we simply update the parameter's current
            * value, using the last value we sent in the output stream.
            */
            if (m_transientTracker.position >= m_transientTracker.transientDurationInSamples)
            {
                m_currentState = State::TRANSIENT_TO_STEADY;
                m_currentValue = m_transientTracker.targetValue;
            }
            else

                /*
                * The Master guarantee 'numSamplesToWorkOn' is strictly positive,
                * so the following access is valid:
                */
                m_currentValue = output[numSamplesToWorkOn - 1];

            break;

        case State::TRANSIENT_TO_STEADY:

            /*
            * The TRANSIENT_TO_STEADY state is a cleanup state immediately following
            * the TRANSIENT state. It corresponds to a parameter which has just
            * reached its target value and ended its transient, and which has
            * therefore left the last part of its transient curve in the output
            * stream. The purpose of this state is precisely to instruct the worker
            * to fill in the output stream entirely (beyong numSamplesToWorkOn) so
            * that it can cease to work afterwards, once in a STEADY state.
            * Consequently, the only action here is to fill in the output stream
            * with the parameter's current value, and then to switch to a STEADY
            * state.
            */

            /* We fill in the output stream entirely */
            for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
                output[i] = m_currentValue;

            /* And we enter the STEADY state */
            m_currentState = State::STEADY;

            break;
        }
    }

    void ParameterGenerator::setParameterValue(floating_type newValue)
    {
        /*
        * We implement the same code as if we were processing an instantaneous
        * ParameterChangeRequest that did not require any transient stage: we change
        * the parameter's current value, without forgetting to clamp it into the
        * parameter's range, and we enter the TRANSIENT_TO_STEADY stage. This way,
        * on the next rendering call, the output stream will be filled up with the
        * new value. This technique has the interesting benefit of only filling the
        * output stream when necessary, during a rendering call.
        */
        m_currentValue = std::max(m_parameter.minimalValue, std::min(newValue, m_parameter.maximalValue));
        m_currentState = State::TRANSIENT_TO_STEADY;
    }
}