#if 0
#include <mitsuba/core/tls.h>
#include <mitsuba/core/thread.h>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>

#if defined(__OSX__)
  #include <pthread.h>
#endif

NAMESPACE_BEGIN(mitsuba)

struct TLSEntry {
    void *data;
    ThreadLocalBase::DestructFunctor destruct;
    std::list<TLSEntry *>::iterator iterator;
};

struct PerThreadData {
    std::mutex mutex;
    std::unordered_map<ThreadLocalBase *, TLSEntry> entries;
    std::list<TLSEntry *> entries_ordered;
    uint32_t ref_count = 1;
};

#if defined(__WINDOWS__)
    static __declspec(thread) PerThreadData *ptd_local = nullptr;
#elif defined(__LINUX__)
    static __thread PerThreadData *ptd_local = nullptr;
#elif defined(__OSX__)
    static pthread_key_t ptd_local;
#endif

/// List of all PerThreadData data structures (one for each thread)
static std::unordered_set<PerThreadData *> ptd_global;

/// Lock to protect ptd_global
static std::mutex ptd_global_lock;

ThreadLocalBase::ThreadLocalBase(const ConstructFunctor &construct_functor,
                                 const DestructFunctor &destruct_functor)
    : m_construct_functor(construct_functor), m_destruct_functor(destruct_functor) { }

ThreadLocalBase::~ThreadLocalBase() {
    clear();
}

void ThreadLocalBase::clear() {
    std::lock_guard<std::mutex> guard(ptd_global_lock);

    /* For every thread */
    for (auto ptd : ptd_global) {
        ptd->mutex.lock();

        /* If the current TLS object is referenced, destroy the contents */
        auto it2 = ptd->entries.find(this);
        if (it2 == ptd->entries.end())
            ptd->mutex.unlock();
            continue;

        auto entry = it2->second;
        ptd->entries_ordered.erase(entry.iterator);
        ptd->entries.erase(it2);
        ptd->mutex.unlock();
        m_destruct_functor(entry.data);
    }
}

void *ThreadLocalBase::get() {
    /* Look up a TLS entry. The goal is to make this entire operation very fast! */

    #if defined(__OSX__)
        PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptd_local);
    #else
        PerThreadData *ptd = ptd_local;
    #endif

    if (unlikely(!ptd))
        throw std::runtime_error(
            "Internal error: call to ThreadLocalPrivate::get() precedes the "
            "construction of thread-specific data structures!");

    /* This is an uncontended thread-local lock (i.e. not to worry) */
    std::lock_guard<std::mutex> guard(ptd->mutex);

    auto it = ptd->entries.find(this);
    if (likely(it != ptd->entries.end()))
        return it->second.data;

    /* This is the first access from this thread */
    ptd->entries.insert(std::make_pair(this, TLSEntry {
        m_construct_functor(),
        m_destruct_functor,
        std::list<TLSEntry *>::iterator()
    }));

    TLSEntry &entry = ptd->entries.find(this)->second;

    ptd->entries_ordered.push_back(&entry);
    entry.iterator = --ptd->entries_ordered.end();

    return entry.data;
}

void ThreadLocalBase::static_initialization() {
#if defined(__OSX__)
    pthread_key_create(&ptd_local, nullptr);
#endif
}

void ThreadLocalBase::static_shutdown() {
#if defined(__OSX__)
    pthread_key_delete(ptd_local);
    memset(&ptd_local, 0, sizeof(pthread_key_t));
#endif
}

bool ThreadLocalBase::register_thread() {
    std::lock_guard<std::mutex> guard(ptd_global_lock);
#if defined(__OSX__)
    PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptd_local);
    if (!ptd) {
        ptd = new PerThreadData();
        ptd_global.insert(ptd);
        pthread_setspecific(ptd_local, ptd);
        return true;
    } else {
        ptd->ref_count++;
    }
#else
    if (!ptd_local) {
        auto ptd = new PerThreadData();
        ptd_local = ptd;
        ptd_global.insert(ptd);
        return true;
    } else {
        ptd_local->ref_count++;
    }
#endif
    return false;
}

/// A thread has died -- destroy any remaining TLS entries associated with it
bool ThreadLocalBase::unregister_thread() {
    std::lock_guard<std::mutex> guard(ptd_global_lock);

    #if defined(__OSX__)
        PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptd_local);
    #else
        PerThreadData *ptd = ptd_local;
    #endif
    if (!ptd)
        return false;

    ptd->ref_count--;

    if (ptd->ref_count == 0) {
        ptd->mutex.lock();
        for (auto it = ptd->entries_ordered.rbegin();
             it != ptd->entries_ordered.rend(); ++it)
            (*it)->destruct((*it)->data);

        ptd->mutex.unlock();
        ptd_global.erase(ptd);
        delete ptd;

        #if defined(__OSX__)
            pthread_setspecific(ptd_local, nullptr);
        #else
            ptd_local = nullptr;
        #endif
    }
    return true;
}

NAMESPACE_END(mitsuba)
#endif
