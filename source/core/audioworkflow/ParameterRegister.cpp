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

#include "ParameterRegister.h"

namespace ANGLECORE
{

    void ParameterRegister::insert(StringView parameterIdentifier, const Entry& entryToInsert)
    {
        m_data[parameterIdentifier] = entryToInsert;
    }

    ParameterRegister::Entry ParameterRegister::find(StringView parameterIdentifier) const
    {
        /*
        * We check whether or not the register's map contains an entry for the given
        * parameter, and if so, we return the corresponding entry.
        */

        const auto& registerIterator = m_data.find(parameterIdentifier);

        /* If an entry is found, we return it */
        if (registerIterator != m_data.end())
            return registerIterator->second;

        /* Otherwise, we return an empty entry */
        Entry entry;
        entry.generator = nullptr;
        entry.stream = nullptr;
        return entry;
    }

    void ParameterRegister::remove(StringView parameterIdentifier)
    {
        m_data.erase(parameterIdentifier);
    }

}