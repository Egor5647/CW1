#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <parlay/parallel.h>

const int parallel_cutoff = 262144;

void split(int* data, int n, int& less, int& greater) {
    int value = data[n / 2];
    less = 0;
    greater = n;
    int current = 0;
    while (current < greater) {
        if (data[current] < value) {
            int temp = data[less];
            data[less] = data[current];
            data[current] = temp;
            less++;
            current++;
        } else if (data[current] > value) {
            greater--;
            int temp = data[current];
            data[current] = data[greater];
            data[greater] = temp;
        } else {
            current++;
        }
    }
}

void seq(int* data, int n) {
    if (n <= 1) {
        return;
    }
    int less;
    int greater;
    split(data, n, less, greater);
    seq(data, less);
    seq(data + greater, n - greater);
}

void par(int* data, int n) {
    if (n <= parallel_cutoff) {
        seq(data, n);
        return;
    }
    int less;
    int greater;
    split(data, n, less, greater);
    parlay::par_do(
        [&] { par(data, less); },
        [&] { par(data + greater, n - greater); });
}

bool same_array(const int* a, const int* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

void check(const int* input, int n, bool sequential) {
    int* expected = new int[n];
    int* data = new int[n];
    for (int i = 0; i < n; ++i) {
        expected[i] = input[i];
        data[i] = input[i];
    }
    std::sort(expected, expected + n);
    if (sequential) {
        seq(data, n);
        if (!same_array(data, expected, n)) {
            std::fprintf(stderr, "seq failed, size=%d\n", n);
            std::exit(1);
        }
        for (int i = 0; i < n; i++) {
            data[i] = input[i];
        }
    }
    par(data, n);
    if (!same_array(data, expected, n)) {
        std::fprintf(stderr, "par failed, size=%d\n", n);
        std::exit(1);
    }
    delete[] expected;
    delete[] data;
}

void suite(bool sequential) {
    int input[8];
    for (int n = 0; n <= 8; ++n) {
        int variants = 1;
        for (int i = 0; i < n; ++i) variants *= 3;
        for (int variant = 0; variant < variants; ++variant) {
            int code = variant;
            for (int i = 0; i < n; ++i) {
                input[i] = code % 3 - 1;
                code /= 3;
            }
            check(input, n, sequential);
        }
    }
    int limits[] = {INT_MIN, 0, INT_MAX, -1, 1};
    check(limits, 5, sequential);
    int sizes[] = {23, 24, 25, 2047, 2048, 2049, 262143,
                   262144, 262145, 524287, 524288, 524289, 1000003};
    for (int index = 0; index < 13; index++) {
        int n = sizes[index];
        int* data = new int[n];
        for (int kind = 0; kind < 6; ++kind) {
            for (int i = 0; i < n; ++i) {
                if (kind == 0) data[i] = i;
                if (kind == 1) data[i] = n - i - 1;
                if (kind == 2) data[i] = 7;
                if (kind == 3) data[i] = i % 2;
                if (kind == 4) data[i] = std::rand() % 17 - 8;
                if (kind == 5) data[i] = std::rand();
            }
            check(data, n, sequential);
        }
        delete[] data;
    }
    for (int trial = 0; trial < 200; ++trial) {
        int n = std::rand() % 10000;
        int* data = new int[n];
        for (int i = 0; i < n; ++i) data[i] = std::rand();
        check(data, n, sequential);
        delete[] data;
    }
}

void run_tests() {
    std::srand(123456789);
    int thread_counts[3] = {1, 2, 4};
    for (int i = 0; i < 3; i++) {
        int workers = thread_counts[i];
        parlay::execute_with_scheduler(workers, [&] {
            suite(workers == 1);
        });
    }
    std::printf("Tests passed\n");
    std::fflush(stdout);
}

void benchmark(bool parallel) {
    int workers = 1;
    const char* mode = "seq";
    if (parallel) {
        workers = 4;
        mode = "par";
    }
    const int n = 100000000;
    parlay::execute_with_scheduler(workers, [&] {
        int* data = new int[n];
        double sum = 0;
        for (int run = 1; run <= 5; ++run) {
            int seed = 123456789 + run;
            std::srand(seed);
            for (int i = 0; i < n; i++) {
                data[i] = std::rand();
            }
            auto start = std::chrono::steady_clock::now();
            if (parallel) par(data, n);
            else seq(data, n);
            auto finish = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(finish - start).count();
            sum += seconds;
            std::printf("%s,%d,%.6f\n", mode, run, seconds);
            std::fflush(stdout);
        }
        std::printf("%s,mean,%.6f\n", mode, sum / 5);
        delete[] data;
    });
}

int main() {
    run_tests();
    std::printf("mode,run,seconds\n");
    benchmark(false);
    benchmark(true);
    return 0;
}
