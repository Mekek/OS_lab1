#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <cstring>

using namespace std;

atomic<bool> stop_flag(false);

// --- Нагрузчик для icache ---
void icache_worker(size_t iterations) {
    volatile int result = 0;
    for (size_t i = 0; i < iterations && !stop_flag; i++) {
        result += i % 7; // простой код для загрузки i-cache
    }
}

// --- Нагрузчик для flushcache ---
void flushcache_worker(size_t iterations) {
    const size_t size = 8 * 1024 * 1024; // 8 МБ – больше кэша CPU
    vector<int> buffer(size);
    for (size_t i = 0; i < iterations && !stop_flag; i++) {
        for (size_t j = 0; j < size; j++) {
            buffer[j] = j + i;
        }
    }
}

// --- Нагрузчик для cache ---
void cache_worker(size_t iterations) {
    const size_t size = 1 * 1024 * 1024; // 1 МБ – частично помещается в кэш
    vector<int> buffer(size);
    for (size_t i = 0; i < iterations && !stop_flag; i++) {
        for (size_t j = 0; j < size; j++) {
            buffer[j]++;
        }
    }
}

// --- Нагрузчик для branch ---
void branch_worker(size_t iterations) {
    volatile int result = 0;
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(0, 1);

    for (size_t i = 0; i < iterations && !stop_flag; i++) {
        if (dist(gen)) {
            result += 1;
        } else {
            result -= 1;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] 
             << " --mode <icache|flushcache|cache|branch> --iterations <num> [--timeout <sec>]\n";
        return 1;
    }

    string mode;
    size_t iterations = 0;
    int timeout_sec = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = stoull(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            timeout_sec = stoi(argv[++i]);
        }
    }

    if (mode.empty() || iterations == 0) {
        cerr << "Error: mode and iterations are required!\n";
        return 1;
    }

    cout << "Running stress mode: " << mode 
         << " with " << iterations << " iterations";
    if (timeout_sec > 0) cout << " and timeout " << timeout_sec << " sec";
    cout << endl;

    // Таймер на завершение
    thread timer;
    if (timeout_sec > 0) {
        timer = thread([&]() {
            this_thread::sleep_for(chrono::seconds(timeout_sec));
            stop_flag = true;
        });
    }

    if (mode == "icache") {
        icache_worker(iterations);
    } else if (mode == "flushcache") {
        flushcache_worker(iterations);
    } else if (mode == "cache") {
        cache_worker(iterations);
    } else if (mode == "branch") {
        branch_worker(iterations);
    } else {
        cerr << "Unknown mode: " << mode << endl;
        return 1;
    }

    if (timer.joinable()) timer.join();

    cout << "Done!" << endl;
    return 0;
}
