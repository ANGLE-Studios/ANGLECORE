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

namespace ANGLECORE
{
    /**
    * Represents a set of instructions to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class Workflow
    {
    public:

        /**
        * Adds the given Stream into the Workflow, and updates the workflow's
        * internal ID-Stream map accordingly. Note that streams are never created by
        * the Workflow itself, but rather passed as arguments after being created by
        * a dedicated entity: the WorkflowDesigner.
        * @param[in] streamToAdd The Stream to add into the Workflow.
        */
        void addStream(const std::shared_ptr<Stream>& streamToAdd);

        /**
        * Adds the given Worker into the Workflow, and updates the workflow's
        * internal ID-Worker map accordingly. Note that workers are never created by
        * the Workflow itself, but rather passed as arguments after being created by
        * a dedicated entity: the WorkflowDesigner.
        * @param[in] workerToAdd The Worker to add into the Workflow.
        */
        void addWorker(const std::shared_ptr<Worker>& workerToAdd);

        /**
        * Connects a Stream into a Worker's input bus, at the given \p
        * inputStreamNumber. If a Stream was already connected at this index, it
        * will be replaced. Returns true if the connection succeeded, and false
        * otherwise. The connection can fail when the Stream or the Worker cannot be
        * found in the Workflow, or when the \p inputStreamNumber is out-of-range.
        * Note that, just like any method that modifies the Workflow's connections,
        * this method is not meant to be fast at all, as it will be called by non
        * real-time threads only (most likely by a single UI thread). Therefore, in
        * the absence of any speed requirement, the Stream and Worker are passed in
        * using their IDs, which will cost an extra search before actually creating
        * the connection.
        * @param[in] streamID The ID of the Stream to connect to the Worker's input
        *   bus. This ID should match a Stream that already exists in the Workflow.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        * @param[in] inputStreamNumber The index of the input Stream to replace in
        *   the Worker's input bus.
        */
        bool plugStreamIntoWorker(uint32_t streamID, uint32_t workerID, unsigned short inputStreamNumber);

        /**
        * Connects a Stream to a Worker's output bus, at the given \p
        * outputStreamNumber. If a Stream was already connected at this index, it
        * will be replaced. Returns true if the connection succeeded, and false
        * otherwise. The connection can fail when the Stream or the Worker cannot be
        * found in the Workflow, or when the \p outputStreamNumber is out-of-range.
        * Note that, just like any method that modifies the Workflow's connections,
        * this method is not meant to be fast at all, as it will be called by non
        * real-time threads only (most likely by a single UI thread). Therefore, in
        * the absence of any speed requirement, the Stream and Worker are passed in
        * using their IDs, which will cost an extra search before actually creating
        * the connection.
        * @param[in] workerID The ID of the Worker to connect to the Stream. This ID
        *   should match a Worker that already exists in the Workflow.
        * @param[in] outputStreamNumber The index of the output Stream to replace in
        *   the Worker's output bus.
        * @param[in] streamID The ID of the Stream to connect to the Worker's output
        *   bus. This ID should match a Stream that already exists in the Workflow.
        */
        bool plugWorkerIntoStream(uint32_t workerID, unsigned short outputStreamNumber, uint32_t streamID);

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
        std::unordered_map<uint32_t, std::shared_ptr<const Worker>> m_map;
    };
}