#pragma once

#include <atomic>
#include <memory>
#include <mutex>

namespace farbot
{
//==============================================================================
namespace detail { template <typename, bool> class NRMScopedAccessImpl; }

//==============================================================================
/** Useful class to synchronise access to an object from multiple threads with the additional feature that one
 * designated thread will never wait to get access to the object (i.e. the realtime thread). */
template <typename T> class NonRealtimeMutatable
{
public:
    /** Creates a default constructed T */
    NonRealtimeMutatable();

    /** Creates a copy of T */
    explicit NonRealtimeMutatable (const T & obj);

    /** Moves T into this realtime wrapper */
    explicit NonRealtimeMutatable (T && obj);

    ~NonRealtimeMutatable();

    /** Create T by calling T's constructor which takes args */
    template <typename... Args>
    static NonRealtimeMutatable create(Args && ... args);

    //==============================================================================
    /** Returns a reference to T. Use this method on the real-time thread.
     *  It must be matched by realtimeRelease when you are finished using the 
     *  object. Alternatively, use the ScopedAccess helper class below.
     * 
     *  Only a single real-time thread can acquire this object at once!
     * 
     *  This method is wait- and lock-free.
     */
    const T& realtimeAcquire();

    /** Releases the lock on T previously acquired by realtimeAcquire.
     * 
     *  This method is wait- and lock-free.
     */
    void realtimeRelease();

    //==============================================================================
    /** Returns a reference to T. Use this method on the non real-time thread.
     *  It must be matched by nonRealtimeRelease when you are finished using
     *  the object. Alternatively, use the ScopedAccess helper class below.
     * 
     *  Multiple non-realtime threads can acquire an object at the same time.
     * 
     *  This method uses a lock should not be used on a realtime thread.
     */ 
    T& nonRealtimeAcquire();

    /** Releases the lock on T previously acquired by nonRealtimeAcquire.
     * 
     *  This method uses both a lock and a spin loop and should not be used
     *  on a realtime thread.
     */
    void nonRealtimeRelease();

    /** Replace the underlying value with a new instance of T by forwarding
     *  the method's arguments to T's constructor
     */
    template <typename... Args>
    void nonRealtimeReplace(Args && ... args);

    //==============================================================================
    /** Instead of calling acquire and release manually, you can also use this RAII
     *  version which calls acquire automatically on construction and release when
     *  destructed.
     */
    template <bool isRealtimeThread>
    class ScopedAccess    : public detail::NRMScopedAccessImpl<T, isRealtimeThread>
    {
    public:
        explicit ScopedAccess (NonRealtimeMutatable& parent);

       #if DOXYGEN
        // Various ways to get access to the underlying object.
        // Non-const method are only supported on the non-realtime thread
        T* get() noexcept;
        const T* get() const noexcept;
        T &operator *() noexcept;
        const T &operator *() const noexcept;
        T* operator->() noexcept;
        const T* operator->() const noexcept;
       #endif

        //==============================================================================
        ScopedAccess(const ScopedAccess&) = delete;
        ScopedAccess(ScopedAccess &&) = delete;
        ScopedAccess& operator=(const ScopedAccess&) = delete;
        ScopedAccess& operator=(ScopedAccess&&) = delete;
    };
private:
    explicit NonRealtimeMutatable(std::unique_ptr<T> && u);

    std::unique_ptr<T> storage;
    std::atomic<T*> pointer;

    std::mutex nonRealtimeLock;
    std::unique_ptr<T> copy;

    // only accessed by realtime thread
    T* currentObj = nullptr;
};
}

//==============================================================================

#include <cassert>

namespace farbot
{
    template <typename T>
    NonRealtimeMutatable<T>::NonRealtimeMutatable() : storage(std::make_unique<T>()), pointer(storage.get()) {}

    template <typename T>
    NonRealtimeMutatable<T>::NonRealtimeMutatable(const T & obj) : storage(std::make_unique<T>(obj)), pointer(storage.get()) {}

    template <typename T>
    NonRealtimeMutatable<T>::NonRealtimeMutatable(T && obj) : storage(std::make_unique<T>(std::move(obj))), pointer(storage.get()) {}

    template <typename T>
    NonRealtimeMutatable<T>::~NonRealtimeMutatable()
    {
        assert(pointer.load() != nullptr); // <- never delete this object while the realtime thread is holding the lock

        // Spin!
        while (pointer.load() == nullptr);

        auto accquired = nonRealtimeLock.try_lock();

        ((void)(accquired));
        assert(accquired);  // <- you didn't call release on one of the non-realtime threads before deleting this object

        nonRealtimeLock.unlock();
    }

    template <typename T>
    template <typename... Args>
    NonRealtimeMutatable<T> NonRealtimeMutatable<T>::create(Args && ... args)
    {
        return RealtimeObject(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <typename T>
    const T& NonRealtimeMutatable<T>::realtimeAcquire()
    {
        assert(pointer.load() != nullptr); // <- You didn't balance your acquire and release calls!
        currentObj = pointer.exchange(nullptr);
        return *currentObj;
    }

    template <typename T>
    void NonRealtimeMutatable<T>::realtimeRelease()
    {
        // You didn't balance your acquire and release calls
        assert(pointer.load() == nullptr);

        // replace back
        pointer.store(currentObj);
    }

    template <typename T>
    T& NonRealtimeMutatable<T>::nonRealtimeAcquire()
    {
        nonRealtimeLock.lock();
        copy.reset(new T(*storage));

        return *copy.get();
    }

    template <typename T>
    void NonRealtimeMutatable<T>::nonRealtimeRelease()
    {
        T* ptr;

        // block until realtime thread is done using the object
        do {
            ptr = storage.get();
        } while (!pointer.compare_exchange_weak(ptr, copy.get()));

        storage = std::move(copy);
        nonRealtimeLock.unlock();
    }

    template <typename T>
    template <typename... Args>
    void NonRealtimeMutatable<T>::nonRealtimeReplace(Args && ... args)
    {
        nonRealtimeLock.lock();
        copy.reset(new T(std::forward<Args>(args)...));

        nonRealtimeRelease();
    }

    template <typename T>
    NonRealtimeMutatable<T>::NonRealtimeMutatable(std::unique_ptr<T> && u) : storage(std::move(u)), pointer(storage.get()) {}

    //==============================================================================
    namespace detail
    {
        template <typename T, bool>
        class NRMScopedAccessImpl
        {
        protected:
            NRMScopedAccessImpl(NonRealtimeMutatable<T>& parent)
                : p(parent), currentValue(&p.nonRealtimeAcquire()) {}
            ~NRMScopedAccessImpl() { p.nonRealtimeRelease(); }
        public:
            T* get()                        { return currentValue; }
            const T* get() const            { return currentValue; }
            T &operator *()                 { return *currentValue; }
            const T &operator *() const     { return *currentValue; }
            T* operator->()                 { return currentValue; }
            const T* operator->() const     { return currentValue; }
        private:
            NonRealtimeMutatable<T>& p;
            T* currentValue;
        };

        template <typename T>
        class NRMScopedAccessImpl<T, true>
        {
        protected:
            NRMScopedAccessImpl(NonRealtimeMutatable<T>& parent)
                : p(parent), currentValue(&p.realtimeAcquire()) {}
            ~NRMScopedAccessImpl()  { p.realtimeRelease(); }
        public:
            const T* get() const            { return currentValue; }
            const T &operator *() const     { return *currentValue; }
            const T* operator->() const     { return currentValue; }
        private:
            NonRealtimeMutatable<T>& p;
            const T* currentValue;
        };
    }

    template <typename T>
    template <bool isRealtimeThread>
    NonRealtimeMutatable<T>::ScopedAccess<isRealtimeThread>::ScopedAccess(NonRealtimeMutatable<T>& parent)
        : detail::NRMScopedAccessImpl<T, isRealtimeThread>(parent) {}
}