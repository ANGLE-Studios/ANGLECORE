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

#include "workflow/Worker.h"
#include "parameter/Parameter.h"
#include "parameter/ParameterGenerator.h"
#include "workflow/Stream.h"

namespace ANGLECORE
{
    /**
    * \struct VoiceContext VoiceContext.h
    * Set of streams and workers designed to provide useful information, such as the
    * frequency the instruments of Each voice should play, in a centralized spot.
    * Note that Workers and streams are not connected upon creation: it is the
    * responsability of the AudioWorkflow they will belong to to make the
    * appropriate connections.
    */
    struct VoiceContext
    {
        /**
        * \class Divider GlobalContext.h
        * Worker that simply computes the sample-wise division of its two input
        * streams.
        */
        class Divider :
            public Worker
        {
        public:

            enum Input
            {
                FREQUENCY = 0,
                SAMPLE_RATE,
                NUM_INPUTS  /**< This will equal two, as a Divider must have exactly
                            * two inputs */
            };

            /**
            * Creates a Divider, which is a Worker with two inputs and one
            * output.
            */
            Divider();

            /**
            * Computes the sample-wise division of the two input streams. Note that
            * the computation is done over the entire Stream, regardless of \p
            * numSamplesToWorkOn, for later use by other items from the Voice.
            * @param[in] numSamplesToWorkOn This parameter will be ignored.
            */
            void work(unsigned int numSamplesToWorkOn);
        };

        Parameter frequency;
        std::shared_ptr<ParameterGenerator> frequencyGenerator;
        std::shared_ptr<Stream> frequencyStream;
        std::shared_ptr<Divider> divider;
        std::shared_ptr<Stream> frequencyOverSampleRateStream;

        Parameter velocity;
        std::shared_ptr<Stream> velocityStream;

        /**
        * Creates a VoiceContext, hereby initializing every pointer to a Stream or a
        * Worker to a non null pointer. Note that this method does not do any
        * connection between streams and workers, as this is the responsability of
        * the AudioWorkflow to which the VoiceContext belong.
        */
        VoiceContext();
    };
}