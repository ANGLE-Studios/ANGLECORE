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
    * Set of streams which provide useful information, such as the sample rate the
    * audio should be rendered into, in a centralized spot, shared by the rest of
    * the AudioWorkflow.
    */
    struct GlobalContext
    {
        std::shared_ptr<Stream> sampleRateStream;
        std::shared_ptr<Stream> sampleRateReciprocalStream;

        /**
        * Creates a GlobalContext, hereby initializing every pointer to a Stream or
        * a Worker to a non null pointer. Note that this method does not do any
        * connection between streams and workers, as this is the responsability of
        * the AudioWorkflow to which the GlobalContext belong.
        */
        GlobalContext();

        void setSampleRate(floating_type sampleRate);

    private:
        floating_type m_currentSampleRate;
    };
}