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

#include "workflow/Worker.h"

#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
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

        /**
        * Instructs the Mixer to turn a Voice on, and to recompute its increments.
        * This method is really fast, as it will only be called by the real-time
        * thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Instructs the Mixer to turn a Voice off, and to recompute its increments.
        * This method is really fast, as it will only be called by the real-time
        * thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOff(unsigned short voiceNumber);

    private:

        /**
        * Recomputes the Mixer's increments after a Voice has been turned on or off.
        * This method is really fast, as it will only be called by the real-time
        * thread.
        */
        void updateIncrements();

        const unsigned short m_totalNumInstruments;

        /**
        * Voice to start from when mixing voices. This may vary depending on which
        * Voice is on and off.
        */
        unsigned short m_start;

        /**
        * Jumps to perform between voices when mixing the audio output in order to
        * avoid those that are off.
        */
        uint32_t m_increments[ANGLECORE_NUM_VOICES];
        
        /** Tracks the on/off status of every Voice */
        bool m_voiceIsOn[ANGLECORE_NUM_VOICES];
    };
}