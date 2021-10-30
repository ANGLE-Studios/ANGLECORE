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

#include "../config/RenderingConfig.h"

namespace ANGLECORE
{
    /**
    * \class FastMath FastMath.h
    * Provides various fast approximations of functions to accelerate computations.
    */
    class FastMath
    {
    public:
        FastMath() = delete;

        /**
        * Returns exp(\p epsilon), using a second order approximation of the
        * Taylor series for the exponential function. This method is faster than
        * using the C exponential function itself, and has a good precision if
        * epsilon is between -0.5 and 0.5.
        * @param[in] epsilon    The argument to compute the exponential of. It
        *   should be close to zero, or at least remain between -0.5 and 0.5.
        */
        /* TODO: in C++14 and later, this could be made a constexpr. */
        inline static floating_type exp(floating_type epsilon)
        {
            return 1.0 + epsilon + epsilon * epsilon * 0.5;
        }
    };
}