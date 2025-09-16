#include <bits/stdc++.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <thread>
#include <random>

using namespace std;

void icache_load(long iterations) {
    // эмуляция i-cache нагрузки: много мелких функций
    auto tiny_func = [](int x) { return x * x + 1; };
    volatile int sink = 0;
    for (long i = 0; i < iterations; i++) {
        sink += tiny_func(i);
    }
}

void cache_load(long iterations, size_t size) {
    // эмуляция нагрузки на кэш: работаем с большим массивом
    vector<int> data(size, 1);
    volatile int sink = 0;
    for (long r = 0; r < iterations; r++) {
        for (size_t i = 0; i < data.size(); i++) {
            data[i] += 1;
            sink += data[i];
        }
    }
}

void branch_load(long iterations) {
    // нагрузка на предсказатель ветвлений: непредсказуемое поведение
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 1);
    volatile int sink = 0;
    for (long i = 0; i < iterations; i++) {
        if (dist(rng))
            sink++;
        else
            sink--;
    }
}

void flush_cache(vector<int>& big_buffer) {
    // "очистка кэша" - просто проходим по большой памяти, чтобы вытеснить все данные
    for (size_t i = 0; i < big_buffer.size(); i++) {
        big_buffer[i] += 1;
    }
}

int main(int argc, char** argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " <iterations_icache> <iterations_cache> <cache_size> <iterations_branch> <threads>\n";
        return 1;
    }

    long icache_iter = atol(argv[1]);
    long cache_iter  = atol(argv[2]);
    size_t cache_size = atol(argv[3]);
    long branch_iter = atol(argv[4]);
    int threads_count = atoi(argv[5]);

    cout << "Running stress-ng-like test with " << threads_count << " threads\n";

    struct rusage usage_start{}, usage_end{};
    struct timespec tstart{}, tend{};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    getrusage(RUSAGE_SELF, &usage_start);

    vector<thread> threads;
    vector<int> flush_buffer(10 * 1024 * 1024, 0); // 10 MB для flushcache

    for (int t = 0; t < threads_count; t++) {
        threads.emplace_back([=, &flush_buffer]() {
            for (int round = 0; round < 5; round++) {
                icache_load(icache_iter);
                cache_load(cache_iter, cache_size);
                branch_load(branch_iter);
                flush_cache(flush_buffer);
            }
        });
    }

    for (auto& th : threads) th.join();

    getrusage(RUSAGE_SELF, &usage_end);
    clock_gettime(CLOCK_MONOTONIC, &tend);

    double real = (tend.tv_sec - tstart.tv_sec) + (tend.tv_nsec - tstart.tv_nsec)/1e9;
    double user = (usage_end.ru_utime.tv_sec - usage_start.ru_utime.tv_sec)
                  + (usage_end.ru_utime.tv_usec - usage_start.ru_utime.tv_usec)/1e6;
    double sys = (usage_end.ru_stime.tv_sec - usage_start.ru_stime.tv_sec)
                 + (usage_end.ru_stime.tv_usec - usage_start.ru_stime.tv_usec)/1e6;

    cout << "Results:\n";
    cout << "real=" << real << "s, user=" << user << "s, sys=" << sys << "s\n";
    cout << "voluntary ctxt switches: " << usage_end.ru_nvcsw - usage_start.ru_nvcsw
         << ", involuntary: " << usage_end.ru_nivcsw - usage_start.ru_nivcsw << "\n";

    return 0;
}
