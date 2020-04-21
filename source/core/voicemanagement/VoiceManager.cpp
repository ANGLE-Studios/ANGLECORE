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

#include "VoiceManager.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    /* Voice
    ***************************************************/

    Voice::Voice() :
        isOn(false),
        isFree(true),
        noteNumber(0)
    {}

    /* VoiceManager
    ***************************************************/

    VoiceManager::VoiceManager() :
        m_voices(ANGLECORE_AUDIOWORKFLOW_MAX_NUM_VOICES)
    {}

    Voice* VoiceManager::findFreeVoice()
    {
        /*
        * We loop through the voices, accessing them by reference to avoid
        * unnecessary copies. Once we find a free voice, we return it.
        */
        for (Voice& voice : m_voices)
            if (voice.isFree)
                return &voice;

        /*
        * If we did not find any free voice, then we return a nullptr to signal the
        * caller our research was unfruitful.
        */
        return nullptr;
    }

    void VoiceManager::turnOn(unsigned short voiceNumber)
    {
        m_voices[voiceNumber].isOn = true;
    }

    void VoiceManager::turnOff(unsigned short voiceNumber)
    {
        m_voices[voiceNumber].isOn = false;
    }
}