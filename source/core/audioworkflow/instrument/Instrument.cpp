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
        )
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
}