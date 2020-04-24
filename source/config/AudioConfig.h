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

/**********************************************************************
** GENERAL AUDIO SETTINGS
**********************************************************************/

#define ANGLECORE_NUM_CHANNELS 2
#define ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE 10

/**********************************************************************
** AUDIO WORKFLOW
**********************************************************************/

#define ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN 0.5

/**********************************************************************
** INSTRUMENT
**********************************************************************/

#define ANGLECORE_INSTRUMENT_MINIMUM_SMOOTHING_DURATION 0.005   /**< Minimum duration to change the parameter of an instrument, in seconds */
#define ANGLECORE_INSTRUMENT_PARAMETER_MINIMUM_NONZERO_LEVEL 0.00001    /**< Minimum value to use when computing log() or multiplications during parameters' transients in an instrument */