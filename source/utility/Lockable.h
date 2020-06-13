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

#include <mutex>

namespace ANGLECORE
{
    /**
    * \class Lockable Lockable.h
    * Utility object that can be locked for handling concurrency issues. The class
    * simply contains a mutex and a public method to access it by reference in order
    * to lock it.
    */
    class Lockable
    {
    public:

        /**
        * Returns the Lockable's internal mutex. Note that Lockable objects never
        * lock their mutex themselves: it is the responsability of the caller to
        * lock a Lockable appropriately when consulting or modifying its content in
        * a multi-threaded environment.
        */
        std::mutex& getLock()
        {
            return m_lock;
        }

    private:
        std::mutex m_lock;
    };
}