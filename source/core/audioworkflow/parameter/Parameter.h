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

#include "../../../utility/StringView.h"
#include "../../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /**
    * \struct Parameter Parameter.h
    * Represents a parameter that an AudioWorkflow can modify and use for rendering.
    */
    struct Parameter
    {
        /**
        * \enum SmoothingMethod Parameter.h
        * Method used to smooth out a parameter change. If set to ADDITIVE, then a
        * parameter change will result in an arithmetic sequence from an initial
        * value to a target value. If set to MULTIPLICATIVE, then the sequence will
        * be geometric and use a multiplicative increment instead.
        */
        enum SmoothingMethod
        {
            ADDITIVE = 0,   /**< Generates an arithmetic sequence */
            MULTIPLICATIVE, /**< Generates a geometric sequence */
            NUM_METHODS     /**< Counts the number of available methods */
        };

        const StringView identifier;
        const SmoothingMethod smoothingMethod;
        const bool minimalSmoothingEnabled;
        const floating_type defaultValue;
        const floating_type minimalValue;
        const floating_type maximalValue;

        Parameter(const char* identifier, SmoothingMethod smoothingMethod, bool minimalSmoothingEnabled, floating_type defaultValue, floating_type minimalValue, floating_type maximalValue) :
            identifier(identifier),
            smoothingMethod(smoothingMethod),
            minimalSmoothingEnabled(minimalSmoothingEnabled),
            defaultValue(defaultValue),
            minimalValue(minimalValue),
            maximalValue(maximalValue)
        {}
    };
}