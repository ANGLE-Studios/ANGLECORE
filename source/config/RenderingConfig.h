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

/*
* =====================================================================
* PROCESSING
* =====================================================================
*/

#define ANGLECORE_PRECISION double          /**< Defines the precision of ANGLECORE's calculations as either single or double. It should equal float or double. Note that one can still use double precision within the workers of an AudioWorkflow if this is set to float. */

/*
* =====================================================================
* MEMORY
* =====================================================================
*/

#define ANGLECORE_FIXED_STREAM_SIZE 256    /**< Fixed size to use for rendering (the rendering will be splitted into chunks of this size). */
#define ANGLECORE_NUM_VOICES 32
#define ANGLECORE_MIDIBUFFER_SIZE 2048     /**< Minimum number of MIDI messages the engine can handle without resizing. */



/*
* =====================================================================
* TYPE SHORTHANDS
* =====================================================================
*/

namespace ANGLECORE
{
    typedef ANGLECORE_PRECISION floating_type;
}