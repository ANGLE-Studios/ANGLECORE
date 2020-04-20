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

#include <algorithm>

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

    bool Workflow::plugStreamIntoWorker(uint32_t streamID, uint32_t workerID, unsigned short inputPortNumber)
    {
        /*
        * We look for the given stream in the workflow's stream map. If we find the
        * stream, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto& streamIterator = m_streams.find(streamID);
        if (streamIterator != m_streams.end())
        {
            /* We found the stream, so we retrieve it */
            std::shared_ptr<Stream> stream = streamIterator->second;

            /*
            * Similarly, we look for the given worker in the workflow's worker map.
            * If we find the worker, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto& workerIterator = m_workers.find(workerID);
            if (workerIterator != m_workers.end())
            {
                /* We found the worker, so we retrieve it */
                std::shared_ptr<Worker> worker = workerIterator->second;

                /*
                * According to the previous two remarks, we know that in this scope
                * 'stream' and 'worker' are both non-null pointers.
                */

                /*
                * We test inputPortNumber against the size of the worker's input
                * bus:
                */
                if (inputPortNumber < worker->getNumInputs())
                {
                    worker->connectInput(inputPortNumber, stream);

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

    bool Workflow::plugWorkerIntoStream(uint32_t workerID, unsigned short outputPortNumber, uint32_t streamID)
    {
        /*
        * We look for the given worker in the workflow's worker map. If we find the
        * worker, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto& workerIterator = m_workers.find(workerID);
        if (workerIterator != m_workers.end())
        {
            /* We found the worker, so we retrieve it */
            std::shared_ptr<Worker> worker = workerIterator->second;

            /*
            * Similarly, we look for the given stream in the workflow's stream map.
            * If we find the stream, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto& streamIterator = m_streams.find(streamID);
            if (streamIterator != m_streams.end())
            {
                /* We found the stream, so we retrieve it */
                std::shared_ptr<Stream> stream = streamIterator->second;

                /*
                * According to the previous two remarks, we know that in this scope
                * 'worker' and 'stream' are both non-null pointers.
                */

                /*
                * We test outputPortNumber against the size of the worker's output
                * bus:
                */
                if (outputPortNumber < worker->getNumOutputs())
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
                    worker->connectOutput(outputPortNumber, stream);

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

    bool Workflow::unplugStreamFromWorker(uint32_t streamID, uint32_t workerID, unsigned short inputPortNumber)
    {
        /*
        * WE STILL NEED TO CHECK IF THE STREAM EXISTS:
        * If the Stream is unregistered from the Workflow while still being
        * referenced by the Worker, the ID comparison will succeed and the Worker
        * will delete its input, possibly causing memory deallocation
        */

        /*
        * We look for the given stream in the workflow's stream map. If we find the
        * stream, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto& streamIterator = m_streams.find(streamID);
        if (streamIterator != m_streams.end())
        {
            /*
            * Similarly, we look for the given worker in the workflow's worker map.
            * If we find the worker, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto& workerIterator = m_workers.find(workerID);
            if (workerIterator != m_workers.end())
            {
                /* We found the worker, so we retrieve it */
                std::shared_ptr<Worker> worker = workerIterator->second;

                const std::vector<std::shared_ptr<const Stream>>& inputBus = worker->getInputBus();

                /*
                * We test inputPortNumber against the size of the worker's input
                * bus, and we compare the encountered stream's ID in a row:
                */
                if (inputPortNumber < worker->getNumInputs() && inputBus[inputPortNumber] && inputBus[inputPortNumber]->id == streamID)
                {
                    // NO MEMORY DEALLOCATION
                    worker->disconnectInput(inputPortNumber);

                    /* The disconnection is successfull, so we return true */
                    return true;
                }
            }
        }

        /*
        * If we arrive here, this means one of the previous three tests failed,
        * which prevents us from making the disconnection. We therefore stop and
        * return false.
        */
        return false;
    }

    bool Workflow::unplugWorkerFromStream(uint32_t workerID, unsigned short outputPortNumber, uint32_t streamID)
    {
        /*
        * WE STILL NEED TO CHECK IF THE STREAM EXISTS:
        * If the Stream is unregistered from the Workflow while still being
        * referenced by the Worker, the ID comparison will succeed and the Worker
        * will delete its input, possibly causing memory deallocation
        */

        /*
        * We look for the given worker in the workflow's worker map. If we find the
        * worker, it automatically implies its corresponding pointer is not null, as
        * we only insert non-null shared pointers into the maps.
        */
        auto& workerIterator = m_workers.find(workerID);
        if (workerIterator != m_workers.end())
        {
            /* We found the worker, so we retrieve it */
            std::shared_ptr<Worker> worker = workerIterator->second;

            const std::vector<std::shared_ptr<Stream>>& outputBus = worker->getOutputBus();

            /*
            * Similarly, we look for the given stream in the workflow's stream map.
            * If we find the stream, it also implies its corresponding pointer is
            * not null, as we only insert non-null shared pointers into the maps.
            */
            auto& streamIterator = m_streams.find(streamID);
            if (streamIterator != m_streams.end())
            {
                /*
                * We test outputPortNumber against the size of the worker's output
                * bus:
                */
                if (outputPortNumber < worker->getNumOutputs() && outputBus[outputPortNumber] && outputBus[outputPortNumber]->id == streamID)
                {
                    // NO MEMORY DEALLOCATION
                    worker->disconnectOutput(outputPortNumber);

                    /* The connection is successfull, so we return true */
                    return true;
                }
            }
        }

        /*
        * If we arrive here, this means one of the previous three tests failed,
        * which prevents us from making the disconnection. We therefore stop and
        * return false.
        */
        return false;
    }

    bool Workflow::executeConnectionInstruction(ConnectionInstruction<STREAM_TO_WORKER, PLUG> instruction)
    {
        return plugStreamIntoWorker(instruction.uphillID, instruction.downhillID, instruction.portNumber);
    }

    bool Workflow::executeConnectionInstruction(ConnectionInstruction<WORKER_TO_STREAM, PLUG> instruction)
    {
        return plugWorkerIntoStream(instruction.uphillID, instruction.downhillID, instruction.portNumber);
    }

    bool Workflow::executeConnectionInstruction(ConnectionInstruction<STREAM_TO_WORKER, UNPLUG> instruction)
    {
        return unplugStreamFromWorker(instruction.uphillID, instruction.downhillID, instruction.portNumber);
    }

    bool Workflow::executeConnectionInstruction(ConnectionInstruction<WORKER_TO_STREAM, UNPLUG> instruction)
    {
        return unplugWorkerFromStream(instruction.uphillID, instruction.downhillID, instruction.portNumber);
    }

    bool Workflow::executeConnectionPlan(const ConnectionPlan& plan)
    {
        /* We use a boolean to trace if every connection succeeded */
        bool success = true;

        /*
        * Now we need to execute every connection instruction, and update this
        * boolean after each execution using the && operator. Note that, because of
        * the && operator precedence rule, we need to always put the 'success'
        * variable as the second operand of each AND operation, in order to keep
        * processing instructions even if a trouble was encountered. Otherwise, if
        * success became false at some point, then the expression "success && ..."
        * would never evaluate its second operand, and therefore we would never
        * execute the instruction.
        */

        for (auto& instruction : plan.streamToWorkerUnplugInstructions)
            success = unplugStreamFromWorker(instruction.uphillID, instruction.downhillID, instruction.portNumber) && success;

        for (auto& instruction : plan.workerToStreamUnplugInstructions)
            success = unplugWorkerFromStream(instruction.uphillID, instruction.portNumber, instruction.downhillID) && success;

        for (auto& instruction : plan.streamToWorkerPlugInstructions)
            success = plugStreamIntoWorker(instruction.uphillID, instruction.downhillID, instruction.portNumber) && success;

        for (auto& instruction : plan.workerToStreamPlugInstructions)
            success = plugWorkerIntoStream(instruction.uphillID, instruction.portNumber, instruction.downhillID) && success;

        return success;
    }

    void Workflow::completeRenderingSequenceForWorker(const std::shared_ptr<Worker>& worker, const ConnectionPlan& plan, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const
    {
        /*
        * If worker is a nullptr, or of it is not part of the workflow, then we have
        * nothing to start the computation from, so we simply return here.
        */
        if (!worker || m_workers.find(worker->id) == m_workers.end())
            return;


        for (unsigned short port = 0; port < worker->getNumInputs(); port++)
        {
            /*
            * We first need to check if the port will be plugged into after the
            * connection plan. If so, then no matter if the port is currently taken
            * or not, or what unplug instructions could be executed first, we simply
            * need to retrieve the new stream that will be connected instead. For
            * that, we iterate through the plan in reverse order, so we only take
            * into account the last valid plug instruction.
            */

            auto& streamToWorkerPlugIterator = std::find_if(
                plan.streamToWorkerPlugInstructions.crbegin(),
                plan.streamToWorkerPlugInstructions.crend(),

                /*
                * We use a lambda function to detect if an instruction matches the
                * current worker and port, and check if the instruction is valid
                * (i.e. refers to existing elements in the workflow). We also ensure
                * we capture the proper external variables in the capture list [],
                * while avoiding unecessary copies (worker is passed by reference to
                * avoid a temporary shared pointer reference count increment, for
                * example). Note that the lambda function will check for validity by
                * searching through m_streams only if the instruction matches the
                * current worker and port (due to operator&& precedence), which
                * brings better performance.
                */
                [&worker, port, this](ConnectionInstruction<STREAM_TO_WORKER, PLUG> instruction) { return instruction.downhillID == worker->id && instruction.portNumber == port && m_streams.find(instruction.uphillID) != m_streams.cend(); }
            );

            /* Is the port part of a valid PLUG instruction? ... */
            if (streamToWorkerPlugIterator != plan.streamToWorkerPlugInstructions.crend())
            {
                /*
                * ... YES! The port will receive a new valid stream after the
                * connection plan is executed. Therefore, we need to use that new
                * stream to compute the next part of the renderingSequence.
                */
                auto& streamIterator = m_streams.find(streamToWorkerPlugIterator->uphillID);

                /*
                * Note that once here, since the plug instruction is considered
                * valid only if the stream involved already exists in the workflow,
                * it is guaranteed the research right above succeeded. Therefore, we
                * do not need to test if streamIterator is at the end of m_streams:
                * we know it is not.
                */

                /*
                * We recursively call the current function on the new input stream
                * found, in order to retrieve all of the workers that need to be
                * called to fill in this stream. We will add the worker at the end
                * of this sequence, right after the 'for' loop on ports.
                */
                completeRenderingSequenceForStream(streamIterator->second, plan, currentRenderingSequence);
            }

            /*
            * If the port is not planned to receive a new input stream through a
            * plug instruction, then we need to use the existing stream that is
            * already plugged in to continue, and check if it will be unplugged.
            */
            else
            {
                const std::shared_ptr<const Stream>& stream = worker->getInputBus()[port];

                /* Is the port already TAKEN? ... */
                if (stream)
                {
                    /*
                    * ... YES! So we need to check in the ConnectionPlan if the port
                    * will be unplugged.
                    */

                    auto& streamToWorkerUnplugIterator = std::find_if(
                        plan.streamToWorkerUnplugInstructions.cbegin(),
                        plan.streamToWorkerUnplugInstructions.cend(),

                        /*
                        * We use a lambda function to detect if an instruction
                        * matches the current stream, worker, and port, and refers
                        * to an existing stream. We also ensure we capture the
                        * proper external variables in the capture list [], while
                        * avoiding unecessary copies (worker is passed by reference
                        * to avoid a temporary shared pointer reference count
                        * increment, for example). Note that the lambda function
                        * will check for validity by searching through m_streams
                        * only if the instruction is a perfect match (due to
                        * operator&& precedence), which provides better performance.
                        */
                        [&worker, port, &stream, this](ConnectionInstruction<STREAM_TO_WORKER, UNPLUG> instruction) { return instruction.downhillID == worker->id && instruction.portNumber == port && instruction.uphillID == stream->id && m_streams.find(instruction.uphillID) != m_streams.cend(); }
                    );

                    /* Is the port NOT part of any valid UNPLUG instruction? */
                    if (streamToWorkerUnplugIterator == plan.streamToWorkerUnplugInstructions.cend())
                    {
                        /*
                        * ... YES! So we should use the existing stream to continue
                        * our computation. Note that the stream's existence will be
                        * checked again in the following function call.
                        */
                        completeRenderingSequenceForStream(stream, plan, currentRenderingSequence);
                    }

                    /*
                    * Otherwise, if the port will actually be unplugged and not
                    * reconnected to a new stream, then we have nothing to do, so we
                    * can stop here and wait to move on to the next port.
                    */
                }
            }
        }

        /*
        * Finally, we add the worker to the rendering sequence if it was not already
        * there, and if it is part of the workflow, which we already know.
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

    void Workflow::completeRenderingSequenceForStream(const std::shared_ptr<const Stream>& stream, const ConnectionPlan& plan, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const
    {
        /*
        * If stream is a nullptr, or of it is not part of the workflow, then we have
        * nothing to start the computation from, so we simply return here.
        */
        if (!stream || m_streams.find(stream->id) == m_streams.end())
            return;

        /*
        * We first need to check if a worker will be plugged into the stream after
        * the connection plan. If so, then no matter if a worker is currently
        * plugged in or not, or what unplug instructions could be executed first, we
        * simply need to retrieve the new worker that will be connected instead. For
        * that, we iterate through the plan in reverse order, so we only take into
        * account the last valid plug instruction.
        */

        auto& workerToStreamPlugIterator = std::find_if(
            plan.workerToStreamPlugInstructions.crbegin(),
            plan.workerToStreamPlugInstructions.crend(),

            /*
            * We use a lambda function to detect if an instruction matches the
            * current stream, and check if the instruction is valid (i.e. refers to
            * existing elements in the workflow). We also ensure we capture the
            * proper external variables in the capture list [], while avoiding
            * unecessary copies (stream is passed by reference to avoid a temporary
            * shared pointer reference count increment, for example). Note that the
            * lambda function will check for validity by searching through m_workers
            * only if the instruction matches the current stream (due to operator&&
            * precedence), which provides better performance.
            */
            [&stream, this](ConnectionInstruction<WORKER_TO_STREAM, PLUG> instruction) { return instruction.downhillID == stream->id && m_workers.find(instruction.uphillID) != m_workers.cend(); }
        );

        /* Is the stream part of a valid PLUG instruction? ... */
        if (workerToStreamPlugIterator != plan.workerToStreamPlugInstructions.crend())
        {
            /*
            * ... YES! The stream will be connected to a new input worker after the
            * connection plan is executed. Therefore, we need to use that new worker
            * to compute the next part of the renderingSequence.
            */
            auto& workerIterator = m_workers.find(workerToStreamPlugIterator->uphillID);

            /*
            * Note that once here, since the plug instruction is considered valid
            * only if the worker involved already exists in the workflow, it is
            * guaranteed the research right above succeeded. Therefore, we do not
            * need to test if workerIterator is at the end of m_workers: we know it
            * is not.
            */
            
            /*
            * We recursively call the current function on the worker, in order to
            * retrieve all of the workers that need to be called before it. We will
            * add the worker at the end of this sequence.
            */
            completeRenderingSequenceForWorker(workerIterator->second, plan, currentRenderingSequence);
        }

        /*
        * If the stream is not planned to be connected to a new input worker through
        * a plug instruction, then we need to use the existing input worker that is
        * already plugged in to continue, and check if it will be unplugged
        */
        else
        {
            /*
            * We retrieve the current stream's input worker using the m_inputWorkers
            * attribute, which stores that information.
            */
            auto& inputWorkerIterator = m_inputWorkers.find(stream->id);
            if (inputWorkerIterator != m_inputWorkers.end())
            {
                const std::shared_ptr<Worker>& inputWorker = inputWorkerIterator->second;

                /*
                * Note that once here, since we found the input worker in the map,
                * it is guaranteed inputWorker is not a null pointer, as we only
                * insert non-null shared pointers into the workflow's maps.
                */

                /* We need to check in the ConnectionPlan if the input worker will
                * be disconnected from the stream.
                */

                auto& workerToStreamUnplugIterator = std::find_if(
                    plan.workerToStreamUnplugInstructions.cbegin(),
                    plan.workerToStreamUnplugInstructions.cend(),

                    /*
                    * We use a lambda function to detect if an unplug instruction
                    * matches the current worker and stream at a valid port, and
                    * refers to an existing worker. We also ensure we capture the
                    * proper external variables in the capture list [], while
                    * avoiding unecessary copies (inputWorker is passed by reference
                    * to avoid a temporary shared pointer reference count increment,
                    * for example). Note that the lambda function will check for
                    * validity by searching through m_workers only if the
                    * instruction is a perfect match (due to operator&& precedence),
                    * which provides better performance.
                    */
                    [&inputWorker, &stream, this](ConnectionInstruction<WORKER_TO_STREAM, UNPLUG> instruction) { return instruction.downhillID == stream->id && instruction.uphillID == inputWorker->id && instruction.portNumber < inputWorker->getNumOutputs() && inputWorker->getOutputBus()[instruction.portNumber] && inputWorker->getOutputBus()[instruction.portNumber]->id == stream->id && m_streams.find(instruction.uphillID) != m_streams.cend(); }
                );

                /* Is the stream NOT part of any valid UNPLUG instruction? */
                if (workerToStreamUnplugIterator == plan.workerToStreamUnplugInstructions.cend())
                {
                    /*
                    * ... YES! So we should use its existing input worker to
                    * continue our computation. Note that the stream's existence
                    * will be checked again in the following function call.
                    */
                    completeRenderingSequenceForWorker(inputWorker, plan, currentRenderingSequence);
                }

                /*
                * Otherwise, if the stream will actually be disconnected from its
                * input worker without any replacement, then we have nothing to do,
                * so we can stop here.
                */
            }
        }
    }
}