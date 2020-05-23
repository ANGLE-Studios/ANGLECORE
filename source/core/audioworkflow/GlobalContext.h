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

#include <memory>

#include "workflow/Worker.h"
#include "parameter/Parameter.h"
#include "parameter/ParameterGenerator.h"
#include "workflow/Stream.h"

namespace ANGLECORE
{
    /**
    * \struct GlobalContext GlobalContext.h
    * Set of streams and workers designed to provide useful information, such as the
    * sample rate the audio should be rendered into, in a centralized spot, shared
    * by the rest of the AudioWorkflow. Note that Workers and streams are not
    * connected upon creation: it is the responsability of the AudioWorkflow they
    * will belong to to make the appropriate connections.
    */
    struct GlobalContext
    {
        /**
        * \class SampleRateInverter GlobalContext.h
        * Worker that simply computes the sample-wise reciprocal of a Stream.
        */
        class SampleRateInverter :
            public Worker
        {
        public:

            /**
            * Creates a SampleRateInverter, which is a Worker with one input and one
            * output.
            */
            SampleRateInverter();

            /**
            * Computes the sample-wise reciprocal of a Stream if, and only if, a new
            * value is detected in the input Stream. Note that the computation is
            * done over the entire Stream, regardless of \p numSamplesToWorkOn, for
            * later use by other items from the AudioWorkflow.
            * @param[in] numSamplesToWorkOn This parameter will be ignored.
            */
            void work(unsigned int numSamplesToWorkOn);

        private:
            floating_type m_oldSampleRate;
        };

        Parameter sampleRate;
        std::shared_ptr<ParameterGenerator> sampleRateGenerator;
        std::shared_ptr<Stream> sampleRateStream;
        std::shared_ptr<SampleRateInverter> sampleRateInverter;
        std::shared_ptr<Stream> sampleRateReciprocalStream;

        /**
        * Creates a GlobalContext, hereby initializing every pointer to a Stream or
        * a Worker to a non null pointer. Note that this method does not do any
        * connection between streams and workers, as this is the responsability of
        * the AudioWorkflow to which the GlobalContext belong.
        */
        GlobalContext();
    };
}