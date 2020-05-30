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

#define ANGLECORE_NUM_MIDI_NOTES 128

namespace ANGLECORE
{
    /**
    * \class MIDI MIDI.h
    * Utility class that contains useful functions for working with MIDI messages.
    */
    class MIDI
    {
    public:

        /**
        * Returns the frequency corresponding to the MIDI note number \p noteNumber,
        * with the note A4 tuned to 440Hz.
        * @param[in] noteNumber The note number to retrieve the frequency from.
        */
        static floating_type getFrequencyOf(unsigned char noteNumber);

        /**
        * Returns the frequency corresponding to the MIDI note number \p noteNumber,
        * with the note A4 tuned to \p frequencyOfA4.
        * @param[in] noteNumber The note number to retrieve the frequency from.
        * @param[in] frequencyOfA4 The frequency that A4 should be tuned to. If this
        *   equals 440Hz, then you should consider calling the other overload of
        *   this function instead, which will return its result slightly faster.
        */
        static floating_type getFrequencyOf(unsigned char noteNumber, floating_type frequencyOfA4);

    protected:

        /**
        * Array containing at index \p i the frequency corresponding to the MIDI
        * note number \p i, with the note A4 tuned to 440Hz.
        */
        static const floating_type m_MIDI_frequencies_with_A4_at_440[ANGLECORE_NUM_MIDI_NOTES];

        /**
        * Array containing at index \p i a normalized coefficient which, when
        * multiplied by a reference frequency, give the frequency of the MIDI note
        * number \p i if A4 was tuned to that reference frequency. If the
        * coefficients were all multiplied by 440Hz, for instance, then the array
        * would simply become the same as m_MIDI_frequencies_with_A4_at_440.
        * The formula for the coefficient at index \p i is: 2 ^ ((\p i - 69) / 127).
        */
        static const floating_type m_normalized_MIDI_coefficients[ANGLECORE_NUM_MIDI_NOTES];
    };
}