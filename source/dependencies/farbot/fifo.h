#pragma once
#include <vector>
#include <atomic>
#include <cassert>
#include <thread>

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
    // Setting this a s consumer option causes the fifo to return a default constructed value on pop when the fifo is empty
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
    fifo (int capacity);

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
        };

        template <std::size_t MAX_THREADS>
        struct multi_position_info
        {
            std::atomic<uint32_t> num_threads = { 0 };
            std::array<thread_info, MAX_THREADS> tinfos = { {} };

            std::atomic<uint32_t>& get_tpos() noexcept
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

            uint32_t getpos(uint32_t min) const noexcept
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
            uint32_t getpos() const noexcept
            {
                return posinfo.getpos(reserve.load(std::memory_order_relaxed));
            }

            bool push_or_pop(std::vector<T>& s, T && arg, uint32_t max) noexcept
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
            uint32_t getpos() const noexcept
            {
                return reserve.load(std::memory_order_acquire);
            }

            bool push_or_pop(std::vector<T>& s, T && arg, uint32_t max) noexcept
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
            uint32_t getpos() const noexcept
            {
                return reserve.load(std::memory_order_acquire);
            }

            bool push_or_pop(std::vector<T>& s, T && arg, uint32_t) noexcept
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
            uint32_t getpos() const noexcept
            {
                return posinfo.getpos(reserve.load(std::memory_order_relaxed));
            }

            bool push_or_pop(std::vector<T>& s, T && arg, uint32_t) noexcept
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

                // We moved the initial lock-free assertion on thread::id here
                static_assert(std::atomic<std::thread::id>::is_lock_free());
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