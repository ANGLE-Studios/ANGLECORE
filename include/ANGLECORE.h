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
#include <memory>
#include <vector>
#include <unordered_map>

namespace ANGLECORE
{

    /*
    =================================================
    Workflow
    =================================================
    */


    /**
    * \struct WorkflowItem WorkflowItem.h
    * Item of a workflow, with a unique ID.
    */
    struct WorkflowItem
    {
        /** The ID of the workflow item. */
        const uint32_t id;

        /**
        * The constructor simply defines an ID for the new item, using a static
        * integer variable.
        */
        WorkflowItem();

    private:

        /**
        * This static variable is used to define a unique ID for each item in the
        * workflow. Whenever an item is created, this integer is incremented. Note
        * that using this method for creating IDs exposes our system to possible ID
        * duplicates, especially with overflow. This should not a problem here:
        * since the upper limit of uint32_t is approximately 4 billions, we are
        * protected against 4 billions item creations, so we should be safe.
        */
        static uint32_t nextId;
    };

    /**
    * \class Stream Stream.h
    * Owner of a data stream used in the rendering process. The class implements
    * RAII.
    */
    class Stream :
        public WorkflowItem
    {
    public:

        /**
        * Creates a stream of constant size for rendering.
        */
        Stream();

        /**
        * Delete the copy constructor.
        */
        Stream(const Stream& other) = delete;

        /**
        * Delete the stream and its internal buffer.
        */
        ~Stream();

        /** Provides a read access to the internal buffer. */
        const double* const getDataForReading() const;

        /** Provides a write access to the internal buffer. */
        double* const getDataForWriting();

    private:

        /** Internal buffer */
        double* data;
    };

    /**
    * \class Worker Worker.h
    * Represents an agent that processes input streams into output streams. This is
    * an abstract class.
    */
    class Worker :
        public WorkflowItem
    {
    public:

        /**
        * Resizes the internal vectors of streams, and initializes all the Stream
        * pointers to nullptr. To be fully operational, the WorkflowManager needs to
        * connect the Worker's input and output to valid streams.
        * @param[in] numInputs Number of inputs.
        * @param[in] numOutputs Number of output.
        */
        Worker(unsigned short numInputs, unsigned short numOutputs);

        /** Returns the number of inputs in the input bus */
        unsigned short getNumInputs() const;

        /** Returns the number of outputs in the output bus */
        unsigned short getNumOutputs() const;

        /**
        * Connects the Worker's input at \p index to the given Stream. Note that if
        * an input Stream was already connected at the given index in the input bus,
        * it will be replaced by the new one.
        * @param[in] index Which index to connect the stream to within the input
        *   bus.
        * @param[in] newOutputStream A pointer to the new input stream to work from.
        */
        void connectInput(unsigned short index, std::shared_ptr<const Stream> newInputStream);

        /**
        * Connects the Worker's output at \p index to the given Stream. Note that if
        * an output Stream was already connected at the given index in the output
        * bus, it will be replaced by the new one.
        * @param[in] index Which index to connect the stream to within the output
        *   bus.
        * @param[in] newOutputStream A pointer to the new output stream to work
        *   into.
        */
        void connectOutput(unsigned short index, std::shared_ptr<Stream> newOutputStream);

        /**
        * Disconnects any Stream previously connected to the Worker's input bus at
        * the given \p inputPortNumber. This method could cause memory deallocation
        * if the Stream only existed within the Worker's input bus, but this should
        * never be the case.
        * @param[in] inputPortNumber Index to remove the connection from
        */
        void disconnectInput(unsigned short inputPortNumber);

        /**
        * Disconnects any Stream previously connected to the Worker's output bus at
        * the given \p outputPortNumber. This method could cause memory deallocation
        * if the Stream only existed within the Worker's input bus, but this should
        * never be the case.
        * @param[in] outputPortNumber Index to remove the connection from
        */
        void disconnectOutput(unsigned short outputPortNumber);

        /**
        * Provides a read only access to the Stream at \p index in the input bus.
        * @param[in] index Index of the stream within the input bus.
        */
        const double* const getInputStream(unsigned short index) const;

        /**
        * Provides a write access to the Stream at \p index in the output bus.
        * @param[in] index Index of the stream within the output bus.
        */
        double* getOutputStream(unsigned short index) const;

        /**
        * Returns a vector containing all the input streams the worker is connected
        * to. The vector may contain null pointers when no stream is attached.
        */
        const std::vector<std::shared_ptr<const Stream>>& getInputBus() const;

        /**
        * Returns a vector containing all the output streams the worker is connected
        * to. The vector may contain null pointers when no stream is attached.
        */
        const std::vector<std::shared_ptr<Stream>>& getOutputBus() const;

        /**
        * Returns true if the input bus is empty, meaning the Worker is actually a
        * generator, and false otherwise.
        */
        bool hasInputs() const;

        /**
        * Computes the values of every output Stream based on the input streams.
        * This method should be overriden in each sub-class to perform rendering.
        * Note that this method should be really fast, and lock-free. Also note this
        * is the only method of this class that can be called by the real-time
        * thread.
        * @param[in] numSamplesToWorkOn number of samples to generate or process.
        *   This should always be less than or equal to the renderer's buffer size.
        */
        virtual void work(unsigned int numSamplesToWorkOn) = 0;

    private:
        const unsigned short m_numInputs;
        const unsigned short m_numOutputs;
        std::vector<std::shared_ptr<const Stream>> m_inputBus;
        std::vector<std::shared_ptr<Stream>> m_outputBus;
        const bool m_hasInputs;
    };

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
        * Returns true if the Workflow contains a Stream with the given ID, and
        * false otherwise.
        * @param[in] streamID The ID of the Stream to look for
        */
        bool containsStream(uint32_t streamID);

        /**
        * Returns true if the Workflow contains a Worker with the given ID, and
        * false otherwise.
        * @param[in] workerID The ID of the Worker to look for
        */
        bool containsWorker(uint32_t workerID);

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
        bool executeConnectionPlan(const ConnectionPlan& plan);

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
        * @param[in] plan The ConnectionPlan that will be executed next, and which
        *   should be therefore taken into account in the computation
        * @param[out] currentRenderingSequence The output sequence of the
        *   computation, which is recursively filled up.
        */
        void completeRenderingSequenceForWorker(const std::shared_ptr<Worker>& worker, const ConnectionPlan& plan, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const;

        /**
        * Computes the chain of workers that must be called to fill up a given
        * \p stream. This method simply retrieve the Stream's input Worker, and call
        * completeRenderingSequenceForWorker() with that worker.
        * @param[in] stream The Stream to start the computation from. The function
        *   will actually compute which Worker should be called and in which order
        *   to render \p stream.
        * @param[in] plan The ConnectionPlan that will be executed next, and which
        *   should be therefore taken into account in the computation
        * @param[out] currentRenderingSequence The output sequence of the
        *   computation, which is recursively filled up.
        */
        void completeRenderingSequenceForStream(const std::shared_ptr<const Stream>& stream, const ConnectionPlan& plan, std::vector<std::shared_ptr<Worker>>& currentRenderingSequence) const;

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



    /*
    =================================================
    AudioWorkflow
    =================================================
    */

    #define ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS 2
    #define ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN 0.5

    /**
    * \class Exporter Exporter.h
    * Worker that exports data from its input streams into an output buffer sent
    * to the host. An exporter applies a gain for calibrating its output level.
    */
    template<typename OutputType>
    class Exporter :
        public Worker
    {
    public:

        /**
        * Creates a Worker with zero output.
        */
        Exporter() :
            Worker(ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS, 0),
            m_outputBuffer(nullptr),
            m_numOutputChannels(0)
        {}

        /**
        * Sets the new memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        * @param[in] numChannels Number of output channels.
        */
        void setOutputBuffer(OutputType** buffer, unsigned short numChannels, uint32_t startSample)
        {
            m_outputBuffer = buffer;
            m_numOutputChannels = numChannels;
            m_startSample = startSample;
        }

        void work(unsigned int numSamplesToWorkOn)
        {
            /*
            * It is assumed that both m_numOutputChannels and numSamplesToWorkOn
            * are in-range, i.e. less than or equal to the output buffer's
            * number of channels and the stream and buffer size respectively. It
            * is also assumed the output buffer has been properly set to a valid
            * memory location.
            */

            /*
            * If the host request less channels than rendered, we sum their
            * content using a modulo approach.
            */
            if (m_numOutputChannels < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS)
            {
                /* We first clear the output buffer */
                for (unsigned short c = 0; c < m_numOutputChannels; c++)
                    for (uint32_t i = m_startSample; i < m_startSample + numSamplesToWorkOn; i++)
                        m_outputBuffer[c][i] = 0.0;

                /* And then we compute the sum into the output buffer */
                for (unsigned short c = 0; c < ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS; c++)
                    for (uint32_t i = m_startSample; i < m_startSample + numSamplesToWorkOn; i++)
                        m_outputBuffer[c % m_numOutputChannels][i] += static_cast<OutputType>(getInputStream(c)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
            }

            /*
            * Otherwise, if we have not rendered enough channels for the host,
            * we simply duplicate data.
            */
            else
            {
                for (unsigned int c = 0; c < m_numOutputChannels; c++)
                    for (uint32_t i = m_startSample; i < m_startSample + numSamplesToWorkOn; i++)
                        m_outputBuffer[c][i] = static_cast<OutputType>(getInputStream(c % ANGLECORE_AUDIOWORKFLOW_NUM_CHANNELS)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
            }
        }

    private:
        OutputType** m_outputBuffer;
        unsigned short m_numOutputChannels;
        uint32_t m_startSample;
    };

    /**
    * \class Mixer Mixer.h
    * Worker that sums all of its non-nullptr input stream, based on the audio
    * channel they each represent.
    */
    class Mixer :
        public Worker
    {
    public:

        /**
        * Initializes the Worker's buses size according to the audio
        * configuration (number of channels, number of instruments...)
        */
        Mixer();

        /**
        * Mixes all the channels together
        */
        void work(unsigned int numSamplesToWorkOn);

    private:
        const unsigned short m_totalNumInstruments;
    };

    /**
    * \class Builder Builder.h
    * Abstract class representing an object that is able to build worfklow items.
    */
    class Builder
    {
        /**
        * \struct WorkflowIsland Builder.h
        * Isolated subset of a workflow, which is not connected to the real-time
        * rendering pipeline yet, but will be connected to the whole workflow by the
        * real-time thread.
        */
        struct WorkflowIsland
        {
            std::vector<std::shared_ptr<Stream>> streams;
            std::vector<std::shared_ptr<Worker>> workers;
        };

        /* We rely on the default constructor */
        
        /**
        * Builds and returns a WorkflowIsland for a Workflow to integrate. This
        * method should be overriden in each sub-class to construct the appropriate
        * WorkflowIsland.
        */
        virtual std::shared_ptr<WorkflowIsland> build() = 0;
    };

    /**
    * \class AudioWorkflow AudioWorkflow.h
    * Workflow internally structured to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class AudioWorkflow :
        public Workflow
    {
    public:
        /** Builds the base structure of the AudioWorkflow (Exporter, Mixer...) */
        AudioWorkflow();

        /**
        * Builds and returns the Workflow's rendering sequence, starting from its
        * Exporter. This method allocates memory, so it should never be called by
        * the real-time thread. Note that the method relies on the move semantics to
        * optimize its vector return.
        */
        std::vector<std::shared_ptr<Worker>> buildRenderingSequence(const ConnectionPlan& connectionPlan) const;

    private:
        std::shared_ptr<Exporter<float>> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
    };
}