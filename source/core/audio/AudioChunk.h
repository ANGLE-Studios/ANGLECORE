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

namespace ANGLECORE
{
    /**
    * \struct AudioChunk AudioChunk.h
    * An Audio Chunk is a portion of an audio block that does not contain any MIDI
    * message and that can therefore be rendered in a row. An audio chunk typically
    * consists of a region in between two consecutive MIDI messages within an audio
    * block to render.
    */
    template<typename T>
    struct AudioChunk
    {
        T** const rawData;
        const unsigned short numChannels;
        const uint32_t chunkStartSample;
        const uint32_t chunkNumSamples;

        /**
        * The constructor just copies the arguments into the structure's attributes.
        */
        AudioChunk(T** rawData, unsigned short numChannels, uint32_t startSample, uint32_t numSamples) :
            rawData(rawData),
            numChannels(numChannels),
            chunkStartSample(startSamples),
            chunkNumSamples(numSamples)
        {}
    };
}