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

#include "Workflow.h"

namespace ANGLECORE
{
    void Workflow::addStream(const std::shared_ptr<Stream>& streamToAdd)
    {
        /* The workflow will be modified only if the given pointer is valid */
        if (streamToAdd)

            /*
            * Since ID's are supposed to be unique, no replacement should occur here
            */
            m_streams[streamToAdd->id] = streamToAdd;
    }

    void Workflow::addWorker(const std::shared_ptr<Worker>& workerToAdd)
    {
        /* The workflow will be modified only if the given pointer is valid */
        if (workerToAdd)

            /*
            * Since ID's are supposed to be unique, no replacement should occur here
            */
            m_workers[workerToAdd->id] = workerToAdd;
    }

    bool Workflow::plugStreamIntoWorker(uint32_t streamID, uint32_t workerID, unsigned short inputStreamNumber)
    {
        /*
        * We look for the given stream in the workflow's stream map. If we find the
        * stream, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto streamIterator = m_streams.find(streamID);
        if (streamIterator != m_streams.end())
        {
            /* We found the stream, so we retrieve it */
            std::shared_ptr<Stream> stream = streamIterator->second;

            /*
            * Similarly, we look for the given worker in the workflow's worker map.
            * If we find the worker, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto workerIterator = m_workers.find(workerID);
            if (workerIterator != m_workers.end())
            {
                /* We found the worker, so we retrieve it */
                std::shared_ptr<Worker> worker = workerIterator->second;

                /*
                * According to the previous two remarks, we know that in this scope
                * 'stream' and 'worker' are both non-null pointers.
                */

                /*
                * We test inputStreamNumber against the size of the worker's input
                * bus:
                */
                if (inputStreamNumber < worker->getNumInputs())
                {
                    worker->connectInput(inputStreamNumber, stream);

                    /* The connection is successfull, so we return true */
                    return true;
                }
            }
        }

        /*
        * If we arrive here, this means one of the previous three tests failed,
        * which prevents us from making the connection. We therefore stop and return
        * false.
        */
        return false;
    }

    bool Workflow::plugWorkerIntoStream(uint32_t workerID, unsigned short outputStreamNumber, uint32_t streamID)
    {
        /*
        * We look for the given worker in the workflow's worker map. If we find the
        * worker, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto workerIterator = m_workers.find(workerID);
        if (workerIterator != m_workers.end())
        {
            /* We found the worker, so we retrieve it */
            std::shared_ptr<Worker> worker = workerIterator->second;

            /*
            * Similarly, we look for the given stream in the workflow's stream map.
            * If we find the stream, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto streamIterator = m_streams.find(streamID);
            if (streamIterator != m_streams.end())
            {
                /* We found the stream, so we retrieve it */
                std::shared_ptr<Stream> stream = streamIterator->second;

                /*
                * According to the previous two remarks, we know that in this scope
                * 'worker' and 'stream' are both non-null pointers.
                */

                /*
                * We test inputStreamNumber against the size of the worker's input
                * bus:
                */
                if (outputStreamNumber < worker->getNumOutputs())
                {
                    /*
                    * We connect the stream to the worker, and register that new
                    * connection into the workflow's streamID->inputWorker map. Just
                    * to be safe, we register the connection first and perform it
                    * second. That way, if for some reason the workflow was seen
                    * and used in an intermediate state here, it would still build a
                    * reliable rendering sequence, and prevent the worker from using
                    * uninitialized memory.
                    */
                    m_map[stream->id] = worker;
                    worker->connectOutput(outputStreamNumber, stream);

                    /* The connection is successfull, so we return true */
                    return true;
                }
            }
        }

        /*
        * If we arrive here, this means one of the previous three tests failed,
        * which prevents us from making the connection. We therefore stop and return
        * false.
        */
        return false;
    }
}