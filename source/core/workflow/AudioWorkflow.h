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

#include "Workflow.h"

namespace ANGLECORE
{
    /**
    * \class AudioWorkflow AudioWorkflow.h
    * Workflow internally structured to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class AudioWorkflow :
        public Workflow
    {
    public:

        /**
        * \class Exporter Exporter.h
        * Worker that exports data from its input streams into an output buffer sent
        * to the host. An exporter applies a gain for calibrating its output level.
        */
        template<typename OutputType>
        class Exporter :
            public Worker
        {
        public:

            /**
            * Creates a Worker with zero output.
            */
            Exporter() :
                Worker(ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS, 0),
                m_outputBuffer(nullptr),
                m_numOutputChannels(0)
            {}

            /**
            * Sets the new memory location to write into when exporting.
            * @param[in] buffer The new memory location.
            * @param[in] numChannels Number of output channels.
            */
            void setOutputBuffer(OutputType** buffer, unsigned short int numChannels)
            {
                m_outputBuffer = buffer;
                m_numOutputChannels = numChannels;
            }

            void work(unsigned int numSamplesToWorkOn)
            {
                /*
                * It is assumed that both m_numOutputChannels and numSamplesToWorkOn
                * are in-range, i.e. less than or equal to the output buffer's
                * number of channels and the stream and buffer size respectively. It
                * is also assumed the output buffer has been properly set to a valid
                * memory location.
                */

                /*
                * If the host request less channels than rendered, we sum their
                * content using a modulo approach.
                */
                if (m_numOutputChannels < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS)
                {
                    /* We first clear the output buffer */
                    for (unsigned short int c = 0; c < m_numOutputChannels; c++)
                        for (unsigned int i = 0; i < numSamplesToWorkOn; i++)
                            m_outputBuffer[c][i] = 0.0;

                    /* And then we compute the sum into the output buffer */
                    for (unsigned short int c = 0; c < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS; c++)
                        for (unsigned int i = 0; i < numSamplesToWorkOn; i++)
                            m_outputBuffer[c % m_numOutputChannels][i] += static_cast<OutputType>(getInputStream(c)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
                }

                /*
                * Otherwise, if we have not rendered enough channels for the host,
                * we simply duplicate data.
                */
                else
                {
                    for (unsigned int c = 0; c < m_numOutputChannels; c++)
                        for (unsigned int i = 0; i < numSamplesToWorkOn; i++)
                            m_outputBuffer[c][i] = static_cast<OutputType>(getInputStream(c % ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
                }
            }

        private:
            OutputType** m_outputBuffer;
            unsigned short int m_numOutputChannels;
        };

        /**
        * \class Mixer Mixer.h
        * Worker that sums all of its non-nullptr input stream, based on the audio
        * channel they each represent.
        */
        class Mixer :
            public Worker
        {
        public:

            /**
            * Initializes the Worker's buses size according to the audio
            * configuration (number of channels, number of instruments...)
            */
            Mixer();

            /**
            * Mixes all the channels together
            */
            void work(unsigned int numSamplesToWorkOn);

        private:
            const unsigned short m_totalNumInstruments;
        };

        AudioWorkflow();
    };
}