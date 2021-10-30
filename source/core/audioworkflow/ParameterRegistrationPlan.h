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
#include <vector>

#include "../../utility/StringView.h"
#include "parameter/ParameterGenerator.h"
#include "workflow/Stream.h"

namespace ANGLECORE
{
    /**
    * \struct ParameterRegistrationPlan ParameterRegistrationPlan.h
    * When the end-user asks to add an Instrument to an AudioWorkflow or to remove
    * one from it, an instance of this structure is created to plan an update of its
    * parameter registers.
    */
    struct ParameterRegistrationPlan
    {
        /**
        * \struct Instruction ParameterRegistrationPlan.h
        * Contains all the details of a specific entry to either add to or remove
        * from the ParameterRegister.
        */
        struct Instruction
        {
            unsigned short rackNumber;
            StringView parameterIdentifier;
            std::shared_ptr<ParameterGenerator> parameterGenerator;
            std::shared_ptr<Stream> parameterStream;

            Instruction(unsigned short rackNumber, StringView parameterIdentifier, std::shared_ptr<ParameterGenerator> parameterGenerator, std::shared_ptr<Stream> parameterStream) :
                rackNumber(rackNumber),
                parameterIdentifier(parameterIdentifier),
                parameterGenerator(parameterGenerator),
                parameterStream(parameterStream)
            {}
        };

        std::vector<Instruction> removeInstructions;
        std::vector<Instruction> addInstructions;
    };
}