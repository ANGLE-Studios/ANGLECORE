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

#include <stdint.h>
#include <memory>

#include "../workflow/Worker.h"
#include "Parameter.h"
#include "../../../dependencies/farbot/fifo.h"
#include "../../requests/ParameterChangeRequest.h"

namespace ANGLECORE
{
    /**
    * \class ParameterGenerator ParameterGenerator.h
    * Worker that generates the values of a Parameter in a Stream, according to the
    * end-user requests. A ParameterGenerator also takes care of smoothing out every
    * sudden change to avoid audio glitches.
    */
    class ParameterGenerator :
        public Worker
    {
    public:

        /**
        * Creates a ParameterGenerator from the given Parameter.
        * @param[in] parameter Parameter containing all the necessary information
        *   for the ParameterGenerator to generate its value.
        */
        ParameterGenerator(const Parameter& parameter);

        /**
        * Pushes the given request into the request queue. This method uses move
        * semantics, so it will take ownership of the pointer passed as argument. It
        * will never be called by the real-time thread, and will only be called by
        * the non real-time thread upon user request.
        * @param[in] request The ParameterChangeRequest to post to the
        *   ParameterGenerator. It will not be processed immediately, but before
        *   rendering the next audio block.
        */
        void postParameterChangeRequest(std::shared_ptr<ParameterChangeRequest>&& request);

        /**
        * Generates the successive values of the associated Parameter for the next
        * rendering session.
        * @param[in] numSamplesToWorkOn Number of samples to generate.
        */
        void work(unsigned int numSamplesToWorkOn);

        /**
        * Instructs the generator to change the parameter's value instantaneously,
        * without any transient phase. This method must never be called by the non
        * real-time thread, and should only be called by the real-time thread.
        * Requests emitted by the non-real thread should use the
        * postParameterChangeRequest() method instead.
        * @param[in] newValue The new value of the Parameter.
        */
        void setParameterValue(floating_type newValue);

    private:

        /**
        * \struct TransientTracker ParameterGenerator.h
        * A TransientTracker tracks a Parameter's position while in a transient
        * state. This structure can store how close the Parameter is to its target
        * value (in terms of remaining samples), as well as the increment to use for
        * its update.
        */
        struct TransientTracker
        {
            floating_type targetValue;
            uint32_t transientDurationInSamples;
            uint32_t position;
            floating_type increment;
        };

        /**
        * \enum State
        * Represents the state of a ParameterGenerator.
        */
        enum State
        {
            STEADY = 0,             /**< The Parameter has a constant value, and the output stream is entierly filled with that value */
            TRANSIENT,              /**< The Parameter is currently changing its value */
            TRANSIENT_TO_STEADY,    /**< The Parameter has reached a new value but the output stream still needs to be filled up accordingly */
            NUM_STATES              /**< Counts the number of possible states */
        };

        const Parameter& m_parameter;
        floating_type m_currentValue;
        State m_currentState;

        /** Queue for receiving Parameter change requests. */
        farbot::fifo<
            std::shared_ptr<const ParameterChangeRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_requestQueue;

        TransientTracker m_transientTracker;
    };
}