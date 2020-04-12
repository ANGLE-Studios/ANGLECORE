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
        * Returns true if the input bus is empty, meaning the Worker is actually a
        * generator, and false otherwise.
        */
        bool hasInputs() const;

        /**
        * Computes the values of every output Stream based on the input streams.
        * This method should be overidden in any sub-class to perform rendering.
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
        std::vector<std::shared_ptr<Worker>> buildRenderingSequence() const;

    private:
        std::shared_ptr<Exporter<float>> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
    };
}