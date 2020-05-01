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
#include <atomic>
#include <cassert>
#include <thread>
#include <array>

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
        * Deletes the stream and its internal buffer.
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
        * Provides a read-only access to the Stream at \p index in the input bus.
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

    #define ANGLECORE_NUM_CHANNELS 2
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
            Worker(ANGLECORE_NUM_CHANNELS, 0),
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
            if (m_numOutputChannels < ANGLECORE_NUM_CHANNELS)
            {
                /* We first clear the output buffer */
                for (unsigned short c = 0; c < m_numOutputChannels; c++)
                    for (uint32_t i = m_startSample; i < m_startSample + numSamplesToWorkOn; i++)
                        m_outputBuffer[c][i] = 0.0;

                /* And then we compute the sum into the output buffer */
                for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
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
                        m_outputBuffer[c][i] = static_cast<OutputType>(getInputStream(c % ANGLECORE_NUM_CHANNELS)[i] * ANGLECORE_AUDIOWORKFLOW_EXPORTER_GAIN);
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



    /*
    =================================================
    VoiceAssigner
    =================================================
    */

    /**
    * \struct VoiceAssignment VoiceAssigner.h
    * Represents an item in a voice sequence.
    */
    struct VoiceAssignment
    {
        bool isNull;
        unsigned short voiceNumber;

        VoiceAssignment(bool isNull, unsigned short voiceNumber);
    };

    /**
    * \class VoiceAssigner VoiceAssigner.h
    * Entity that assigns workers to voices, and keep track of its assingments
    * internally.
    */
    class VoiceAssigner
    {
    public:

        /**
        * Maps a Worker to a Voice, so that the Worker will only be called if the
        * Voice is on at runtime. If a Worker is not part of any Voice, it will be
        * considered global, and will always be rendered. Note that the
        * \p voiceNumber is expected to be in-range. No safety checks will be
        * performed, mostly to increase speed.
        * @param[in] voiceNumber Unique number that identifies the Voice \p worker
        *   should be mapped to
        * @param[in] workerID ID of the Worker to map
        */
        void assignVoiceToWorker(unsigned short voiceNumber, uint32_t workerID);

        /**
        * Revokes every assignment the given Worker was related to. If the Worker
        * was not assigned a Voice, this method will simply return without any side
        * effect.
        * @param[in] workerID ID of the Worker to unmap
        */
        void revokeAssignments(uint32_t workerID);

        /**
        * Returns the voices assigned to each Worker in the \p workers vector. The
        * vector returned will be of the same size as the input vector. If a Worker
        * was not assigned any Voice, its corresponding VoiceAssignment will be
        * null. Note that this method relies on move semantics for efficiency.
        * @param[in] workers Vector of workers to retrieve the assigned voices from.
        *   The vector should not contain any null pointer, and should preferably be
        *   the rendering sequence of an AudioWorkflow.
        */
        std::vector<VoiceAssignment> getVoiceAssignments(const std::vector<std::shared_ptr<Worker>>& workers) const;

    private:

        /**
        * Maps a Worker to its assigned Voice, if relevant. Workers are referred to
        * with their ID, and voices with their number.
        */
        std::unordered_map<uint32_t, unsigned short> m_voiceAssignments;
    };



    /*
    =================================================
    Renderer
    =================================================
    */

    #define ANGLECORE_NUM_VOICES 32

    namespace farbot
    {
        namespace detail { template <typename, bool, bool, bool, bool, std::size_t> class fifo_impl; }

        namespace fifo_options
        {
            enum class concurrency
            {
                single,                  // single consumer/single producer
                multiple
            };

            enum class full_empty_failure_mode
            {
                // Setting this as producer option causes the fifo to overwrite on a push when the fifo is full
                // Setting this as consumer option causes the fifo to return a default constructed value on pop when the fifo is empty
                overwrite_or_return_default,

                // Return false on push/pop if the fifo is full/empty respectively
                return_false_on_full_or_empty
            };
        }

        // single consumer, single producer
        template <typename T,
            fifo_options::concurrency consumer_concurrency = fifo_options::concurrency::multiple,
            fifo_options::concurrency producer_concurrency = fifo_options::concurrency::multiple,
            fifo_options::full_empty_failure_mode consumer_failure_mode = fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            fifo_options::full_empty_failure_mode producer_failure_mode = fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            std::size_t MAX_THREADS = 64>
        class fifo
        {
        public:
            fifo(int capacity);

            bool push(T&& result);

            bool pop(T& result);

        private:
            detail::fifo_impl<T,
                consumer_concurrency == fifo_options::concurrency::single,
                producer_concurrency == fifo_options::concurrency::single,
                consumer_failure_mode == fifo_options::full_empty_failure_mode::overwrite_or_return_default,
                producer_failure_mode == fifo_options::full_empty_failure_mode::overwrite_or_return_default,
                MAX_THREADS> impl;
        };
    }

    //==============================================================================

    namespace farbot
    {
        namespace detail
        {
            template <typename T, bool isWrite, bool swap_when_read> struct fifo_manip { static void access(T && slot, T&& arg) { slot = std::move(arg); } };
            template <typename T> struct fifo_manip<T, false, false>     { static void access(T && slot, T&& arg) { arg = std::move(slot); } };
            template <typename T> struct fifo_manip<T, false, true>     { static void access(T && slot, T&& arg)  { arg = T(); std::swap(slot, arg); } };

            struct thread_info
            {
                std::atomic<std::thread::id> tid = {};
                std::atomic<uint32_t> pos = { std::numeric_limits<uint32_t>::max() };

                thread_info()
                {
                    // We moved the initial lock-free assertion on thread::id here
                    assert(tid.is_lock_free());
                }
            };

            template <std::size_t MAX_THREADS>
            struct multi_position_info
            {
                std::atomic<uint32_t> num_threads = { 0 };
                std::array<thread_info, MAX_THREADS> tinfos = { {} };

                std::atomic<uint32_t>& get_tpos()
                {
                    auto my_tid = std::this_thread::get_id();
                    auto num = num_threads.load(std::memory_order_relaxed);

                    for (auto it = tinfos.begin(); it != tinfos.begin() + num; ++it)
                        if (it->tid.load(std::memory_order_relaxed) == my_tid)
                            return it->pos;

                    auto pos = num_threads.fetch_add(1, std::memory_order_relaxed);

                    if (pos >= MAX_THREADS)
                    {
                        assert(false);
                        // do something!
                        return tinfos.front().pos;
                    }

                    auto& result = tinfos[pos];
                    result.tid.store(my_tid, std::memory_order_relaxed);

                    return result.pos;
                }

                uint32_t getpos(uint32_t min) const
                {
                    auto num = num_threads.load(std::memory_order_relaxed);

                    for (auto it = tinfos.begin(); it != tinfos.begin() + num; ++it)
                        min = std::min(min, it->pos.load(std::memory_order_acquire));

                    return min;
                }
            };

            template <typename T, bool is_writer,
                bool single_consumer_producer,
                bool overwrite_or_return_zero,
                std::size_t MAX_THREADS>
            struct read_or_writer
            {
                uint32_t getpos() const
                {
                    return posinfo.getpos(reserve.load(std::memory_order_relaxed));
                }

                bool push_or_pop(std::vector<T>& s, T && arg, uint32_t max)
                {
                    auto& tpos = posinfo.get_tpos();
                    auto pos = reserve.load(std::memory_order_relaxed);

                    if (pos >= max)
                        return false;

                    tpos.store(pos, std::memory_order_release);

                    if (!reserve.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        do
                        {
                            if (pos >= max)
                            {
                                tpos.store(std::numeric_limits<uint32_t>::max(), std::memory_order_relaxed);
                                return false;
                            }
                        } while (!reserve.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed));

                        tpos.store(pos, std::memory_order_release);
                    }

                    detail::fifo_manip<T, is_writer, false>::access(std::move(s[pos & (s.size() - 1)]), std::move(arg));
                    tpos.store(std::numeric_limits<uint32_t>::max(), std::memory_order_release);
                    return true;
                }

                std::atomic<uint32_t> reserve = { 0 };
                multi_position_info<MAX_THREADS> posinfo;
            };

            template <typename T, bool is_writer, std::size_t MAX_THREADS>
            struct read_or_writer<T, is_writer, true, false, MAX_THREADS>
            {
                uint32_t getpos() const
                {
                    return reserve.load(std::memory_order_acquire);
                }

                bool push_or_pop(std::vector<T>& s, T && arg, uint32_t max)
                {
                    auto pos = reserve.load(std::memory_order_relaxed);

                    if (pos >= max)
                        return false;

                    detail::fifo_manip<T, is_writer, false>::access(std::move(s[pos & (s.size() - 1)]), std::move(arg));
                    reserve.store(pos + 1, std::memory_order_release);

                    return true;
                }

                std::atomic<uint32_t> reserve = { 0 };
            };

            template <typename T, bool is_writer, std::size_t MAX_THREADS>
            struct read_or_writer<T, is_writer, true, true, MAX_THREADS>
            {
                uint32_t getpos() const
                {
                    return reserve.load(std::memory_order_acquire);
                }

                bool push_or_pop(std::vector<T>& s, T && arg, uint32_t)
                {
                    auto pos = reserve.load(std::memory_order_relaxed);

                    detail::fifo_manip<T, is_writer, true>::access(std::move(s[pos & (s.size() - 1)]), std::move(arg));
                    reserve.store(pos + 1, std::memory_order_release);

                    return true;
                }

                std::atomic<uint32_t> reserve = { 0 };
            };

            template <typename T, bool is_writer, std::size_t MAX_THREADS>
            struct read_or_writer<T, is_writer, false, true, MAX_THREADS>
            {
                uint32_t getpos() const
                {
                    return posinfo.getpos(reserve.load(std::memory_order_relaxed));
                }

                bool push_or_pop(std::vector<T>& s, T && arg, uint32_t)
                {
                    auto& tpos = posinfo.get_tpos();
                    auto pos = reserve.fetch_add(1, std::memory_order_relaxed);

                    tpos.store(pos, std::memory_order_release);
                    detail::fifo_manip<T, is_writer, true>::access(std::move(s[pos & (s.size() - 1)]), std::move(arg));
                    tpos.store(std::numeric_limits<uint32_t>::max(), std::memory_order_release);

                    return true;
                }

                multi_position_info<MAX_THREADS> posinfo;
                std::atomic<uint32_t> reserve = { 0 };
            };

            template <typename T, bool consumer_concurrency, bool producer_concurrency,
                bool consumer_failure_mode, bool producer_failure_mode, std::size_t MAX_THREADS>
            class fifo_impl
            {
            public:
                fifo_impl(int capacity) : slots(capacity)
                {
                    assert((capacity & (capacity - 1)) == 0);
                }

                bool push(T&& result)
                {
                    return writer.push_or_pop(slots, std::move(result), reader.getpos() + static_cast<uint32_t> (slots.size()));
                }

                bool pop(T& result)
                {
                    return reader.push_or_pop(slots, std::move(result), writer.getpos());
                }

            private:
                //==============================================================================
                std::vector<T> slots = {};

                read_or_writer<T, false, consumer_concurrency, consumer_failure_mode, MAX_THREADS> reader;
                read_or_writer<T, true, producer_concurrency, producer_failure_mode, MAX_THREADS> writer;
            };
        } // detail

        template <typename T,
            fifo_options::concurrency consumer_concurrency,
            fifo_options::concurrency producer_concurrency,
            fifo_options::full_empty_failure_mode consumer_failure_mode,
            fifo_options::full_empty_failure_mode producer_failure_mode,
            std::size_t MAX_THREADS>
            fifo<T, consumer_concurrency, producer_concurrency, consumer_failure_mode, producer_failure_mode, MAX_THREADS>::fifo(int capacity) : impl(capacity) {}

        template <typename T,
            fifo_options::concurrency consumer_concurrency,
            fifo_options::concurrency producer_concurrency,
            fifo_options::full_empty_failure_mode consumer_failure_mode,
            fifo_options::full_empty_failure_mode producer_failure_mode,
            std::size_t MAX_THREADS>
            bool fifo<T, consumer_concurrency, producer_concurrency, consumer_failure_mode, producer_failure_mode, MAX_THREADS>::push(T&& result) { return impl.push(std::move(result)); }

        template <typename T,
            fifo_options::concurrency consumer_concurrency,
            fifo_options::concurrency producer_concurrency,
            fifo_options::full_empty_failure_mode consumer_failure_mode,
            fifo_options::full_empty_failure_mode producer_failure_mode,
            std::size_t MAX_THREADS>
            bool fifo<T, consumer_concurrency, producer_concurrency, consumer_failure_mode, producer_failure_mode, MAX_THREADS>::pop(T& result) { return impl.pop(result); }
    }

    /**
    * \struct ConnectionRequest ConnectionRequest.h
    * Request to execute a ConnectionPlan on a Workflow. A ConnectionRequest
    * contains both the ConnectionPlan and its consequences (the new rendering
    * sequence and voice assignments after the plan is executed), which should be
    * computed in advance for the Renderer. To be valid, a ConnectionRequest should
    * verify the following two properties:
    * 1. None of its three vectors newRenderingSequence, newVoiceAssignments, and
    * oneIncrements should be empty;
    * 2. All of those three vectors should be of the same length.
    * .
    * To be consistent, both vectors newRenderingSequence and newVoiceAssignments
    * should be computed from the ConnectionPlan by the AudioWorkflow and
    * VoiceAssigner respectively.
    */
    struct ConnectionRequest
    {
        ConnectionPlan plan;
        std::vector<std::shared_ptr<Worker>> newRenderingSequence;
        std::vector<VoiceAssignment> newVoiceAssignments;

        /**
        * This vector provides pre-allocated memory for the Renderer's increment
        * computation. It should always be of the same size as newRenderingSequence
        * and newVoiceAssignments, and should always end with the number 1.
        */
        std::vector<uint32_t> oneIncrements;

        /**
        * Equals true if the request has been received and successfully executed by
        * the Renderer. Equals false if the request has not been received, if it is
        * not valid (its argument do not verify the two properties described in the
        * ConnectionRequest documentation) and was therefore ignored by the
        * Renderer, or if at least one of the ConnectionInstruction failed when the
        * ConnectionPlan was executed.
        */
        std::atomic<bool> hasBeenSuccessfullyProcessed;

        ConnectionRequest();
    };

    /**
    * \class Renderer Renderer.h
    * Agent responsible for actually performing the rendering of an audio block, by
    * calling the proper workers in its rendering sequence.
    */
    class Renderer
    {
    public:

        /**
        * Creates a Renderer with an empty rendering sequence. The Renderer will not
        * be ready to render anything upon creation: it will wait to be initialized
        * with a ConnectionRequest.
        * @param[in] workflow The Workflow to execute connection requests on
        */
        Renderer(Workflow& workflow);

        /**
        * Renders the given number of samples. This method constitutes the kernel of
        * ANGLECORE's real-time rendering pipeline. It will be repeatedly called by
        * the real-time thread, and is implemented to be the fastest possible.
        * @param[in] numSamplesToRender Number of samples to render. Every Worker in
        *   the rendering sequence that should be rendered will be instructed to
        *   render this number of samples.
        */
        void render(unsigned int numSamplesToRender);

        /**
        * Instructs the Renderer to turn a Voice on, and to recompute its increments
        * later, before rendering the next audio block. This method is really fast,
        * as it will only be called by the real-time thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Instructs the Renderer to turn a Voice off, and to recompute its
        * increments later, before rendering the next audio block. This method is
        * really fast, as it will only be called by the real-time thread.
        * @param[in] voiceNumber Number identifying the Voice to turn off
        */
        void turnVoiceOff(unsigned short voiceNumber);

        /**
        * Pushes the given request to the request queue. This method uses move
        * semantics, so it will take ownership of the pointer passed as argument. It
        * will never be called by the real-time thread, and will only be called by
        * the non real-time thread upon user request.
        * @param[in] request The ConnectionRequest to post to the Renderer. It will
        *   not be processed immediately, but before rendering the next audio block.
        */
        void postConnectionRequest(std::shared_ptr<ConnectionRequest>&& request);

    private:

        /**
        * Recomputes the Renderer's increments after a Voice has been turned on or
        * off, or after a ConnectionRequest has been processed. This method is fast,
        * as it will be called by the real-time thread.
        */
        void updateIncrements();

        /**
        * The Workflow the Renderer is working with. When processing connection
        * requests, this is the Workflow on which the request will be executed.
        */
        Workflow& m_workflow;

        /**
        * Boolean flag which signals when the rendering sequence and voice
        * assignments are valid, i.e. when they are both non-empty and of the same
        * size. It is initialized to false, and the Renderer expects an empty
        * ConnectionRequest to start up. Once set to true, this boolean will never
        * be set to false again, as only valid connection requests will be accepted.
        */
        bool m_isReadyToRender;

        /**
        * The current rendering sequence used by the renderer. It should always be
        * of the same length as m_voiceAssignments and m_increments.
        */
        std::vector<std::shared_ptr<Worker>> m_renderingSequence;

        /**
        * The voice assignments corresponding to each Worker in the current
        * rendering sequence. It should always be of the same length as
        * m_renderingSequence and m_increments.
        */
        std::vector<VoiceAssignment> m_voiceAssignments;

        /**
        * Position to start from in the rendering sequence. This may vary depending
        * on which Voice is on and off, and depending on the Workflow's connections.
        */
        uint32_t m_start;

        /**
        * Jumps to perform in the rendering sequence, in order to avoid Workers that
        * should not be called. It should always be of the same length as
        * m_renderingSequence and m_voiceAssignments.
        */
        std::vector<uint32_t> m_increments;

        /** Tracks the on/off status of every Voice */
        bool m_voiceIsOn[ANGLECORE_NUM_VOICES];

        /**
        * Flag that indicates whether to recompute the increments or not in the next
        * rendering session. This flag is useful for preventing the Renderer from
        * updating its increments twice, after a Voice has been turned on or off and
        * a new ConnectionRequest has been received.
        */
        bool m_shouldUpdateIncrements;

        /** Queue for receiving connection requests. */
        farbot::fifo<
            std::shared_ptr<ConnectionRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_connectionRequests;
    };
}