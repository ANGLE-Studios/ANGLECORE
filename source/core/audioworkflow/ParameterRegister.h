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
#include <unordered_map>

#include "parameter/ParameterGenerator.h"
#include "workflow/Stream.h"
#include "../../utility/StringView.h"

namespace ANGLECORE
{
    /**
    * \class ParameterRegister ParameterRegister.h
    * The goal of a ParameterRegister is to track which workflow items are involved
    * in the generation of a Parameter. It maps a Parameter with its corresponding
    * ParameterGenerator and with the Stream the generator will write into, which
    * provides a more direct access to the Parameter's values.
    */
    class ParameterRegister
    {
    public:

        /**
        * \struct Entry ParameterRegister.h
        * Element stored in the ParameterRegister that contains the
        * ParameterGenerator and Stream corresponding to a particular Parameter.
        */
        struct Entry
        {
            std::shared_ptr<ParameterGenerator> generator;
            std::shared_ptr<Stream> stream;
        };

        /**
        * Stores the given Entry into the register. Note that this method must only
        * be called by the real-time thread to execute a ParameterRegistrationPlan.
        * It should not be called by the non real-time thread, since the real-time
        * thread may be using the register to dispatch parameter change requests in
        * the meantime.
        * @param[in] parameterIdentifier The Parameter's identifier.
        * @param[in] entryToInsert The ParameterGenerator and Stream that correspond
        *   to the Parameter identified by \p parameterIdentifier.
        */
        void insert(StringView parameterIdentifier, const Entry& entryToInsert);

        /**
        * Searches for the given Parameter in the register. If the Parameter is
        * found, then the corresponding Entry is returned. Otherwise, this method
        * will return an Entry with empty pointers.
        * @param[in] parameterIdentifier The Parameter's identifier.
        */
        Entry find(StringView parameterIdentifier) const;

        /**
        * Removes any Entry that matches the given Parameter from the register. Note
        * that this method must only be called by the real-time thread to execute a
        * ParameterRegistrationPlan. It should not be called by the non real-time
        * thread, since the real-time thread may be using the register to dispatch
        * parameter change requests in the meantime. Also note that this method
        * deletes the shared pointers that were contained in the register after
        * removing them. It is the responsibility of the Master to retrieve a copy
        * of those pointers first, and then pass the copies to the non real-time
        * thread for deletion, so that the real-time thread does not deallocate any
        * memory.
        * @param[in] parameterIdentifier The Parameter's identifier.
        */
        void remove(StringView parameterIdentifier);

    private:
        std::unordered_map<StringView, Entry> m_data;
    };
}