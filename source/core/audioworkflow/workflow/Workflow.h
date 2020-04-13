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

#include "Stream.h"
#include "Worker.h"
#include "ConnectionPlan.h"

namespace ANGLECORE
{
    /**
    * \class Workflow Workflow.h
    * Represents a set of instructions as a succession of workers and streams. A
    * Workflow should not contain any feedback.
    */
    class Workflow
    {
    public:

        /**
        * Adds the given Stream into the Workflow, and updates the workflow's
        * internal ID-Stream map accordingly. Note that streams are never created by
        * the Workflow itself, but rather passed as arguments after being created by
        * a dedicated entity.
        * @param[in] streamToAdd The Stream to add into the Workflow.
        */
        void addStream(const std::shared_ptr<Stream>& streamToAdd);

        /**
        * Adds the given Worker into the Workflow, and updates the workflow's
        * internal ID-Worker map accordingly. Note that workers are never created by
        * the Workflow itself, but rather passed as arguments after being created by
        * a dedicated entity.
        * @param[in] workerToAdd The Worker to add into the Workflow.
        */
        void addWorker(const std::shared_ptr<Worker>& workerToAdd);

        /**
        * Connects a Stream to a Worker's input bus, at the given \p
        * inputPortNumber. If a Stream was already connected at this port, it will
        * be replaced. Returns true if the connection succeeded, and false
        * otherwise. The connection can fail when the Stream or the Worker cannot be
        * found in the Workflow, or when the \p inputPortNumber is out-of-range.
        * Note that the Stream and Worker are passed in using their IDs, which will
        * cost an extra search before actually creating the connection.
        * @param[in] streamID The ID of the Stream to connect to the Worker's input
        *   bus. This ID should match a Stream that already exists in the Workflow.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        * @param[in] inputPortNumber The index of the input Stream to replace in the
        *   Worker's input bus.
        */
        bool plugStreamIntoWorker(uint32_t streamID, uint32_t workerID, unsigned short inputPortNumber);

        /**
        * Connects a Stream to a Worker's output bus, at the given \p
        * outputPortNumber. If a Stream was already connected at this port, it will
        * be replaced. Returns true if the connection succeeded, and false
        * otherwise. The connection can fail when the Stream or the Worker cannot be
        * found in the Workflow, or when the \p outputPortNumber is out-of-range.
        * Note that the Stream and Worker are passed in using their IDs, which will
        * cost an extra search before actually creating the connection.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        * @param[in] outputPortNumber The index of the output Stream to replace in
        *   the Worker's output bus.
        * @param[in] streamID The ID of the Stream to connect to the Worker's output
        *   bus. This ID should match a Stream that already exists in the Workflow.
        */
        bool plugWorkerIntoStream(uint32_t workerID, unsigned short outputPortNumber, uint32_t streamID);

        /**
        * Disconnects a Stream from a Worker's input bus, if and only if it was
        * connected at the given \p inputPortNumber before. Returns true if the
        * disconnection succeeded, and false otherwise. The disconnection can fail
        * when the Stream or the Worker cannot be found in the Workflow, when the \p
        * inputPortNumber is out-of-range, or when the aforementioned condition does
        * not hold. In the latter case, if the Stream is not connected to the Worker
        * on the given port (for example when another Stream is connected instead,
        * or when no Stream is plugged in), then this method will have no effect and
        * the disconnection will fail. Note that the Stream and Worker are passed in
        * using their IDs, which will cost an extra search before actually creating
        * the connection.
        * @param[in] streamID The ID of the Stream to connect to the Worker's input
        *   bus. This ID should match a Stream that already exists in the Workflow.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        * @param[in] inputPortNumber The index of the input Stream to unplug from
        *   the Worker's input bus.
        */
        bool unplugStreamFromWorker(uint32_t streamID, uint32_t workerID, unsigned short inputPortNumber);

        /**
        * Disconnects a Stream from a Worker's output bus, if and only if it was
        * connected at the given \p outputPortNumber before. Returns true if the
        * disconnection succeeded, and false otherwise. The disconnection can fail
        * when the Stream or the Worker cannot be found in the Workflow, when the \p
        * outputPortNumber is out-of-range, or when the aforementioned condition
        * does not hold. In the latter case, if the Stream is not connected to the
        * Worker on the given port (for example when another Stream is connected
        * instead, or when no Stream is plugged in), then this method will have no
        * effect and the disconnection will fail. Note that the Stream and Worker
        * are passed in using their IDs, which will cost an extra search before
        * actually creating the connection.
        * @param[in] streamID The ID of the Stream to connect to the Worker's input
        *   bus. This ID should match a Stream that already exists in the Workflow.
        * @param[in] outputPortNumber The index of the output Stream to replace in
        *   the Worker's input bus.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        */
        bool unplugWorkerFromStream(uint32_t workerID, unsigned short outputPortNumber, uint32_t streamID);

        /**
        * Executes the given instruction, and connects a Stream to a Worker's input
        * bus. Returns true if the connection succeeded, and false otherwise. This
        * method simply unfolds its argument and forwards it to the
        * plugStreamIntoWorker() method.
        * @param[in] instruction The instruction to execute. It should refer to
        *   existing elements in the Workflow, as the execution will fail otherwise.
        */
        bool executeConnectionInstruction(ConnectionInstruction<STREAM_TO_WORKER, PLUG> instruction);

        /**
        * Executes the given instruction, and connects one Worker's output to a
        * Stream. Returns true if the connection succeeded, and false otherwise.
        * This method simply unfolds its argument and forwards it to the
        * plugWorkerIntoStream() method.
        * @param[in] instruction The instruction to execute. It should refer to
        *   existing elements in the Workflow, as the execution will fail otherwise.
        */
        bool executeConnectionInstruction(ConnectionInstruction<WORKER_TO_STREAM, PLUG> instruction);

        /**
        * Executes the given instruction, and disconnects a Stream from a Worker's
        * input bus. Returns true if the disconnection succeeded, and false
        * otherwise. This method simply unfolds its argument and forwards it to the
        * unplugStreamFromWorker() method.
        * @param[in] instruction The instruction to execute. It should refer to
        *   existing elements in the Workflow, as the execution will fail otherwise.
        */
        bool executeConnectionInstruction(ConnectionInstruction<STREAM_TO_WORKER, UNPLUG> instruction);

        /**
        * Executes the given instruction, and disconnects one Worker's output from a
        * Stream. Returns true if the disconnection succeeded, and false otherwise.
        * This method simply unfolds its argument and forwards it to the
        * unplugWorkerFromStream() method.
        * @param[in] instruction The instruction to execute. It should refer to
        *   existing elements in the Workflow, as the execution will fail otherwise.
        */
        bool executeConnectionInstruction(ConnectionInstruction<WORKER_TO_STREAM, UNPLUG> instruction);

        /**
        * Executes the given \p plan, and creates and deletes connections as
        * instructed. Returns true if all the connection instructions were processed
        * successfully, and false otherwise, that is if at least one instruction
        * failed. This method needs to be fast, as it may be called by the real-time
        * thread at the beginning of each rendering session. Note that, for every
        * ConnectionInstruction encountered in the plan, this method will call the
        * plug() and unplug() methods instead of the executeConnectionInstruction()
        * methods to save up one callback layer.
        * @param[in] plan The ConnectionPlan to execute. It should refer to existing
        *   elements in the Workflow, as the execution will partially fail
        *   otherwise.
        */
        bool executeConnectionPlan(ConnectionPlan plan);

    protected:

        /**
        * This method is a recursive utility function for exploring the Workflow's
        * tree-like structure. It computes the chain of workers that must be called
        * before calling a given \p worker, and appends all of the workers
        * encountered (including the provided \p worker) at the end of a given
        * sequence. The calculation keeps track of the order with which the workers
        * should be called for the rendering to be successful, which is why the
        * result is called a "Rendering Sequence".
        * @param[in] worker The worker to start the computation from. The function
        *   will actually compute which Worker should be called and in which order
        *   to render every input of \p worker.
        * @param[out] currentRenderingSequence The output sequence of the
        *   computation, which is recursively filled up.
        */
        void completeRenderingSequenceForWorker(const std::shared_ptr<Worker>& worker, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const;

        /**
        * Computes the chain of workers that must be called to fill up a given
        * \p stream. This method simply retrieve the Stream's input Worker, and call
        * completeRenderingSequenceForWorker() with that worker.
        * @param[in] stream The Stream to start the computation from. The function
        *   will actually compute which Worker should be called and in which order
        *   to render \p stream.
        * @param[out] currentRenderingSequence The output sequence of the
        *   computation, which is recursively filled up.
        */
        void completeRenderingSequenceForStream(const std::shared_ptr<const Stream>& stream, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const;

    private:

        /**
        * Maps a Stream with its ID, hereby providing an ID-based access to a Stream
        * in constant time. This unordered_map will always verify:
        * m_streams[i]->id == i.
        */
        std::unordered_map<uint32_t, std::shared_ptr<Stream>> m_streams;

        /**
        * Maps a Worker with its ID, hereby providing an ID-based access to a Worker
        * in constant time. This unordered_map will always verify:
        * m_workers[i]->id == i.
        */
        std::unordered_map<uint32_t, std::shared_ptr<Worker>> m_workers;

        /** Maps a Stream ID to its input worker */
        std::unordered_map<uint32_t, std::shared_ptr<Worker>> m_inputWorkers;
    };
}