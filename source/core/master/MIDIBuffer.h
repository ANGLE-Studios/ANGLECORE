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
#include <vector>

namespace ANGLECORE
{
    /**
    * \struct MIDIMessage MIDIBuffer.h
    * Represents a MIDI message supported by the Master object.
    */
    struct MIDIMessage
    {
        /** Types of MIDI messages supported by the Master object */
        enum Type
        {
            NONE = 0,       /**< MIDI Messages of this type will be ignored */
            NOTE_ON,
            NOTE_OFF,
            ALL_NOTES_OFF,
            ALL_SOUND_OFF,
            NUM_TYPES
        };

        /** Type of MIDI message */
        Type type;

        /**
        * Timestamp of the message as a sample position within the current audio
        * buffer
        */
        uint32_t sample;

        unsigned char noteNumber;

        /**
        * Creates a MIDI message of type NONE, and initializes every attribute to
        * their default value.
        */
        MIDIMessage();
    };

    /**
    * \class MIDIBuffer MIDIBuffer.h
    * Buffer of MIDI messages.
    */
    class MIDIBuffer
    {
    public:

        /** Creates a MIDI buffer of fixed size */
        MIDIBuffer();

        /**
        * Returns the number of MIDI messages contained in the buffer. This will be
        * different from the buffer's capacity.
        */
        uint32_t getNumMIDIMessages() const;

        /**
        * Adds a new empty MIDIMessage at the end of the buffer, and returns a
        * reference to it.
        */
        MIDIMessage& pushBackNewMIDIMessage();

        /** Removes every MIDIMessage from the buffer. */
        void clear();

        /**
        * Provides a read an write access to the MIDI messages.
        * @param[in] index Position of the MIDIMessage to retrieve from the buffer.
        *   It should be in-range, as it will not be checked at runtime.
        */
        MIDIMessage& operator[](uint32_t index);

    private:
        uint32_t m_capacity;
        std::vector<MIDIMessage> m_messages;
        uint32_t m_numMessages;
    };
}