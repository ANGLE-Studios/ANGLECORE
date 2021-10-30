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
    /** Type of connection to use in a ConnectionInstruction */
    enum ConnectionType
    {
        STREAM_TO_WORKER, /**< Connection from a Stream to a Worker's input bus */
        WORKER_TO_STREAM  /**< Connection from a Worker's output bus to a Stream */
    };

    /** Type of instruction to perform in a ConnectionInstruction */
    enum InstructionType
    {
        PLUG,   /**< Request to make a connection */
        UNPLUG  /**< Request to remove a connection */
    };

    /**
    * \struct ConnectionInstruction ConnectionPlan.h
    * Instruction to either connect or disconnect two Workflow items, one being a
    * Stream and the other a Worker. Both items are referred to with their ID.
    */
    template<ConnectionType connectionType, InstructionType instructionType>
    struct ConnectionInstruction
    {
        uint32_t uphillID;
        uint32_t downhillID;
        unsigned short portNumber;

        /**
        * Builds a ConnectionInstruction corresponding to the given link type.
        * @param[in] streamID ID of the Stream that is part of the connection,
        *   either as an input or output Stream for the associated Worker
        * @param[in] workerID ID of the Worker that is part of the connection
        * @param[in] workerPortNumber Worker's port number, either from the input
        *   or output bus depending on the ConnectionType
        */
        ConnectionInstruction(uint32_t streamID, uint32_t workerID, unsigned short workerPortNumber)
        {
            switch (connectionType)
            {
            case ConnectionType::STREAM_TO_WORKER:
                uphillID = streamID;
                downhillID = workerID;
                break;
            case ConnectionType::WORKER_TO_STREAM:
                uphillID = workerID;
                downhillID = streamID;
                break;
            }
            portNumber = workerPortNumber;
        }
    };

    /**
    * \struct ConnectionPlan ConnectionPlan.h
    * Set of instructions to perform on the connections of an AudioWorkflow. This
    * structure also contains a rendering sequence, which corresponds to the one we
    * would obtain if the request had been proceeded. It is here to be precomputed
    * in advance, and it should replace the real-time thread's rendering sequence
    * when the latter is done with the request. By convention UNPLUG instructions
    * will be executed first, and PLUG instructions second.
    */
    struct ConnectionPlan
    {
        std::vector<ConnectionInstruction<STREAM_TO_WORKER, UNPLUG>> streamToWorkerUnplugInstructions;
        std::vector<ConnectionInstruction<WORKER_TO_STREAM, UNPLUG>> workerToStreamUnplugInstructions;
        std::vector<ConnectionInstruction<STREAM_TO_WORKER, PLUG>> streamToWorkerPlugInstructions;
        std::vector<ConnectionInstruction<WORKER_TO_STREAM, PLUG>> workerToStreamPlugInstructions;
    };
}