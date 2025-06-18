#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <nanothread/nanothread.h>
#include <mutex>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

static ref<Thread> dummy_thread = nullptr;
static std::mutex task_mutex;
static std::vector<Task *> registered_tasks;

Thread::Thread() { }

Thread::~Thread() { }

FileResolver *Thread::file_resolver() {
    return mitsuba::file_resolver();
}

Logger *Thread::logger() {
    return mitsuba::logger();
}

void Thread::set_logger(Logger *logger) {
    mitsuba::set_logger(logger);
}

Thread *Thread::thread() {
    return dummy_thread.get();
}

void Thread::register_task(Task *task) {
    std::lock_guard guard(task_mutex);
    registered_tasks.push_back(task);
}

void Thread::wait_for_tasks() {
    std::vector<Task *> tasks;
    {
        std::lock_guard guard(task_mutex);
        tasks.swap(registered_tasks);
    }
    for (Task *task : tasks)
        task_wait_and_release(task);
}

void Thread::static_initialization() {
    dummy_thread = new Thread();
    // Initialize global file resolver
    set_file_resolver(new FileResolver());
}

void Thread::static_shutdown() {
    // Wait for and release all registered tasks
    for (auto& task : registered_tasks)
        task_wait_and_release(task);
    registered_tasks.clear();

    dummy_thread = nullptr;
    // Clear global file resolver
    set_file_resolver(nullptr);
}

NAMESPACE_END(mitsuba)
