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

#include <cstring>
#include <functional>

/**
* In order to avoid unnecessary copies and memory allocations when manipulating an
* STL key-value containers (map, unordered_map, etc.), it is definitely tempting to
* use C strings instead of std::string as a key type. This especially makes sense in
* the context of the Instrument class, where Parameters are stored in an
* unordered_map that is likely to be heavily solicited when rendering an audio
* block.
* However, in order to use C string keys (const char*) in those containers, one
* needs to define a structure for automatically hashing and comparing C strings.
* In C++17, the std::string_view type seems perfectly suited for the job, but in
* earlier C++ versions, there is no such convenient feature. This commit proposes a
* solution for C++11 and later versions: the ANGLECORE::StringView structure.
*/

namespace ANGLECORE
{
    /**
    * \struct StringView
    * Provides a simplified version of the C++17 string_view feature for compiling
    * with earlier versions of C++.
    * A StringView is simply a view over an existing C string, which provides a
    * simple interface for performing operations on that string.
    * This structure only contains a pointer to a C string, but defines special
    * comparison and hash operations which are applicable to that C string so that
    * it can be properly used as a identifier inside an STL key-value container.
    * Using a StringView instead of a std::string as a key type in such container
    * will prevent undesired memory allocation and copies.
    */
    struct StringView
    {
        /**
        * The constructor simply stores the provided pointer to C string internally.
        */
        StringView(const char* str) :
            string_ptr(str)
        {}

        /**
        * Comparing two StringViews consists in comparing their internal C strings,
        * using strcmp.
        */
        bool operator==(const StringView& other) const
        {
            return strcmp(string_ptr, other.string_ptr) == 0;
        }

        /* This is the internal pointer to the C string hold by the view */
        const char* string_ptr;
    };
}

/**
* In order to be used as a key within a standard key-value container, a StringView
* must have a dedicated hash function. Otherwise, only the internal pointer will be
* hashed, which will not produced the desired effect. Therefore, we have to
* specialize the standard hash structure to our StringView type.
*/
namespace std {
    template<>
    struct hash<ANGLECORE::StringView>
    {
        size_t operator()(const ANGLECORE::StringView& view) const
        {
            /* Here we simply implement the hash function of the boost library: */
            size_t hashValue = 0;
            for (const char* p = view.string_ptr; *p; p++)
                hashValue ^= *p + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
            return hashValue;
        }
    };
}