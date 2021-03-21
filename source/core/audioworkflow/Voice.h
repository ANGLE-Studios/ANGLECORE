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

#include "../../config/AudioConfig.h"
#include "instrument/Instrument.h"
#include "VoiceContext.h"

namespace ANGLECORE
{
    /**
    * \struct Voice Voice.h
    * Set of workers and streams, which together form an audio engine that can be
    * used by the Master, the AudioWorkflow, and the Renderer to generate some audio
    * according to what the user wants to play.
    */
    struct Voice
    {
        /**
        * \struct Rack Voice.h
        * Set of workers and streams which are specific to an Instrument, within a
        * particular Voice.
        */
        struct Rack
        {
            bool isEmpty;
            std::shared_ptr<Instrument> instrument;
            bool isOn;
        };

        bool isFree;
        bool isOn;
        unsigned char currentNoteNumber;

        /**
        * Each Voice has its own context, called a VoiceContext. A VoiceContext is a
        * set of streams and workers who sends information about the frequency and
        * velocity the Voice should use to play some sound.
        */
        VoiceContext voiceContext;
        Rack racks[ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE];

        /**
        * Creates a Voice only comprised of empty racks.
        */
        Voice();
    };
}