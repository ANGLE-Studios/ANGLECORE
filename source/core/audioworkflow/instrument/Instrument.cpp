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

#include "Instrument.h"

#include "../../../config/AudioConfig.h"
#include "../../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /* ContextConfiguration
    ***************************************************/

    Instrument::ContextConfiguration::ContextConfiguration(bool receiveSampleRate, bool receiveSampleRateReciprocal, bool receiveFrequency, bool receiveFrequencyOverSampleRate, bool receiveVelocity) :
        receiveSampleRate(receiveSampleRate),
        receiveSampleRateReciprocal(receiveSampleRateReciprocal),
        receiveFrequency(receiveFrequency),
        receiveFrequencyOverSampleRate(receiveFrequencyOverSampleRate),
        receiveVelocity(receiveVelocity)
    {}


    /* Instrument
    ***************************************************/

    Instrument::Instrument(const std::vector<ContextParameter>& contextParameters, const std::vector<Parameter>& parameters) :
        Worker(contextParameters.size() + parameters.size(), ANGLECORE_NUM_CHANNELS),

        m_contextParameters(contextParameters),
        m_parameters(parameters),

        m_configuration(
            std::find(contextParameters.cbegin(), contextParameters.cend(), ContextParameter::SAMPLE_RATE) != contextParameters.cend(),
            std::find(contextParameters.cbegin(), contextParameters.cend(), ContextParameter::SAMPLE_RATE_RECIPROCAL) != contextParameters.cend(),
            std::find(contextParameters.cbegin(), contextParameters.cend(), ContextParameter::FREQUENCY) != contextParameters.cend(),
            std::find(contextParameters.cbegin(), contextParameters.cend(), ContextParameter::FREQUENCY_OVER_SAMPLE_RATE) != contextParameters.cend(),
            std::find(contextParameters.cbegin(), contextParameters.cend(), ContextParameter::VELOCITY) != contextParameters.cend()
        ),

        /*
        * An Instrument is ON by default, so it is able to generate sound. It is
        * only once it has played and been stopped that it will enter an OFF state
        * for the first time.
        */
        m_state(State::ON)
    {
        unsigned short portNumber = 0;

        for (const ContextParameter& contextParameter : m_contextParameters)
            m_contextParameterInputPortNumbers[contextParameter] = portNumber++;

        for (const Parameter& parameter : m_parameters)
            m_parameterInputPortNumbers[parameter.identifier] = portNumber++;
    }

    unsigned short Instrument::getInputPortNumber(ContextParameter contextParameter) const
    {
        /*
        * We look for the given context parameter in the instrument's corresponding
        * port number map.
        */
        const auto& iterator = m_contextParameterInputPortNumbers.find(contextParameter);
        if (iterator != m_contextParameterInputPortNumbers.end())

            /* If we find it, we return its corresponding port number: */
            return iterator->second;

        /*
        * If we do not find the given context parameter, we return the number of
        * inputs the instrument has, which is an out-of-range port number for the
        * input bus:
        */
        return getNumInputs();
    }

    unsigned short Instrument::getInputPortNumber(const StringView& parameterID) const
    {
        /*
        * We look for the given parameter in the instrument's corresponding port
        * number map.
        */
        const auto& iterator = m_parameterInputPortNumbers.find(parameterID);
        if (iterator != m_parameterInputPortNumbers.end())

            /* If we find it, we return its corresponding port number: */
            return iterator->second;

        /*
        * If we do not find the given parameter, we return the number of inputs the
        * instrument has, which is an out-of-range port number for the input bus:
        */
        return getNumInputs();
    }

    const Instrument::ContextConfiguration& Instrument::getContextConfiguration() const
    {
        return m_configuration;
    }

    const std::vector<Parameter>& Instrument::getParameters() const
    {
        return m_parameters;
    }

    void Instrument::turnOn()
    {
        m_state = State::ON;
    }

    void Instrument::turnOff()
    {
        m_state = State::OFF;
    }

    void Instrument::prepareToStop(uint32_t stopDurationInSamples)
    {
        /* We initialize the instrument's internal stop tracker */
        m_stopTracker.stopDurationInSamples = stopDurationInSamples;
        m_stopTracker.position = 0;

        /* And we enter the ON_ASKED_TO_STOP state */
        m_state = State::ON_ASKED_TO_STOP;
    }

    void Instrument::work(unsigned int numSamplesToWorkOn)
    {
        switch (m_state)
        {
        case State::ON:

            /* In the ON state, we simply play audio normally */
            play(numSamplesToWorkOn);
            break;

        case State::ON_ASKED_TO_STOP:

            /*
            * In the ON_ASKED_TO_STOP state, the instrument is generating its audio
            * tail. By keeping track of the duration of that tail, we can detect
            * when the instrument is done, and stop playing any sound on subsequent
            * rendering calls, hereby saving some computation time.
            */

            /*
            * We create a scope so that we can initialize some intermediate
            * variables.
            */
            {
                /*
                * By construction, the stop tracker's stopDurationInSamples
                * attribute is always greater than or equal to its position
                * attribute, so the following substraction will never overflow.
                */
                uint32_t remainingSamples = m_stopTracker.stopDurationInSamples - m_stopTracker.position;

                if (remainingSamples > numSamplesToWorkOn)
                {
                    /*
                    * We simply play the instrument and increment the stop tracker's
                    * position. When updating the stop tracker's position, we assume
                    * the instrument's audio tail will always be short enough so
                    * that the tracker's position counter will never overflow.
                    */
                    play(numSamplesToWorkOn);
                    m_stopTracker.position += numSamplesToWorkOn;
                }
                else
                {
                    /*
                    * Here, we first need to render the end of the instrument's
                    * audio tail, and then generate some zeros. Once done, we will
                    * be able to move on to the next stage: the ON_TO_OFF state.
                    */

                    /*
                    * Note that, as a worker, an instrument is always guaranteed to
                    * receive a valid number of samples to render, which is a number
                    * greater than 0 and less than ANGLECORE_FIXED_STREAM_SIZE. We
                    * must remember to pass that guarantee to the play() method
                    * here, and only call it if the number of samples queried is non
                    * null:
                    */
                    if (remainingSamples != 0)

                        /*
                        * Note that, as a worker, an instrument is always guaranteed
                        * to receive a valid number of samples to render, which is a
                        * number greather than 0 and less than
                        * ANGLECORE_FIXED_STREAM_SIZE. We will pass that guarantee
                        * to the play() method here:
                        */
                        play(remainingSamples);

                    /*
                    * Then we need to render the right number of zeros, which we
                    * first compute hereafter:
                    */
                    uint32_t numZerosToGenerate = numSamplesToWorkOn - remainingSamples;

                    /*
                    * And we can now fill the rest of the output buffers with the
                    * right amount of zeros:
                    */
                    for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
                    {
                        floating_type* output = getOutputStream(c);
                        for (unsigned int i = 0; i < numZerosToGenerate; i++)
                            output[remainingSamples + i] = static_cast<floating_type>(0.0);
                    }

                    /*
                    * Finally, we move on the ON_TO_OFF state, to prepare for the
                    * OFF state where will be able to simply return immediately
                    * without performing any unnecessary computation while other
                    * instruments may pursue their audio tail in the meantime.
                    */
                    m_state = State::ON_TO_OFF;
                }
                break;
            }

        case State::ON_TO_OFF:

            /*
            * The purpose of the ON_TO_OFF state is to fill in the output streams
            * with zeros entirely, right before we enter the OFF stage where we will
            * no longer generate any output and simply return immediately.
            */

            /* So we first fill in the output streams with zeros: */
            for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
            {
                floating_type* output = getOutputStream(c);
                for (unsigned int i = 0; i < ANGLECORE_FIXED_STREAM_SIZE; i++)
                    output[i] = static_cast<floating_type>(0.0);
            }

            /* And then we enter the OFF state: */
            m_state = State::OFF;
            break;

        case State::OFF:

            /*
            * In the OFF state, we do not compute anything and return immediately,
            * in order to save some computation time.
            */

            break;
        }
    }
}