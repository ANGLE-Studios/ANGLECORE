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
                    m_inputWorkers[stream->id] = worker;
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

    void Workflow::completeRenderingSequenceForWorker(const std::shared_ptr<const Worker>& worker, std::vector<std::shared_ptr<const Worker>>& currentRenderingSequence) const
    {
        /*
        * If worker is a nullptr, we have nothing to start the computation from, so
        * we simply return here.
        */
        if (!worker)
            return;

        for (auto stream : worker->getInputBus())
        {
            /*
            * The worker could have some unplugged input, so we test if
            * streamPointer is null first.
            */
            if (stream)
            {
                /*
                * We retrieve the current stream's input worker, which is the worker
                * who is responsible for filling it up. We stored that information
                * in the m_map attribute, so we simply need to look it up.
                */
                auto inputWorkerIterator = m_inputWorkers.find(stream->id);
                if (inputWorkerIterator != m_inputWorkers.end())
                {
                    const std::shared_ptr<const Worker> inputWorker = inputWorkerIterator->second;

                    /*
                    * Note that once here, since we found the worker in the map, it
                    * is guaranteed inputWorker is not a null pointer, as we only
                    * insert non-null shared pointers into the workflow's maps.
                    */

                    /*
                    * We recursively call the current function on the worker we just
                    * found, in order to retrieve all of the workers that need to be
                    * called before it. We will add inputWorker at the end of this
                    * sequence.
                    */
                    completeRenderingSequenceForWorker(inputWorker, currentRenderingSequence);

                    /*
                    * Finally, we add inputWorker at the end of the current sequence,
                    * but only if it is not already part of the sequence.
                    */
                    bool inputWorkerAlreadyInSequence = false;
                    for (auto it = currentRenderingSequence.cbegin(); it != currentRenderingSequence.cend() && !inputWorkerAlreadyInSequence; it++)
                    {
                        /*
                        * We use the worker's ID to detect whether it is already in
                        * the sequence.
                        */
                        if ((*it)->id == inputWorker->id)
                            inputWorkerAlreadyInSequence = true;
                    }
                    if (!inputWorkerAlreadyInSequence)
                        currentRenderingSequence.emplace_back(inputWorker);
                }
            }
        }


        /*
        * Finally, we add the worker to the rendering sequence if it was not already
        * there.
        */
        bool alreadyInSequence = false;
        for (auto it = currentRenderingSequence.cbegin(); it != currentRenderingSequence.cend() && !alreadyInSequence; it++)
        {
            /*
            * We use the worker's ID to detect whether it is already in the sequence
            */
            if ((*it)->id == worker->id)
                alreadyInSequence = true;
        }
        if (!alreadyInSequence)
            currentRenderingSequence.emplace_back(worker);
    }

    void Workflow::completeRenderingSequenceForStream(const std::shared_ptr<const Stream>& stream, std::vector<std::shared_ptr<const Worker>>& currentRenderingSequence) const
    {
        /*
        * If stream is a nullptr, we have nothing to start the computation from, so
        * we simply return here.
        */
        if (!stream)
            return;

        auto inputWorkerIterator = m_inputWorkers.find(stream->id);
        if (inputWorkerIterator != m_inputWorkers.end())
        {
            /*
            * Note that once here, since we found the worker in the map, it is
            * guaranteed inputWorker is not a null pointer, as we only insert non-
            * null shared pointers into the workflow's maps.
            */
            completeRenderingSequenceForWorker(inputWorkerIterator->second, currentRenderingSequence);
        }
    }
}