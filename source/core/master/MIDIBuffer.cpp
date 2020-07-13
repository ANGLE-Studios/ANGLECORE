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

#include "MIDIBuffer.h"

#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /* MIDIMessage
    ***************************************************/

    MIDIMessage::MIDIMessage() :
        type(Type::NONE),
        timestamp(0),
        noteNumber(0),
        noteVelocity(0)
    {}

    /* MIDIBuffer
    ***************************************************/

    MIDIBuffer::MIDIBuffer() :

        /*
        * The buffer's capacity is initialized to a very large value to minimize the
        * chances of resizing.
        */
        m_capacity(ANGLECORE_MIDIBUFFER_SIZE),

        /*
        * The m_messages vector is initialized with ANGLECORE_MIDIBUFFER_SIZE empty
        * MIDI messages.
        */
        m_messages(ANGLECORE_MIDIBUFFER_SIZE),

        /*
        * The buffer is considered initially empty (despite having its memory pre-
        * allocated), so m_numMessages is initialized to 0.
        */
        m_numMessages(0)
    {}

    uint32_t MIDIBuffer::getNumMIDIMessages() const
    {
        return m_numMessages;
    }

    MIDIMessage& MIDIBuffer::pushBackNewMIDIMessage()
    {
        m_numMessages++;

        /*
        * If we reach the buffer's maximal capacity, we augment it by
        * ANGLECORE_MIDIBUFFER_SIZE through a resize operation on the internal
        * vector. This  will trigger memory allocation and a large amount of copies,
        * but it should only be used as a last resort when receiving more messages
        * than expected.
        */
        if (m_numMessages > m_capacity)
        {
            m_messages.resize(m_capacity + ANGLECORE_MIDIBUFFER_SIZE);
            m_capacity += ANGLECORE_MIDIBUFFER_SIZE;
        }

        return m_messages[m_numMessages - 1];
    }

    void MIDIBuffer::clear()
    {
        m_numMessages = 0;
    }

    MIDIMessage& MIDIBuffer::operator[](uint32_t index)
    {
        return m_messages[index];
    }
}