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
#include <cstring>
#include <functional>
#include <mutex>
#include <chrono>
#include <utility>

namespace ANGLECORE
{
    /*
    =================================================
    General Configuration
    =================================================
    */

    #define ANGLECORE_PRECISION double          /**< Defines the precision of ANGLECORE's calculations as either single or double. It should equal float or double. Note that one can still use double precision within the workers of an AudioWorkflow if this is set to float. */
    typedef ANGLECORE_PRECISION floating_type;

    #define ANGLECORE_NUM_CHANNELS 2
    #define ANGLECORE_EXPORT_TYPE float          /**< Defines the precision of ANGLECORE's export samples as either single or double. It should equal float or double. Note that one can still use double precision in an AudioWorkflow if this is set to float. */
    typedef ANGLECORE_EXPORT_TYPE export_type;

    #define ANGLECORE_NUM_VOICES 32

    #define ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE 10

    /*
    =================================================
    Utility
    =================================================
    */

    /**
    * \struct StringView
    * Provides a simplified version of the C++17 string_view feature for compiling
    * with earlier versions of C++.
    * A StringView is simply a view over an existing C string, which provides a
    * simple interface for performing operations on that string.
    * This structure only contains a pointer to a C string, but defines special
    * comparison and hash operations which are applicable to that C string so that
    * it can be properly used as a identifier inside an STL key-value container.
    * Using a StringView instead of a std::string as a key type in such container
    * will prevent undesired memory allocation and copies.
    */
    struct StringView
    {
        /**
        * The constructor simply stores the provided pointer to C string internally.
        * It hereby provides an implicit conversion from const char* to StringView.
        */
        StringView(const char* str) :
            string_ptr(str)
        {}

        /**
        * Comparing two StringViews consists in comparing their internal C strings,
        * using strcmp.
        */
        bool operator==(const StringView& other) const
        {
            return strcmp(string_ptr, other.string_ptr) == 0;
        }

        /** This is the internal pointer to the C string being viewed */
        const char* string_ptr;
    };

    /**
    * \class Lockable Lockable.h
    * Utility object that can be locked for handling concurrency issues. The class
    * simply contains a mutex and a public method to access it by reference in order
    * to lock it.
    */
    class Lockable
    {
    public:

        /**
        * Returns the Lockable's internal mutex. Note that Lockable objects never
        * lock their mutex themselves: it is the responsability of the caller to
        * lock a Lockable appropriately when consulting or modifying its content in
        * a multi-threaded environment.
        */
        std::mutex& getLock()
        {
            return m_lock;
        }

    private:
        std::mutex m_lock;
    };
}



/*
=================================================
General Utility
=================================================
*/

namespace std {

    /**
    * In order to be used as a key within a standard key-value container, a StringView
    * must have a dedicated hash function. Otherwise, only the internal pointer will be
    * hashed, which will not produce the desired effect. Therefore, we have to
    * specialize the standard hash structure to our StringView type.
    */
    template<>
    struct hash<ANGLECORE::StringView>
    {
        size_t operator()(const ANGLECORE::StringView& view) const
        {
            /* This hash function comes from the boost library */
            size_t hashValue = 0;
            for (const char* p = view.string_ptr; p && *p; p++)
                hashValue ^= *p + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
            return hashValue;
        }
    };
}



namespace ANGLECORE
{
    /*
    =================================================
    Dependencies
    =================================================
    */

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
        const floating_type* getDataForReading() const;

        /** Provides a write access to the internal buffer. */
        floating_type* getDataForWriting();

    private:

        /** Internal buffer */
        floating_type* data;
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
        * pointers to nullptr. To be fully operational, the Workflow needs to
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
        const floating_type* getInputStream(unsigned short index) const;

        /**
        * Provides a write access to the Stream at \p index in the output bus.
        * @param[in] index Index of the stream within the output bus.
        */
        floating_type* getOutputStream(unsigned short index) const;

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
    AudioWorkflow
    =================================================
    */

    /**
    * \class Exporter Exporter.h
    * Worker that exports data from its input streams into an output buffer sent
    * to the host. An exporter applies a gain for calibrating its output level.
    */
    class Exporter :
        public Worker
    {
    public:

        /**
        * Creates a Worker with zero output.
        */
        Exporter();

        /**
        * Sets the new memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        * @param[in] numChannels Number of output channels.
        * @param[in] startSample Position to start from in the buffer.
        */
        void setOutputBuffer(export_type** buffer, unsigned short numChannels, uint32_t startSample);

        /**
        * Exports its input streams into the memory location that was given by the
        * setOutputBuffer() method. The Exporter will take care of any possible
        * mismatch between the number of channels rendered by ANGLECORE and the
        * number of channels requested by the host.
        * @param[in] numSamplesToWorkOn Number of samples to export.
        */
        void work(unsigned int numSamplesToWorkOn);

        /**
        * Increments the voice count. The Exporter will only export data if its
        * internal voice count is positive (> 0). Note that the voice count will
        * be clipped before exceeding the maximum number of voices available if this
        * method is called repeatedly.
        */
        void incrementVoiceCount();

        /**
        * Decrements the voice count. The Exporter will stop exporting data when its
        * internal voice count reaches the value of 0. Note that the voice count
        * will be clipped so it does not become negative if this method is called
        * repeatedly.
        */
        void decrementVoiceCount();

    private:
        export_type** m_outputBuffer;
        unsigned short m_numOutputChannels;
        uint32_t m_startSample;
        unsigned short m_numVoicesOn;
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

        /**
        * Instructs the Mixer to turn a Voice on, and to recompute its increments.
        * This method is really fast, as it will only be called by the real-time
        * thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Instructs the Mixer to turn a Voice off, and to recompute its increments.
        * This method is really fast, as it will only be called by the real-time
        * thread.
        * @param[in] voiceNumber Number identifying the Voice to turn on
        */
        void turnVoiceOff(unsigned short voiceNumber);

        /**
        * Turns the given Rack on and use it in the mix. This method must only be
        * called by the real-time thread.
        * @param[in] rackNumber Rack to turn on.
        */
        void turnRackOn(unsigned short rackNumber);

        /**
        * Turns the given Rack off and stop using it in the mix. This method must
        * only be called by the real-time thread.
        * @param[in] rackNumber Rack to turn off.
        */
        void turnRackOff(unsigned short rackNumber);

    private:

        /**
        * Recomputes the Mixer's voice increments after a Voice has been turned on
        * or off. This method is really fast, as it will only be called by the
        * real-time thread.
        */
        void updateVoiceIncrements();

        /**
        * Recomputes the Mixer's rack increments after a Voice has been turned on or
        * off. This method is really fast, as it will only be called by the
        * real-time thread.
        */
        void updateRackIncrements();

        const unsigned short m_totalNumInstruments;

        /**
        * Voice to start from when mixing voices. This may vary depending on which
        * Voice is on and off.
        */
        unsigned short m_voiceStart;

        /**
        * Jumps to perform between voices when mixing the audio output in order to
        * avoid those that are off.
        */
        uint32_t m_voiceIncrements[ANGLECORE_NUM_VOICES];

        /** Tracks the on/off status of every Voice */
        bool m_voiceIsOn[ANGLECORE_NUM_VOICES];

        /**
        * Rack to start from when mixing audio. This may vary depending on which
        * Rack is on and off.
        */
        unsigned short m_rackStart;

        /**
        * Jumps to perform between racks when mixing the audio output in order to
        * avoid those corresponding to instruments that are off.
        */
        uint32_t m_rackIncrements[ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE];

        /** Tracks the on/off status of every Rack */
        bool m_rackIsOn[ANGLECORE_NUM_VOICES];
    };

    /**
    * \struct Parameter Parameter.h
    * Represents a parameter that an AudioWorkflow can modify and use for rendering.
    */
    struct Parameter
    {
        /**
        * \enum SmoothingMethod Parameter.h
        * Method used to smooth out a parameter change. If set to ADDITIVE, then a
        * parameter change will result in an arithmetic sequence from an initial
        * value to a target value. If set to MULTIPLICATIVE, then the sequence will
        * be geometric and use a multiplicative increment instead.
        */
        enum SmoothingMethod
        {
            ADDITIVE = 0,   /**< Generates an arithmetic sequence */
            MULTIPLICATIVE, /**< Generates a geometric sequence */
            NUM_METHODS     /**< Counts the number of available methods */
        };

        StringView identifier;
        floating_type defaultValue;
        floating_type minimalValue;
        floating_type maximalValue;
        SmoothingMethod smoothingMethod;
        bool minimalSmoothingEnabled;
        uint32_t minimalSmoothingDurationInSamples;

        /** Creates a Parameter from the arguments provided */
        Parameter(const char* identifier, floating_type defaultValue, floating_type minimalValue, floating_type maximalValue, SmoothingMethod smoothingMethod, bool minimalSmoothingEnabled, uint32_t minimalSmoothingDurationInSamples) :
            identifier(identifier),
            defaultValue(defaultValue),
            minimalValue(minimalValue),
            maximalValue(maximalValue),
            smoothingMethod(smoothingMethod),
            minimalSmoothingEnabled(minimalSmoothingEnabled),
            minimalSmoothingDurationInSamples(minimalSmoothingDurationInSamples)
        {}
    };

    /**
    * \struct ParameterChangeRequest ParameterChangeRequest.h
    * When the end-user asks to change the value of a Parameter within an Instrument
    * (volume, etc.) or the entire AudioWorkflow (sample rate, etc.), an instance of
    * this structure is created to store all necessary information about that
    * request.
    */
    struct ParameterChangeRequest
    {
        floating_type newValue;
        uint32_t durationInSamples;
    };

    /**
    * \class ParameterGenerator ParameterGenerator.h
    * Worker that generates the values of a Parameter in a Stream, according to the
    * end-user requests. A ParameterGenerator also takes care of smoothing out every
    * sudden change to avoid audio glitches.
    */
    class ParameterGenerator :
        public Worker
    {
    public:

        /**
        * Creates a ParameterGenerator from the given Parameter.
        * @param[in] parameter Parameter containing all the necessary information
        *   for the ParameterGenerator to generate its value.
        */
        ParameterGenerator(const Parameter& parameter);

        /**
        * Pushes the given request into the request queue. This method uses move
        * semantics, so it will take ownership of the pointer passed as argument. It
        * will never be called by the real-time thread, and will only be called by
        * the non real-time thread upon user request.
        * @param[in] request The ParameterChangeRequest to post to the
        *   ParameterGenerator. It will not be processed immediately, but before
        *   rendering the next audio block.
        */
        void postParameterChangeRequest(std::shared_ptr<ParameterChangeRequest>&& request);

        /**
        * Generates the successive values of the associated Parameter for the next
        * rendering session.
        * @param[in] numSamplesToWorkOn Number of samples to generate.
        */
        void work(unsigned int numSamplesToWorkOn);

        /**
        * Instructs the generator to change the parameter's value instantaneously,
        * without any transient phase. This method must never be called by the non
        * real-time thread, and should only be called by the real-time thread.
        * Requests emitted by the non-real thread should use the
        * postParameterChangeRequest() method instead.
        * @param[in] newValue The new value of the Parameter.
        */
        void setParameterValue(floating_type newValue);

    private:

        /**
        * \struct TransientTracker ParameterGenerator.h
        * A TransientTracker tracks a Parameter's position while in a transient
        * state. This structure can store how close the Parameter is to its target
        * value (in terms of remaining samples), as well as the increment to use for
        * its update.
        */
        struct TransientTracker
        {
            floating_type targetValue;
            uint32_t transientDurationInSamples;
            uint32_t position;
            floating_type increment;
        };

        /**
        * \enum State
        * Represents the state of a ParameterGenerator.
        */
        enum State
        {
            STEADY = 0,             /**< The Parameter has a constant value, and the output stream is entierly filled with that value */
            TRANSIENT,              /**< The Parameter is currently changing its value */
            TRANSIENT_TO_STEADY,    /**< The Parameter has reached a new value but the output stream still needs to be filled up accordingly */
            NUM_STATES              /**< Counts the number of possible states */
        };

        const Parameter& m_parameter;
        floating_type m_currentValue;
        State m_currentState;

        /** Queue for receiving Parameter change requests. */
        farbot::fifo<
            std::shared_ptr<const ParameterChangeRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_requestQueue;

        TransientTracker m_transientTracker;
    };

    /**
    * \struct GlobalContext GlobalContext.h
    * Set of streams which provide useful information, such as the sample rate the
    * audio should be rendered into, in a centralized spot, shared by the rest of
    * the AudioWorkflow.
    */
    struct GlobalContext
    {
        std::shared_ptr<Stream> sampleRateStream;
        std::shared_ptr<Stream> sampleRateReciprocalStream;

        /**
        * Creates a GlobalContext, hereby initializing every pointer to a Stream or
        * a Worker to a non null pointer. Note that this method does not do any
        * connection between streams and workers, as this is the responsability of
        * the AudioWorkflow to which the GlobalContext belong.
        */
        GlobalContext();

        void setSampleRate(floating_type sampleRate);

    private:
        floating_type m_currentSampleRate;
    };

    /**
    * \class Instrument Instrument.h
    * Worker that generates audio within an AudioWorkflow.
    */
    class Instrument :
        public Worker
    {
    public:

        enum ContextParameter
        {
            SAMPLE_RATE = 0,
            SAMPLE_RATE_RECIPROCAL,
            FREQUENCY,
            FREQUENCY_OVER_SAMPLE_RATE,
            VELOCITY,
            NUM_CONTEXT_PARAMETERS
        };

        /**
        * \struct ContextConfiguration Instrument.h
        * The ContextConfiguration describes whether or not an Instrument should be
        * connected to a particular context within an AudioWorkflow. In defines how
        * an Instrument interacts with its surrounding, and, for example, if it
        * should capture and use the sample rate during its rendering.
        */
        struct ContextConfiguration
        {
            bool receiveSampleRate;
            bool receiveSampleRateReciprocal;
            bool receiveFrequency;
            bool receiveFrequencyOverSampleRate;
            bool receiveVelocity;

            /** Creates a ContextConfiguration from the given parameters */
            ContextConfiguration(bool receiveSampleRate, bool receiveSampleRateReciprocal, bool receiveFrequency, bool receiveFrequencyOverSampleRate, bool receiveVelocity);
        };

        enum State
        {
            ON = 0,
            ON_ASKED_TO_STOP,
            ON_TO_OFF,
            OFF
        };

        /**
        * Creates an Instrument from a list of parameters, split between the context
        * parameters, which are common to other instruments (such as the sample rate
        * and the velocity), and the specific parameters that are unique for the
        * Instrument. Note that the two vectors passed in as arguments will be
        * copied inside the Instrument.
        * @param[in] contextParameters Vector containing all the context parameters
        *   the Instrument needs in order to work properly.
        * @param[in] parameters Vector containing all the specific parameters the
        *   Instrument needs to work properly, in addition to the context
        *   parameters.
        */
        Instrument(const std::vector<ContextParameter>& contextParameters, const std::vector<Parameter>& parameters);

        /**
        * Returns the input port number where the given \p contextParameter should
        * be plugged in.
        * @param[in] contextParameter Context parameter to retrieve the input port
        *   number from.
        */
        unsigned short getInputPortNumber(ContextParameter contextParameter) const;

        /**
        * Returns the input port number where the given Parameter, identified using
        * its StringView identifier, should be plugged in.
        * @param[in] parameterID String, submitted either as a StringView or a const
        *   char*, which uniquely identifies the given Parameter.
        */
        unsigned short getInputPortNumber(const StringView& parameterID) const;

        /**
        * Returns the Instrument's ContextConfiguration, which specifies how the
        * Instrument should be connected to the audio workflows' GlobalContext and
        * VoiceContext in order to retrieve shared information, such as the sample
        * rate or the velocity of each note.
        */
        const ContextConfiguration& getContextConfiguration() const;

        /**
        * Returns the Instrument's internal set of parameters, therefore excluding
        * the context parameters which are all exogenous.
        */
        const std::vector<Parameter>& getParameters() const;

        /**
        * Turns the Instrument on, so it can generate sound. An Instrument that is
        * off will immediately return when called in a rendering session.
        */
        void turnOn();

        /**
        * Turns the Instrument off, so it stops generating sound. An Instrument that
        * is off will immediately return when called in a rendering session.
        */
        void turnOff();

        /**
        * Instructs the Instrument to evolve to a ready-to-stop state, and prepare
        * to render \p stopDurationInSamples before being muted. This method will
        * only be called by the real-time thread.
        * @param[in] stopDurationInSamples Number of samples that the Instrument
        *   must generate before being muted.
        */
        void prepareToStop(uint32_t stopDurationInSamples);

        /**
        * Generates the given number of samples according to the Instrument's
        * internal state. This method overrides the pure virtual work() method from
        * the Worker class, and it will only be called by the real-time thread.
        * @param[in] numSamplesToWorkOn Number of samples to generate.
        */
        void work(unsigned int numSamplesToWorkOn);

        /**
        * Instructs the Instrument to reset itself, in order to be ready to play a
        * new note. This method will always be called when the end-user requests to
        * play a note, right before rendering. Subclasses of the Instrument class
        * must override this method and define a convenient behavior during this
        * reinitialization. For instance, instruments can use this opportunity to
        * read the sample rate and update internal parameters if it has changed.
        */
        virtual void reset() = 0;

        /**
        * Instructs the Instrument to start playing. This method will always be
        * called right after the reset() method. Subclasses of the Instrument class
        * must override this method and define a convenient behavior during this
        * initialization step. For instance, instruments can use this opportunity to
        * change the state of an internal Envelope.
        */
        virtual void startPlaying() = 0;

        /**
        * Generates the given number of samples and send them into the Instrument's
        * output streams. This method is the core method of the Instrument class, as
        * it defines what sound the Instrument will generate. Subclasses of the
        * Instrument class must override this method and implement the desired audio
        * synthesis and audio processing algorithms inside.
        * @param[in] numSamplesToPlay Number of audio samples to generate.
        */
        virtual void play(unsigned int numSamplesToPlay) = 0;

        /**
        * Calculates and returns the number of samples required by the Instrument to
        * generate its audio tail. This method will always be called right before
        * the Instrument is instructed to stop. Subclasses of the Instrument class
        * must override this method and return the minimum of samples the Instrument
        * needs to fade out. ANGLECORE will guarantee that the Instrument renders
        * this exact number of samples before being muted and forced to generate
        * silence, and at least this number of samples before being turned off. Note
        * that this method will only be called by the real-time thread.
        */
        virtual uint32_t computeStopDurationInSamples() const = 0;

        /**
        * Instructs the Instrument to stop playing, and returns the number of
        * samples required by the Instrument to generate its audio tail. This method
        * will always be called after the reset() and startPlaying() methods.
        * Subclasses of the Instrument class must override this method and ensure
        * that, after the number of samples returned by this method, the object will
        * only generate silence. This method can trigger, for instance, a gentle
        * fade out through the release stage of an internal Envelope.
        */
        virtual void stopPlaying() = 0;

    protected:

        /**
        * \struct StopTracker Instrument.h
        * A StopTracker tracks an Instrument's position while it stops playing and
        * renders its audio tail. This structure can store how close the Instrument
        * is to the end of its tail in terms of remaining samples.
        */
        struct StopTracker
        {
            uint32_t stopDurationInSamples;
            uint32_t position;
        };

    private:
        const std::vector<ContextParameter> m_contextParameters;
        const std::vector<Parameter> m_parameters;
        const ContextConfiguration m_configuration;
        std::unordered_map<ContextParameter, unsigned short> m_contextParameterInputPortNumbers;
        std::unordered_map<StringView, unsigned short> m_parameterInputPortNumbers;
        State m_state;
        StopTracker m_stopTracker;
    };

    /**
    * \struct VoiceContext VoiceContext.h
    * Set of streams and workers designed to provide useful information, such as the
    * frequency the instruments of Each voice should play, in a centralized spot.
    * Note that Workers and streams are not connected upon creation: it is the
    * responsability of the AudioWorkflow they will belong to to make the
    * appropriate connections.
    */
    struct VoiceContext
    {
        /**
        * \class RatioCalculator GlobalContext.h
        * Worker that computes the sample-wise division of the Voice's frequency by
        * the global sample rate. The RatioCalculator takes advantage of the fact
        * that the reciprocal of the sample rate is already computed in the
        * AudioWorkflow's GlobalContext, and therefore performs a multiplication
        * rather than a division to compute the desired ratio, which leads to a
        * shorter computation time.
        */
        class RatioCalculator :
            public Worker
        {
        public:

            enum Input
            {
                FREQUENCY = 0,
                SAMPLE_RATE_RECIPROCAL,
                NUM_INPUTS  /**< This will equal two, as a RatioCalculator must have
                            * exactly two inputs */
            };

            /**
            * Creates a RatioCalculator, which is a Worker with two inputs and one
            * output.
            */
            RatioCalculator();

            /**
            * Computes the sample-wise division of the two input streams. Note that
            * the computation is done over the entire Stream, regardless of \p
            * numSamplesToWorkOn, for later use by other items from the Voice.
            * @param[in] numSamplesToWorkOn This parameter will be ignored.
            */
            void work(unsigned int numSamplesToWorkOn);
        };

        Parameter frequency;
        std::shared_ptr<ParameterGenerator> frequencyGenerator;
        std::shared_ptr<Stream> frequencyStream;
        std::shared_ptr<RatioCalculator> ratioCalculator;
        std::shared_ptr<Stream> frequencyOverSampleRateStream;

        Parameter velocity;
        std::shared_ptr<Stream> velocityStream;

        /**
        * Creates a VoiceContext, hereby initializing every pointer to a Stream or a
        * Worker to a non null pointer. Note that this method does not do any
        * connection between streams and workers, as this is the responsability of
        * the AudioWorkflow to which the VoiceContext belong.
        */
        VoiceContext();
    };

    /**
    * \struct Voice Voice.h
    * Set of workers and streams, which together form an audio engine that can be
    * used by the Master, the AudioWorkflow, and the Renderer to generate some audio
    * according to what the user wants to play.
    */
    struct Voice
    {
        /**
        * \struct Rack Voice.h
        * Set of workers and streams which are specific to an Instrument, within a
        * particular Voice.
        */
        struct Rack
        {
            bool isEmpty;
            std::shared_ptr<Instrument> instrument;
        };

        bool isFree;
        bool isOn;
        unsigned char currentNoteNumber;
        unsigned char currentNoteVelocity;

        /**
        * Each Voice has its own context, called a VoiceContext. A VoiceContext is a
        * set of streams and workers who sends information about the frequency and
        * velocity the Voice should use to play some sound.
        */
        VoiceContext voiceContext;
        Rack racks[ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE];

        /**
        * Creates a Voice only comprised of empty racks.
        */
        Voice();
    };

    /**
    * \class AudioWorkflow AudioWorkflow.h
    * Workflow internally structured to generate an audio output. It consists of a
    * succession of workers and streams, and it should not contain any feedback.
    */
    class AudioWorkflow :
        public Workflow,
        public VoiceAssigner,
        public Lockable
    {
    public:

        /** Builds the base structure of the AudioWorkflow (Exporter, Mixer...) */
        AudioWorkflow();

        /**
        * Sets the sample rate of the AudioWorkflow.
        * @param[in] sampleRate The value of the sample rate, in Hz.
        */
        void setSampleRate(floating_type sampleRate);

        /**
        * Builds and returns the Workflow's rendering sequence, starting from its
        * Exporter. This method allocates memory, so it should never be called by
        * the real-time thread. Note that the method relies on the move semantics to
        * optimize its vector return.
        * @param[in] connectionPlan The ConnectionPlan that will be executed next,
        *   and that should therefore be taken into account in the computation
        */
        std::vector<std::shared_ptr<Worker>> buildRenderingSequence(const ConnectionPlan& connectionPlan) const;

        /**
        * Set the Exporter's memory location to write into when exporting.
        * @param[in] buffer The new memory location.
        * @param[in] numChannels Number of output channels.
        * @param[in] startSample Position to start from in the buffer.
        */
        void setExporterOutput(float** buffer, unsigned short numChannels, uint32_t startSample);

        /**
        * Tries to find a Rack that is empty in all voices to insert an instrument
        * inside. Returns the valid rack number of an empty rack if has found one,
        * and an out-of-range number otherwise.
        */
        unsigned short findEmptyRack() const;

        /**
        * Adds an Instrument to the given Voice, at the given \p rackNumber, then
        * build the Instrument's Environment and plan its bridging to the real-time
        * rendering pipeline by completing the given \p planToComplete. Note that
        * every parameter is expected to be in-range and valid, and that no safety
        * check will be performed by this method.
        * @param[in] voiceNumber Voice to place the Instrument in.
        * @param[in] rackNumber Rack to insert the Instrument into within the given
        *   voice.
        * @param[in] instrument The Instrument to insert. It should not be a null
        *   pointer.
        * @param[out] planToComplete The ConnectionPlan to complete with bridging
        *   instructions.
        */
        void addInstrumentAndPlanBridging(unsigned short voiceNumber, unsigned short rackNumber, const std::shared_ptr<Instrument>& instrument, ConnectionPlan& planToComplete);

        /**
        * Tries to find a Voice that is free, i.e. not currently playing anything,
        * in order to make it play some sound. Returns the valid voice number of an
        * empty voice if has found one, and an out-of-range number otherwise.
        */
        unsigned short findFreeVoice() const;

        /**
        * Turns the given Voice on. This method must only be called by the real-time
        * thread.
        * @param[in] voiceNumber Voice to turn on.
        */
        void turnVoiceOn(unsigned short voiceNumber);

        /**
        * Turns the given Voice off and sets it free. This method must only be
        * called by the real-time thread.
        * @param[in] voiceNumber Voice to turn off and set free.
        */
        void turnVoiceOffAndSetItFree(unsigned short voiceNumber);

        /**
        * Instructs the AudioWorkflow to take the given Voice, so it is not marked
        * as free anymore, and to make it play the given note. This method takes the
        * note, sends the note to play to the Voice, then resets every Instrument
        * located in the latter, and starts them all. Note that every parameter is
        * expected to be in-range, and that no safety check will be performed by
        * this method.
        * @param[in] voiceNumber Voice that should play the note.
        * @param[in] noteNumber Note number to send to the Voice.
        * @param[in] velocity Velocity to play the note with.
        */
        void takeVoiceAndPlayNote(unsigned short voiceNumber, unsigned char noteNumber, unsigned char noteVelocity);

        /**
        * Returns true if the given Voice is on and playing the given \p noteNumber,
        * and false otherwise. Note that \p voiceNumber is expected to be in-range,
        * and that, consequently, no safety check will be performed by this method.
        * @param[in] voiceNumber Voice to test.
        * @param[in] noteNumber Note to look for in the given Voice.
        */
        bool playsNoteNumber(unsigned short voiceNumber, unsigned char noteNumber) const;

        /**
        * Requests the Mixer to turn the given Rack on and use it in the mix. This
        * method must only be called by the real-time thread.
        * @param[in] rackNumber Rack to turn on.
        */
        void turnRackOn(unsigned short rackNumber);

        /**
        * Requests the Mixer to turn the given Rack off and stop using it in the
        * mix. This method must only be called by the real-time thread.
        * @param[in] rackNumber Rack to turn off.
        */
        void turnRackOff(unsigned short rackNumber);

        /**
        * Requests all the instruments contained in the given Voice to stop playing,
        * and returns the Voice's audio tail duration in samples, which is the
        * number of samples before the Voice begins to emit complete silence.
        * This method must only be called by the real-time thread.
        * @param[in] voiceNumber Voice that should stop playing.
        */
        uint32_t stopVoice(unsigned short voiceNumber);

    protected:

        /**
        * Retrieve the ID of the Mixer's input Stream at the provided location. Note
        * that every parameter is expected to be in-range, and that no safety check
        * will be performed by this method.
        * @param[in] voiceNumber Voice whose workers ultimately fill in the Stream.
        * @param[in] rackNumber Rack at the end of which the Stream is located.
        * @param[in] channel Audio channel that the Stream corresponds to.
        */
        uint32_t getMixerInputStreamID(unsigned short voiceNumber, unsigned short rackNumber, unsigned short channel) const;

        /**
        * Retrieve the ID of the Stream containing the current sample rate in the
        * AudioWorkflow's global context.
        */
        uint32_t getSampleRateStreamID() const;

        /**
        * Retrieve the ID of the Stream containing the reciprocal of the current
        * sample rate in the AudioWorkflow's global context.
        */
        uint32_t getSampleRateReciprocalStreamID() const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * frequency. Note that voiceNumber is expected to be in-range, and that no
        * safety check will be performed by this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getFrequencyStreamID(unsigned short voiceNumber) const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * frequency divided by the global sample rate. Note that voiceNumber is
        * expected to be in-range, and that no safety check will be performed by
        * this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getFrequencyOverSampleRateStreamID(unsigned short voiceNumber) const;

        /**
        * Retrieve the ID of the Stream containing the given Voice's current
        * velocity. Note that voiceNumber is expected to be in-range, and that no
        * safety check will be performed by this method.
        * @param[in] voiceNumber Voice to retrieve the information from.
        */
        uint32_t getVelocityStreamID(unsigned short voiceNumber) const;

    private:
        std::shared_ptr<Exporter> m_exporter;
        std::shared_ptr<Mixer> m_mixer;
        Voice m_voices[ANGLECORE_NUM_VOICES];
        GlobalContext m_globalContext;
    };



    /*
    =================================================
    Renderer
    =================================================
    */

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
    * should be computed from the same ConnectionPlan and by the same AudioWorkflow.
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
        */
        Renderer();

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
        * Acquires the new rendering sequence and voice assignments from the given
        * ConnectionRequest, as well as the increment vector. The request passed as
        * argument must be valid, as this method will perform no validity check and
        * take the ConnectionRequest's results from granted. This method uses move
        * semantics, so it will take ownership of the vectors contained in the
        * ConnectionRequest passed as argument. This method must only be called by
        * the real-time thread.
        * @param[in] request The ConnectionRequest to take the results from. It
        *   should be a valid ConnectionRequest, i.e. it should respect the two
        *   properties defined in the structure definition (the rendering sequence,
        *   voice assignments and one increment vectors must all be of the same size
        *   and must all be non empty).
        */
        void processConnectionRequest(ConnectionRequest& request);

    private:

        /**
        * Recomputes the Renderer's increments after a Voice has been turned on or
        * off, or after a ConnectionRequest has been processed. This method is fast,
        * as it will be called by the real-time thread.
        */
        void updateIncrements();

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
    };



    /*
    =================================================
    Master
    =================================================
    */

    /**
    * \struct MIDIMessage MIDIBuffer.h
    * Represents a MIDI message supported by the Master object.
    */
    struct MIDIMessage
    {
        /** Types of MIDI messages supported by the Master object */
        enum Type
        {
            NONE = 0,       /**< MIDI Messages of this type will be ignored */
            NOTE_ON,
            NOTE_OFF,
            ALL_NOTES_OFF,
            ALL_SOUND_OFF,
            NUM_TYPES
        };

        /** Type of MIDI message */
        Type type;

        /**
        * Timestamp of the message as a sample position within the current audio
        * buffer
        */
        uint32_t timestamp;

        unsigned char noteNumber;
        unsigned char noteVelocity;

        /**
        * Creates a MIDI message of type NONE, and initializes every attribute to
        * their default value.
        */
        MIDIMessage();
    };

    /**
    * \class MIDIBuffer MIDIBuffer.h
    * Buffer of MIDI messages.
    */
    class MIDIBuffer
    {
    public:

        /** Creates a MIDI buffer of fixed size */
        MIDIBuffer();

        /**
        * Returns the number of MIDI messages contained in the buffer. This will be
        * different from the buffer's capacity.
        */
        uint32_t getNumMIDIMessages() const;

        /**
        * Adds a new empty MIDIMessage at the end of the buffer, and returns a
        * reference to it.
        */
        MIDIMessage& pushBackNewMIDIMessage();

        /** Removes every MIDIMessage from the buffer. */
        void clear();

        /**
        * Provides a read an write access to the MIDI messages.
        * @param[in] index Position of the MIDIMessage to retrieve from the buffer.
        *   It should be in-range, as it will not be checked at runtime.
        */
        MIDIMessage& operator[](uint32_t index);

    private:
        uint32_t m_capacity;
        std::vector<MIDIMessage> m_messages;
        uint32_t m_numMessages;
    };

    /**
    * \struct InstrumentRequest InstrumentRequest.h
    * When the end-user adds a new Instrument or removes one from an AudioWorkflow,
    * an instance of this structure is created to request the Mixer of the
    * AudioWorkflow to either activate or deactivate the corresponding rack for its
    * mixing process, and to create or remove the necessary connections within the
    * AudioWorkflow.
    */
    struct InstrumentRequest
    {
        enum Type
        {
            ADD = 0,
            REMOVE,
            NUM_TYPES
        };

        Type type;
        unsigned short rackNumber;

        /**
        * ConnectionRequest that matches the InstrumentRequest, and which instructs
        * to add or remove connections from the AudioWorkflow the Instrument will be
        * inserted into or removed from.
        */
        ConnectionRequest connectionRequest;

        /**
        * Creates an InstrumentRequest.
        */
        InstrumentRequest(Type type, unsigned short rackNumber);
    };

    /**
    * \class Master Master.h
    * Agent that orchestrates the rendering and the interaction with the end-user
    * requests. It should be the only entry point from outside when developping an
    * ANGLECORE-based application.
    */
    class Master
    {
    public:
        Master();

        /**
        * Sets the sample rate of the Master's AudioWorkflow.
        * @param[in] sampleRate The value of the sample rate, in Hz.
        */
        void setSampleRate(floating_type sampleRate);

        /**
        * Clears the Master's internal MIDIBuffer to prepare for rendering the next
        * audio block.
        */
        void clearMIDIBufferForNextAudioBlock();

        /**
        * Adds a new MIDIMessage at the end of the Master's internal MIDIBuffer, and
        * returns a reference to it. This method should only be called by the
        * real-time thread, right before calling the renderNextAudioBlock() method.
        */
        MIDIMessage& pushBackNewMIDIMessage();

        /**
        * Renders the next audio block, using the internal MIDIBuffer as a source
        * of MIDI messages, and the provided parameters for computing and exporting
        * the output result.
        * @param[in] audioBlockToGenerate The memory location that will be used to
        *   output the audio data generated by the Renderer. This should correspond
        *   to an audio buffer of at least \p numChannels audio channels and \p
        *   numSamples audio samples.
        * @param[in] numChannels The number of channels available at the memory
        *   location \p audioBlockToGenerate. Note that ANGLECORE's engine may
        *   generate more channels than this number, and eventually merge them all
        *   to match that number.
        * @param[in] numSamples The number of samples to generate and write into
        *   \p audioBlockToGenerate.
        */
        void renderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples);

        template<class InstrumentType>
        bool addInstrument();

    protected:

        /**
        * \struct StopTracker Master.h
        * A StopTracker tracks a Voice's position while it stops playing and renders
        * its audio tail. This structure can store how close the Voice is to the end
        * of its tail in terms of remaining samples.
        */
        struct StopTracker
        {
            uint32_t stopDurationInSamples;
            uint32_t position;
        };

        /**
        * Renders the next audio block by splitting it into smaller audio chunks
        * that are all smaller than ANGLECORE's Stream fixed buffer size. This
        * method is the one that guarantees the Renderer only receives a valid
        * number of samples to render.
        * @param[in] audioBlockToGenerate The memory location that will be used to
        *   output the audio data generated by the Renderer. This should correspond
        *   to an audio buffer of at least \p numChannels audio channels and \p
        *   numSamples audio samples.
        * @param[in] numChannels The number of channels available at the memory
        *   location \p audioBlockToGenerate. Note that ANGLECORE's engine may
        *   generate more channels than this number, and eventually merge them all
        *   to match that number.
        * @param[in] numSamples The number of samples to generate and write into
        *   \p audioBlockToGenerate.
        * @param[in] startSample The position within the \p audioBlockToGenerate to
        *   start writing from.
        */
        void splitAndRenderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples, uint32_t startSample);

        /** Processes the requests received in its internal queues. */
        void processRequests();

        /** Processes the given MIDIMessage. */
        void processMIDIMessage(const MIDIMessage& message);

        /**
        * Updates the Master's internal stop trackers corresponding to each Voice,
        * after \p numSamples were rendered. This method detects if it is necessary
        * to turn some voices off, and do so if needed.
        * @param[in] numSamples Number of samples that were just rendered, and that
        *   should be used to increment the stop trackers.
        */
        void updateStopTrackersAfterRendering(uint32_t numSamples);

    private:
        AudioWorkflow m_audioWorkflow;
        Renderer m_renderer;
        MIDIBuffer m_midiBuffer;

        /** Queue for pushing and receiving instrument requests. */
        farbot::fifo<
            std::shared_ptr<InstrumentRequest>,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::concurrency::single,
            farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
            farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default
        > m_instrumentRequests;

        bool m_voiceIsStopping[ANGLECORE_NUM_VOICES];
        StopTracker m_stopTrackers[ANGLECORE_NUM_VOICES];
    };

    template<class InstrumentType>
    bool Master::addInstrument()
    {
        std::lock_guard<std::mutex> scopedLock(m_audioWorkflow.getLock());

        /* Can we insert a new instrument? We need to find an empty spot first */
        unsigned short emptyRackNumber = m_audioWorkflow.findEmptyRack();
        if (emptyRackNumber >= ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)

            /*
            * There is no empty spot, so we stop here and return false. We cannot
            * insert a new instrument to the workflow.
            */
            return false;

        /*
        * Otherwise, if we can insert a new instrument, then we need to prepare the
        * instrument's environment, as well as a new InstrumentRequest for bridging
        * the instrument with the real-time rendering pipeline.
        */

        std::shared_ptr<InstrumentRequest> request = std::make_shared<InstrumentRequest>(InstrumentRequest::Type::ADD, emptyRackNumber);
        ConnectionRequest& connectionRequest = request->connectionRequest;
        ConnectionPlan& plan = connectionRequest.plan;

        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            /*
            * We create an Instrument of the given type, and then cast it to an
            * Instrument, to ensure type validity.
            */
            std::shared_ptr<Instrument> instrument = std::make_shared<InstrumentType>();

            /*
            * Then, we insert the Instrument into the Workflow and plan its bridging
            * to the real-time rendering pipeline.
            */
            m_audioWorkflow.addInstrumentAndPlanBridging(v, emptyRackNumber, instrument, plan);
        }

        /*
        * Once here, we have a ConnectionPlan ready to be used in our
        * ConnectionRequest. We now need to precompute the consequences of executing
        * than plan and connecting all of the instruments to the AudioWorkflow's
        * real-time rendering pipeline.
        */

        /*
        * We first calculate the rendering sequence that will take effect right
        * after the plan is executed.
        */
        std::vector<std::shared_ptr<Worker>> newRenderingSequence = m_audioWorkflow.buildRenderingSequence(plan);

        /*
        * And from that sequence, we can precompute and assign the rest of the
        * request properties:
        */
        connectionRequest.newRenderingSequence = newRenderingSequence;
        connectionRequest.newVoiceAssignments = m_audioWorkflow.getVoiceAssignments(newRenderingSequence);
        connectionRequest.oneIncrements.resize(newRenderingSequence.size(), 1);

        /*
        * Finally, we need to send the InstrumentRequest to the Renderer. To avoid
        * any memory deallocation by the real-time thread after it is done with the
        * request, we do not pass the request straight to the Renderer. Instead, we
        * create a copy and send that copy to the latter. Therefore, when the
        * Renderer is done with the request, it will only delete a copy of a shared
        * pointer and decrement its reference count by one, signaling to the Master
        * the request has been processed (and either succeeded or failed), and that
        * it can be safely destroyed by the non real-time thread that created it.
        */

        /* We copy the InstrumentRequest... */
        std::shared_ptr<InstrumentRequest> requestCopy = request;

        /* ... And post the copy:*/
        m_instrumentRequests.push(std::move(requestCopy));

        /*
        * From now on, the InstrumentRequest is in the hands of the real-time
        * thread. We cannot access any member of 'request'. We will still use the
        * reference count of the shared pointer as an indicator of when the
        * real-time thread is done with the request (although it is not guaranteed
        * to be safe by the standard). As long as that number is greater than 1
        * (and, normally, equal to 2), the real-time thread is still in possession
        * of the copy, and possibly processing it. So the non real-time thread
        * should wait. When this number reaches 1, it means the real-time thread is
        * done with the request and the non real-time thread can safely delete it.
        */

        /*
        * To avoid infinite loops and therefore deadlocks while waiting for the
        * real-time thread, we introduce a timeout, using a number of attempts.
        */
        const unsigned short timeoutAttempts = 4;
        unsigned short attempt = 0;

        /*
        * We then wait for the real-time thread to finish, or for the timeout to
        * arise.
        */
        while (request.use_count() > 1 && attempt++ < timeoutAttempts)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

        /*
        * Once here, we either reached the timeout, or the copy of the
        * InstrumentRequest has been destroyed, and only the original remains, and
        * can be safely deleted. In any case, the original request will be deleted,
        * so if the timeout is the reason for leaving the loop, then the copy will
        * outlive the original in the real-time thread, which will trigger memory
        * deallocation upon destruction, and possibly provoke an audio glitch (this
        * should actually be very rare). If we left the loop because the copy was
        * destroyed (which is the common case), then the original will be safely
        * deleted here by the non real-time thread.
        */

        /*
        * We access the 'hasBeenSuccessfullyProcessed' variable that may have been
        * edited by the real-time thread, in order to determine whether or not the
        * request was been successfully processed.
        */
        return request->connectionRequest.hasBeenSuccessfullyProcessed.load();
    }
}