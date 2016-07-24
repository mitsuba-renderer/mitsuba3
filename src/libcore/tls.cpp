#include <mitsuba/core/tls.h>
#include <tbb/spin_mutex.h>
#include <tbb/mutex.h>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <list>

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
    tbb::spin_mutex mutex;
    std::unordered_map<ThreadLocalBase *, TLSEntry> entries;
    std::list<TLSEntry *> entries_ordered;
    uint32_t refCount = 1;
};

#if defined(__WINDOWS__)
    static __declspec(thread) PerThreadData *ptdLocal = nullptr;
#elif defined(__LINUX__)
    static __thread PerThreadData *ptdLocal = nullptr;
#elif defined(__OSX__)
    static pthread_key_t ptdLocal;
#endif

/// List of all PerThreadData data structures (one for each thread)
static std::unordered_set<PerThreadData *> ptdGlobal;

/// Lock to protect ptdGlobal
static tbb::mutex ptdGlobalLock;

ThreadLocalBase::ThreadLocalBase(const ConstructFunctor &constructFunctor,
                                 const DestructFunctor &destructFunctor)
    : m_constructFunctor(constructFunctor), m_destructFunctor(destructFunctor) { }

ThreadLocalBase::~ThreadLocalBase() {
    tbb::mutex::scoped_lock guard(ptdGlobalLock);

    /* For every thread */
    for (auto ptd : ptdGlobal) {
        tbb::spin_mutex::scoped_lock guard2(ptd->mutex);

        /* If the current TLS object is referenced, destroy the contents */
        auto it2 = ptd->entries.find(this);
        if (it2 == ptd->entries.end())
            continue;

        auto entry = it2->second;
        ptd->entries_ordered.erase(entry.iterator);
        ptd->entries.erase(it2);
        guard2.release();
        m_destructFunctor(entry.data);
    }
}

void *ThreadLocalBase::get() {
    /* Look up a TLS entry. The goal is to make this entire operation very fast! */

    #if defined(__OSX__)
        PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptdLocal);
    #else
        PerThreadData *ptd = ptdLocal;
    #endif

    if (unlikely(!ptd))
        throw std::runtime_error(
            "Internal error: call to ThreadLocalPrivate::get() precedes the "
            "construction of thread-specific data structures!");

    /* This is an uncontended thread-local lock (i.e. not to worry) */
    tbb::spin_mutex::scoped_lock guard(ptd->mutex);

    auto it = ptd->entries.find(this);
    if (likely(it != ptd->entries.end()))
        return it->second.data;

    /* This is the first access from this thread */
    ptd->entries.insert(std::make_pair(this, TLSEntry {
        m_constructFunctor(),
        m_destructFunctor,
        std::list<TLSEntry *>::iterator()
    }));

    TLSEntry &entry = ptd->entries.find(this)->second;

    ptd->entries_ordered.push_back(&entry);
    entry.iterator = --ptd->entries_ordered.end();

    return entry.data;
}

void ThreadLocalBase::staticInitialization() {
#if defined(__OSX__)
    pthread_key_create(&ptdLocal, nullptr);
#endif
}

void ThreadLocalBase::staticShutdown() {
#if defined(__OSX__)
    pthread_key_delete(ptdLocal);
    memset(&ptdLocal, 0, sizeof(pthread_key_t));
#endif
}

bool ThreadLocalBase::registerThread() {
    tbb::mutex::scoped_lock guard(ptdGlobalLock);
    bool success = false;
#if defined(__OSX__)
    PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptdLocal);
    if (!ptd) {
        ptd = new PerThreadData();
        ptdGlobal.insert(ptd);
        pthread_setspecific(ptdLocal, ptd);
        success = true;
        return true;
    } else {
        ptd->refCount++;
    }
#else
    if (!ptdLocal) {
        auto ptd = new PerThreadLocal();
        ptdLocal = ptd;
        ptdGlobal.insert(ptd);
        return true;
    } else {
        ptdLocal->refCount++;
    }
#endif
    return false;
}

/// A thread has died -- destroy any remaining TLS entries associated with it
void ThreadLocalBase::unregisterThread() {
    tbb::mutex::scoped_lock guard(ptdGlobalLock);

    #if defined(__OSX__)
        PerThreadData *ptd = (PerThreadData *) pthread_getspecific(ptdLocal);
    #else
        PerThreadData *ptd = ptdLocal;
    #endif
    ptd->refCount--;

    if (ptd->refCount == 0) {
        tbb::spin_mutex::scoped_lock local_guard(ptd->mutex);
        for (auto it = ptd->entries_ordered.rbegin();
             it != ptd->entries_ordered.rend(); ++it)
            (*it)->destruct((*it)->data);

        local_guard.release();
        ptdGlobal.erase(ptd);
        delete ptd;

        #if defined(__OSX__)
            pthread_setspecific(ptdLocal, nullptr);
        #else
            ptdLocal = nullptr;
        #endif
    }
}

NAMESPACE_END(mitsuba)
