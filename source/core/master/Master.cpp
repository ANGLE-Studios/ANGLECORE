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

#include "Master.h"

namespace ANGLECORE
{
    Master::Master() :
        m_renderer(m_workflow)
    {}

    void Master::clearMIDIBufferForNextAudioBlock()
    {
        m_midiBuffer.clear();
    }

    MIDIMessage& Master::pushBackNewMIDIMessage()
    {
        return m_midiBuffer.pushBackNewMIDIMessage();
    }

    void Master::renderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples)
    {
        // TO DO
    }

    void Master::processMIDIMessage(const MIDIMessage& message)
    {
        switch (message.type)
        {
        case MIDIMessage::Type::NOTE_ON:
            {
                // TO DO
            }
            break;
        case MIDIMessage::Type::NOTE_OFF:
            {
                // TO DO
            }
            break;
        }
    }
}