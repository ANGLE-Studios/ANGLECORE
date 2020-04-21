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
#include <vector>
#include <unordered_map>

#include "../audioworkflow/workflow/Worker.h"

namespace ANGLECORE
{
    /**
    * \struct Voice VoiceManager.h
    * Represents a Voice that can be used to play some sound.
    */
    struct Voice
    {
        bool isOn;
        bool isFree;
        unsigned int noteNumber;

        Voice();
    };

    /**
    * \struct VoiceSequenceItem VoiceManager.h
    * Represents an item in a voice sequence.
    */
    struct VoiceSequenceItem
    {
        bool isGlobal;
        unsigned short voiceNumber;

        VoiceSequenceItem(bool isGlobal, unsigned short voiceNumber);
    };

    /**
    * \class VoiceManager VoiceManager.h
    * Entity that holds and manipulates a fixed number of voices.
    */
    class VoiceManager
    {
    public:

        /** Creates a VoiceManager with a fixed number of voices */
        VoiceManager();

        /**
        * Returns the inquired voice. Note that, just like for every method of the
        * VoiceManager class, the \p voiceNumber is expected to be in-range. No
        * safety checks will be performed, mostly to increase speed.
        * @param[in] voiceNumber ID of the Worker to map
        * @param[in] voiceNumber Unique number that identifies the Voice \p worker
        *   should be mapped to
        */
        Voice& getVoice(unsigned short voiceNumber);

        /**
        * Search for Voice that is free and available for rendering. If all the
        * voices are taken, it will return a nullptr.
        */
        Voice* findFreeVoice();

        /**
        * Turns on the given Voice. Note that because this method will be called by
        * the real-time thread, it needs to be fast, so it does not check if \p
        * voiceNumber is in-range.
        * @param[in] voiceNumber Number of the voice to turn on
        */
        void turnOn(unsigned short voiceNumber);

        /**
        * Turns off the given Voice. Note that, just like for every method of the
        * VoiceManager class, the \p voiceNumber is expected to be in-range. No
        * safety checks will be performed, mostly to increase speed.
        * @param[in] voiceNumber Number of the voice to turn off
        */
        void turnOff(unsigned short voiceNumber);

        /**
        * Maps a Worker to a Voice, so that the Worker will only be called if the
        * Voice is on at runtime. If a Worker is not part of any Voice, it will be
        * considered global, and will always be rendered. Note that, just like for
        * every method of the VoiceManager class, the \p voiceNumber is expected to
        * be in-range. No safety checks will be performed, mostly to increase speed.
        * @param[in] workerID ID of the Worker to map
        * @param[in] voiceNumber Unique number that identifies the Voice \p worker
        *   should be mapped to
        */
        void attachWorkerToVoice(uint32_t workerID, unsigned short voiceNumber);

        /**
        * Removes a Worker from a Voice, only if it was already mapped to it before.
        * Afterwards, the Worker will be considered global.
        * @param[in] workerID ID of the Worker to unmap
        */
        void detachWorkerFromVoice(uint32_t workerID);

        std::vector<VoiceSequenceItem> buildVoiceSequenceFromRenderingSequence(const std::vector<std::shared_ptr<Worker>>& renderingSequence) const;

    private:

        /** Fixed-size vector of voices */
        std::vector<Voice> m_voices;

        /**
        * Maps a Worker to its parent Voice, if relevant. Workers are referred to
        * with their ID, and voices with their number.
        */
        std::unordered_map<uint32_t, unsigned short> m_voiceAttachments;
    };
}