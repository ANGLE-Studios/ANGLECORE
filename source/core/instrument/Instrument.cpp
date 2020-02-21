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

#include "Instrument.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    /*  Parameter
    ************************************/

    BaseInstrument::Parameter::Parameter(double initialValue, bool minimalSmoothing, SmoothingMethod smoothingMethod) :
        state(INITIAL),
        minimalSmoothingEnabled(minimalSmoothing),
        smoothingMethod(smoothingMethod),
        internalValue(initialValue)
    {
        /* Initialize the transient curve pointer */
        transientCurve = std::make_unique<std::vector<double>>();

        /* Initialize the changeRequestDeposit */
        changeRequestDeposit.newChangeRequestReceived = false;
    }

    void BaseInstrument::Parameter::initialize(uint32_t maxSamplesPerBlock)
    {
        /* By construction, transientCurve is never a null pointer */
        transientCurve->reserve(maxSamplesPerBlock);

        /* Once the memory allocated, the parameter is ready for rendering */
        state.store(STEADY);
    }

    /*  ParameterRenderer
    ************************************/

    void BaseInstrument::ParameterRenderer::setSampleRate(double sampleRate)
    {
        /*
        * This function is called only when the sampleRate really takes a new value.
        */

        m_minSmoothingSamples.store(static_cast<uint32_t>(m_minSmoothingDuration * sampleRate));
        /* Note that we should actually add an extra sample when the result of the
        * multiplication is not an integer (which should be rare) */
    }

    void BaseInstrument::ParameterRenderer::setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock)
    {
        /*
        * For each parameter, we reserve the necessary capacity in the transient
        * curve. Note that internal pointers to Parameter cannot be null by
        * construction, so there is no need to test for nullptr.
        */
        for (auto& it : m_parameters)
            it.second->initialize(maxSamplesPerBlock);
    }

    void BaseInstrument::ParameterRenderer::renderParametersForNextAudioBlock(uint32_t blockSize)
    {
        /*  We first ensure that blockSize is at least one: */
        if (blockSize == 0)
            return;

        /* And then we render each parameter: */
        for (auto& it : m_parameters)
        {
            /*
            * Note that shared pointers to Parameter cannot be null by construction,
            * so there is no need to test for nullptr.
            */
            Parameter& parameter = *it.second;

            Parameter::State state = parameter.state.load();
                
            switch (state)
            {
            case Parameter::State::INITIAL:

                /*
                * If the parameter is in its initial state, there is nothing to do.
                * The parameter will return its internalValue when rendered.
                */
                break;

            case Parameter::State::STEADY:
            case Parameter::State::TRANSIENT:

                /*
                * We create a scope, which is necessary for initializing objects
                * within a switch statement, but also ensures the mutexes inside
                * will be unlocked on destruction.
                */
                {
                    /*
                    * The same process is applied when a change request is received,
                    * no matter which state the parameter is in between STEADY and
                    * TRANSIENT. Therefore, the code has been factorized according
                    * to the existence of a change request (see the 'if' statement).
                    * When no change request has been received, then only parameters
                    * in a TRANSIENT state needs to be taken care of, since we need
                    * to compute the next part of their transient curve. This is
                    * done in the 'else' part of the change request 'if' statement.
                    */
                    Parameter::ChangeRequest changeRequest;

                    /* We lock the parameter's ChangeRequestDeposit to read inside */
                    std::unique_lock<std::mutex> scopedLockOnDeposit(parameter.changeRequestDeposit.lock);

                    /* Have we received a new change request? ... */
                    if (parameter.changeRequestDeposit.newChangeRequestReceived)
                    {
                        /*
                        * ... YES we have!
                        * So we copy the newly received ChangeRequest. It is assumed
                        * copying a double, a boolean, and an unsigned int is fast
                        * enough (O(1)) to let us hold the mutex without any
                        * consequence, and without requiring to use a pointer swap
                        * for efficiency instead.
                        */
                        changeRequest = parameter.changeRequestDeposit.data;

                        /*
                        * Now that the copy is done, the change request should be
                        * marked as read. This is done by simply informing the
                        * ChangeRequestDeposit that there is no more ChangeRequest.
                        */
                        parameter.changeRequestDeposit.newChangeRequestReceived = false,

                        scopedLockOnDeposit.unlock();

                        /*
                        * Is that change request supposed to be smooth?
                        * Note that a smooth change of 0 samples is considered
                        * instantaneous, and treated as such:
                        */
                        if (changeRequest.smoothChange && changeRequest.durationInSamples > 0)
                        {

                            /* COMPUTING THE TRANSIENT CURVE (1/3)
                            **************************************/

                            std::vector<double>& curve = *parameter.transientCurve;

                            /*
                            * We then need to compute the curve's increment, which
                            * we first initialize:
                            */
                            double increment = parameter.smoothingMethod == Parameter::SmoothingMethod::MULTIPLICATIVE ? 1.0 : 0.0;

                            /*
                            * We atomically read the internalValue of the parameter,
                            * which is its initial value when following a transient
                            * curve.
                            */
                            double startValue = parameter.internalValue.load();

                            /*
                            * We know that changeRequest.durationInSamples > 0, so
                            * there is no need to test for a division by 0 here:
                            */
                            switch (parameter.smoothingMethod)
                            {
                            case Parameter::SmoothingMethod::ADDITIVE:
                                increment = (changeRequest.targetValue - startValue) / changeRequest.durationInSamples;
                                break;
                            case Parameter::SmoothingMethod::MULTIPLICATIVE:

                                /*
                                * Here we use the Taylor series of the exponential
                                * function, which is faster than using the C
                                * exponential function itself. However, because the
                                * change requested may be over a few samples, we
                                * need to go to order 2 so that we do not loose to
                                * much precision.
                                */
                                double endValue = fmax(changeRequest.targetValue, ANGLECORE_INSTRUMENT_PARAMETER_MINIMUM_NONZERO_LEVEL);
                                double epsilon = (log(endValue) - log(startValue)) / changeRequest.durationInSamples;
                                increment = 1.0 + epsilon + epsilon * epsilon * 0.5;
                                break;
                            }

                            /*
                            * Although blockSize is supposed to be always smaller
                            * than maxSamplesPerBlock, which implies that the
                            * transient curve is supposed to be always larger than
                            * blockSize, we still want to ensure to access it
                            * in-range in case the plugin is used with a buggy host.
                            * Therefore we still test our index variables against
                            * the size of the transient curve.
                            */
                            uint32_t curveSize = static_cast<uint32_t>(curve.size());

                            if (curveSize >= 1)
                            {
                                curve[0] = startValue;

                                /* Here we also ensure to access our transientCurve
                                * in-range, using a minimum size to fill:
                                */
                                uint32_t size = blockSize < curveSize ? blockSize : curveSize;

                                switch (parameter.smoothingMethod)
                                {
                                case Parameter::SmoothingMethod::ADDITIVE:

                                    /* We need to check for a possible short
                                    * transient, which would use less than a block
                                    * of size blockSize to complete:
                                    */
                                    if (changeRequest.durationInSamples < size)
                                    {
                                        /*
                                        * If the transient is very short, then we
                                        * first fill the transient samples (from
                                        * index 0 to durationInSamples-1), and then
                                        * we fill the rest of the curve with the end
                                        * value. And since the first value of the
                                        * transient curve has already been set at
                                        * index 0, we start the counter from 1:
                                        */
                                        for (uint32_t i = 1; i < changeRequest.durationInSamples; i++)
                                            curve[i] = curve[i - 1] + increment;
                                        for (uint32_t i = changeRequest.durationInSamples; i < size; i++)
                                            curve[i] = changeRequest.targetValue;
                                    }
                                    else
                                    {
                                        /*
                                        * If the transient is longer than size, then
                                        * we simply fill the curve incrementally:
                                        */
                                        for (uint32_t i = 1; i < size; i++)
                                            curve[i] = curve[i - 1] + increment;
                                    }
                                    break;

                                case Parameter::SmoothingMethod::MULTIPLICATIVE:

                                    /* We need to check for a possible short
                                    * transient, which would use less than a block
                                    * of size blockSize to complete:
                                    */
                                    if (changeRequest.durationInSamples < size)
                                    {
                                        /*
                                        * If the transient is very short, then we
                                        * first fill the transient samples (from
                                        * index 0 to durationInSamples-1), and then
                                        * we fill the rest of the curve with the end
                                        * value. And since the first value of the
                                        * transient curve has already been set at
                                        * index 0, we start the counter from 1:
                                        */
                                        for (uint32_t i = 1; i < changeRequest.durationInSamples; i++)
                                            curve[i] = curve[i - 1] * increment;
                                        for (uint32_t i = changeRequest.durationInSamples; i < size; i++)
                                            curve[i] = changeRequest.targetValue;
                                    }
                                    else
                                    {
                                        /*
                                        * If the transient is longer than size, then
                                        * we simply fill the curve incrementally:
                                        */
                                        for (uint32_t i = 1; i < size; i++)
                                            curve[i] = curve[i - 1] * increment;
                                    }
                                    break;
                                }
                            }

                            /* UPDATING THE TRANSIENT TRACKER (2/3)
                            ***************************************/

                            /*
                            * We create a reference to the parameter's transient
                            * tracker in order to make the code easier to read.
                            */
                            Parameter::TransientTracker& transientTracker(parameter.transientTracker);

                            /* (We create a scope for the following mutex to be
                            * unlocked on destruction...) */
                            {
                                /* We lock the parameter's TransientTracker */
                                std::lock_guard<std::mutex> scopedLockOnTransientTracker(transientTracker.lock);

                                transientTracker.targetValue = changeRequest.targetValue;
                                transientTracker.transientDurationInSamples = changeRequest.durationInSamples;
                                transientTracker.position = 0;
                                transientTracker.increment = increment;
                            }

                            /* CHANGING THE PARAMETER'S STATE (3/3)
                            ***************************************/

                            /*
                            * We can finally change the parameter’s state to
                            * TRANSIENT. This change needs to be done after the
                            * transient curve has been prepared, so that the
                            * parameter still renders as its internalValue in the
                            * meantime (the parameter will still be considered
                            * STEADY from the outside).
                            */
                            parameter.state.store(Parameter::State::TRANSIENT);
                        }

                        /*
                        * Else, if the change request is required to be
                        * instantaneous:
                        */
                        else
                        {
                            /*
                            * If however the change is supposed to be instantaneous,
                            * then we simply need to change the parameter's
                            * internalValue. This is done atomically.
                            */
                            parameter.internalValue.store(changeRequest.targetValue);

                            /*
                            * Any non-smooth change forces the parameter into a
                            * steady state:
                            */
                            parameter.state.store(Parameter::State::STEADY);
                        }
                    }

                    /*
                    * Else, no change request has been received, but if the
                    * parameter is in a TRANSIENT state, we still need to compute
                    * its transient curve!
                    */
                    if (state == Parameter::State::TRANSIENT)
                    {
                        std::vector<double>& curve = *parameter.transientCurve;

                        /*
                        * Here we also protect ourselves from buggy hosts, and
                        * ensure we access the curve in-range:
                        */
                        uint32_t curveSize = static_cast<uint32_t>(curve.size());
                        if (curveSize >= 1)
                        {
                            /* The parameter's internalValue is its start value: */
                            curve[0] = parameter.internalValue.load();

                            /* Here we also ensure to access our transientCurve
                            * in-range, using a minimum size to fill:
                            */
                            uint32_t size = blockSize < curveSize ? blockSize : curveSize;

                            Parameter::TransientTracker& transientTracker(parameter.transientTracker);

                            /*
                            * To detect the case of an ending transient, we need to
                            * compute the transient's remaining samples. We use a
                            * substraction of two uint32 for that, which should not
                            * overflow, as a parameter still in TRANSIENT state at
                            * this stage should always verify:
                            * position < transientDurationInSamples.
                            */
                            uint32_t remainingSamples = transientTracker.transientDurationInSamples - transientTracker.position;

                            switch (parameter.smoothingMethod)
                            {
                            case Parameter::SmoothingMethod::ADDITIVE:

                                /* We need to check for an ending transient, which
                                * would use less than a block of size blockSize to
                                * terminate:
                                */
                                if (remainingSamples < size)
                                {
                                    /*
                                    * If the transient is very short, then we
                                    * first fill the transient samples (from
                                    * index 0 to remainingSamples-1), and then we
                                    * fill the rest of the curve with the end value
                                    * And since the first value of the transient
                                    * curve has already been set at index 0, we
                                    * start the counter from 1:
                                    */
                                    for (uint32_t i = 1; i < remainingSamples; i++)
                                        curve[i] = curve[i - 1] + transientTracker.increment;
                                    for (uint32_t i = remainingSamples; i < size; i++)
                                        curve[i] = transientTracker.targetValue;
                                }
                                else
                                {
                                    /*
                                    * If the transient still needs more samples than
                                    * size to be complete, then we simply fill the
                                    * curve incrementally:
                                    */
                                    for (uint32_t i = 1; i < size; i++)
                                        curve[i] = curve[i - 1] + transientTracker.increment;
                                }
                                break;

                            case Parameter::SmoothingMethod::MULTIPLICATIVE:

                                /* We need to check for an ending transient, which
                                * would use less than a block of size blockSize to
                                * terminate:
                                */
                                if (remainingSamples < size)
                                {
                                    /*
                                    * If the transient is very short, then we
                                    * first fill the transient samples (from
                                    * index 0 to remainingSamples-1), and then we
                                    * fill the rest of the curve with the end value
                                    * And since the first value of the transient
                                    * curve has already been set at index 0, we
                                    * start the counter from 1:
                                    */
                                    for (uint32_t i = 1; i < remainingSamples; i++)
                                        curve[i] = curve[i - 1] * transientTracker.increment;
                                    for (uint32_t i = remainingSamples; i < size; i++)
                                        curve[i] = transientTracker.targetValue;
                                }
                                else
                                {
                                    /*
                                    * If the transient still needs more samples than
                                    * size to be complete, then we simply fill the
                                    * curve incrementally:
                                    */
                                    for (uint32_t i = 1; i < size; i++)
                                        curve[i] = curve[i - 1] * transientTracker.increment;
                                }
                                break;
                            }
                        }
                    }

                    /*
                    * Once here, the unique_lock will automatically unlock the
                    * deposit's mutex on its destruction.
                    */
                }
                break;
            }
        }
    }

    void BaseInstrument::ParameterRenderer::updateParametersAfterRendering(uint32_t blockSize)
    {
        // TO DO
        
        /*
        for (auto& it : m_parameters)
        {
            Parameter& parameter = *it.second;

            Parameter::State state = parameter.state.load();

            if (state == Parameter::State::TRANSIENT)
            {
                std::unique_lock<std::mutex> scopedLockTracker(parameter.transientTracker.lock);
                if (parameter.transientTracker.position >= parameter.transientTracker.transientDurationInSamples)
                {
                    parameter.internalValue.store(parameter.transientTracker.targetValue);
                    scopedLockTracker.unlock();
                    parameter.state.store(Parameter::State::STEADY);
                }
            }
        }
        */
    }

    /*  BaseInstrument
    ************************************/

    BaseInstrument::BaseInstrument() :
        m_parameterRenderer(m_parameters, ANGLECORE_INSTRUMENT_MINIMUM_SMOOTHING_DURATION)
    {
        // TO DO
    }

    void BaseInstrument::initialize(double sampleRate, uint32_t maxSamplesPerBlock)
    {
        // TO DO
    }

    void BaseInstrument::setSampleRate(double sampleRate)
    {
        /*
        * Increasing the sampleRate leads to memory reallocation, which can be an
        * expensive operation, so the sampleRate is changed only if the received
        * value is new.
        */
    }

    void BaseInstrument::setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock)
    {
        // TO DO
    }

    void BaseInstrument::requestParameterChange(const char* ID, double newValue, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    double BaseInstrument::sampleRate() const
    {
        // TO DO
    }

    double BaseInstrument::inverseSampleRate() const
    {
        // TO DO
    }

    void BaseInstrument::addParameter(const char* ID, double initialValue, bool minimalSmoothing, Parameter::SmoothingMethod smoothingMethod)
    {
        // TO DO
    }

    double BaseInstrument::parameter(const char* ID, uint32_t index) const
    {
        // TO DO
    }

    /*  Instrument
    ************************************/

    const char* Instrument::PARAMETER_ID_FREQUENCY_TO_PLAY = "ANGLECORE_PARAMETER_FREQUENCY_TO_PLAY";
    const char* Instrument::PARAMETER_ID_VELOCITY = "ANGLECORE_PARAMETER_VELOCITY";
    const char* Instrument::PARAMETER_ID_GAIN = "ANGLECORE_PARAMETER_GAIN";

    Instrument::Instrument()
    {
        // TO DO
    }

    void Instrument::requestNewFrequencyToPlay(double frequency, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    void Instrument::requestNewVelocity(double velocity, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    void Instrument::requestNewGain(double gain, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    double Instrument::frequencyToPlay(uint32_t index)
    {
        // TO DO
    }

    double Instrument::velocity(uint32_t index)
    {
        // TO DO
    }

    double Instrument::gain(uint32_t index)
    {
        // TO DO
    }
}