#include <mitsuba/core/profiler.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>

#if defined(MTS_ENABLE_PROFILER)
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <tbb/tbb.h>
#include <array>
#include <map>

NAMESPACE_BEGIN(mitsuba)

static thread_local uint64_t profiler_flags_storage = 0;
uint64_t *profiler_flags() { return &profiler_flags_storage; }

struct ProfilerSample {
    uint64_t flags = (uint64_t) -1;
    uint64_t count = 0;
};

static std::array<ProfilerSample, MTS_PROFILE_HASH_SIZE> profiler_samples;

static void profiler_callback(int, siginfo_t *, void *) {
    uint64_t flags = *profiler_flags();

    uint64_t bucket_id =
        std::hash<uint64_t>{}(flags) % (profiler_samples.size() - 1);

    // Hash table with linear probing
    size_t tries = 0;
    while (tries < profiler_samples.size()) {
        ProfilerSample &bucket = profiler_samples[bucket_id];
        if (bucket.flags == (uint64_t) -1 || bucket.flags == flags)
            break;
        if (++bucket_id == profiler_samples.size())
            bucket_id = 0;
        ++tries;
    }

    if (tries == profiler_samples.size()) {
        Log(Warn, "Profiler hash table filled up -- you may need to increase "
                  "MTS_PROFILE_HASH_SIZE.");
        return;
    }

    ProfilerSample &bucket = profiler_samples[bucket_id];
    bucket.flags = flags;
    bucket.count++;
}

void Profiler::static_initialization() {
    if (!util::detect_debugger()) {
        (void) profiler_flags();

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = profiler_callback;
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGPROF, &sa, nullptr))
            Throw("profiler_start(): failure in sigaction(): %s", strerror(errno));

        itimerval timer;
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 1000000 / 100; // 100 Hz sampling
        timer.it_value = timer.it_interval;

        if (setitimer(ITIMER_PROF, &timer, nullptr))
            Throw("profiler_start(): failure in setitimer(): %s", strerror(errno));
    }
}

void Profiler::static_shutdown() {
    if (util::detect_debugger())
        return;

    itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;

    if (setitimer(ITIMER_PROF, &timer, nullptr))
        Throw("profiler_stop(): failure in setitimer(): %s", strerror(errno));
}

void Profiler::print_report() {
    using SampleMap = std::map<std::string, uint64_t>;

    uint64_t event_count_total = 0,
             buckets_used = 0;

    SampleMap leaf_results, hierarchical_results;

    size_t prefix_length = 0;
    size_t max_indent = 0;

    for (auto const &sample: profiler_samples) {
        if (sample.count == 0)
            continue;
        uint64_t sample_flags = sample.flags;

        event_count_total += sample.count;
        buckets_used++;

        size_t indent = 0;
        std::string name_hierarchical;
        for (int i = 0; i < int(ProfilerPhase::ProfilerPhaseCount); ++i) {
            uint64_t flag = 1ull << i;
            if (sample_flags & flag) {
                const char *name = profiler_phase_id[i];
                if (!name_hierarchical.empty())
                    name_hierarchical += "/";
                name_hierarchical += name;
                prefix_length = std::max(prefix_length, strlen(name));
                hierarchical_results[name_hierarchical] += sample.count;
                sample_flags &= ~flag;
                if (sample_flags == 0)
                    leaf_results[name] += sample.count;
                indent += 1;
            }
            max_indent = std::max(indent, max_indent);
        }

        if (name_hierarchical.empty()) {
            hierarchical_results["Idle"] += sample.count;
            leaf_results["Idle"] += sample.count;
        }
    }

    Log(Info, "Recorded %i samples, used %i/%i hash table entries.",
        event_count_total, buckets_used, profiler_samples.size());

    if (event_count_total < 250)
        Log(Warn, "Collected very few samples -- perform a longer "
                  "rendering to obtain more reliable profile data.");

    std::vector<std::pair<std::string, uint64_t>> leaf_results_sorted;
    leaf_results_sorted.reserve(leaf_results.size());
    for (const auto &r : leaf_results)
        leaf_results_sorted.push_back(r);

    std::sort(
        leaf_results_sorted.begin(), leaf_results_sorted.end(),
        [](auto a, auto b) { return a.second > b.second; });

    prefix_length += max_indent * 2 + 10;

    Log(Info, "\U000023F1  Profile (hierarchical):");
    for (auto kv : hierarchical_results) {
        int indent = 4;
        auto slash_index = kv.first.find_last_of("/");

        if (slash_index == std::string::npos)
            slash_index = -1;
        else
            indent += 2 * std::count(kv.first.begin(), kv.first.end(), '/');

        std::string suffix = kv.first.substr(slash_index + 1);

        Log(Info, "%s%s%s%.2f%%",
            std::string(indent, ' '),
            suffix,
            std::string(prefix_length - suffix.length() - indent, ' '),
            kv.second / float(event_count_total) * 100.f);
    }

    Log(Info, "\U000023F1  Profile (flat):");
    for (auto kv : leaf_results_sorted) {
        Log(Info, "    %s%s%.2f%%", kv.first,
            std::string(prefix_length - kv.first.length() - 4, ' '),
            kv.second / float(event_count_total) * 100.f);
    }
}

MTS_IMPLEMENT_CLASS(Profiler, Object)
NAMESPACE_END(mitsuba)
#endif
