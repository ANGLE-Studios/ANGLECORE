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

#include <vector>
#include <unordered_map>

#include "../workflow/Worker.h"
#include "../parameter/Parameter.h"
#include "../../../utility/StringView.h"

namespace ANGLECORE
{
    /**
    * \class Instrument Instrument.h
    * Worker that generates audio within an AudioWorkflow.
    */
    class Instrument :
        public Worker
    {
    public:

        enum ContextParameter
        {
            SAMPLE_RATE = 0,
            SAMPLE_RATE_RECIPROCAL,
            FREQUENCY,
            FREQUENCY_OVER_SAMPLE_RATE,
            VELOCITY,
            NUM_CONTEXT_PARAMETERS
        };

        /**
        * \struct ContextConfiguration Instrument.h
        * The ContextConfiguration describes whether or not an Instrument should be
        * connected to a particular context within an AudioWorkflow. In defines how
        * an Instrument interacts with its surrounding, and, for example, if it
        * should capture and use the sample rate during its rendering.
        */
        struct ContextConfiguration
        {
            bool receiveSampleRate;
            bool receiveSampleRateReciprocal;
            bool receiveFrequency;
            bool receiveFrequencyOverSampleRate;
            bool receiveVelocity;

            /** Creates a ContextConfiguration from the given parameters */
            ContextConfiguration(bool receiveSampleRate, bool receiveSampleRateReciprocal, bool receiveFrequency, bool receiveFrequencyOverSampleRate, bool receiveVelocity);
        };

        /**
        * Creates an Instrument from a list of parameters, split between the context
        * parameters, which are common to other instruments (such as the sample rate
        * and the velocity), and the specific parameters that are unique for the
        * Instrument. Note that the two vectors passed in as arguments will be
        * copied inside the Instrument.
        * @param[in] contextParameters Vector containing all the context parameters
        *   the Instrument needs in order to work properly.
        * @param[in] parameters Vector containing all the specific parameters the
        *   Instrument needs to work properly, in addition to the context
        *   parameters.
        */
        Instrument(const std::vector<ContextParameter>& contextParameters, const std::vector<Parameter>& parameters);

        /**
        * Returns the input port number where the given \p contextParameter should
        * be plugged in.
        * @param[in] contextParameter Context parameter to retrieve the input port
        *   number from.
        */
        unsigned short getInputPortNumber(ContextParameter contextParameter) const;

        /**
        * Returns the input port number where the given Parameter, identified using
        * its StringView identifier, should be plugged in.
        * @param[in] parameterID String, submitted either as a StringView or a const
        *   char*, which uniquely identifies the given Parameter.
        */
        unsigned short getInputPortNumber(const StringView& parameterID) const;

        /**
        * Returns the Instrument's ContextConfiguration, which specifies how the
        * Instrument should be connected to the audio workflows' GlobalContext and
        * VoiceContext in order to retrieve shared information, such as the sample
        * rate or the velocity of each note.
        */
        const ContextConfiguration& getContextConfiguration() const;

        /**
        * Returns the Instrument's internal set of parameters, therefore excluding
        * the context parameters which are all exogenous.
        */
        const std::vector<Parameter>& getParameters() const;

        /**
        * Instructs the Instrument to reset itself, in order to be ready to play a
        * new note. This method will always be called when the end-user requests to
        * play a note, right before rendering. Subclasses of the Instrument class
        * must override this method and define a convenient behavior during this
        * reinitialization. For instance, instruments can use this opportunity to
        * read the sample rate and update internal parameters if it has changed.
        */
        virtual void reset() = 0;

        /**
        * Instructs the Instrument to start playing. This method will always be
        * called right after the reset() method. Subclasses of the Instrument class
        * must override this method and define a convenient behavior during this
        * initialization step. For instance, instruments can use this opportunity to
        * change the state of an internal Envelope.
        */
        virtual void startPlaying() = 0;

    private:
        const std::vector<ContextParameter> m_contextParameters;
        const std::vector<Parameter> m_parameters;
        const ContextConfiguration m_configuration;
        std::unordered_map<ContextParameter, unsigned short> m_contextParameterInputPortNumbers;
        std::unordered_map<StringView, unsigned short> m_parameterInputPortNumbers;
    };
}