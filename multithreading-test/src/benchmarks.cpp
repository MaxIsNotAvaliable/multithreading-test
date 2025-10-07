// =================================================================================================
// Benchmark
// =================================================================================================
// Threads: 16, Iterations: 1'600'000 (Total)
// CPU: AMD Ryzen 7 PRO 6850U (16 Cores)
// CPU Caches:
//   L1 Data 32 KiB (x8)
//   L1 Instruction 32 KiB (x8)
//   L2 Unified 512 KiB (x8)
//   L3 Unified 16384 KiB (x1)
//
// -------------------------------------------------------------------------------------------------
// Linux                                          Time             CPU
// -------------------------------------------------------------------------------------------------
// literal<logger>                              326 ns          253 ns
// dynamic<logger>                              291 ns          239 ns
// dynamic<logger_with_spdlog>                18848 ns         1971 ns
// dynamic<logger_with_tbb_bounded_queue>     33399 ns         2396 ns
// dynamic<logger_with_tbb_queue>              3967 ns         2794 ns
// dynamic<logger_with_boost_lockfree_queue>   8375 ns         5087 ns
// dynamic<logger_with_boost_asio>             7829 ns         4808 ns
// dynamic<logger_with_mutex>                 11033 ns          662 ns
// dynamic<logger_with_coroutines>             9231 ns         5564 ns
// dynamic<logger_with_condition_variable>     3417 ns         1574 ns
// dynamic<logger_with_semaphore>              3502 ns         2076 ns
// dynamic<logger_with_atomic>                 3228 ns         1901 ns
// dynamic<logger_with_tbb_allocator>          1680 ns         1580 ns
//
// -------------------------------------------------------------------------------------------------
// Windows                                        Time             CPU
// -------------------------------------------------------------------------------------------------
// literal<logger>                              247 ns          137 ns
// dynamic<logger>                              246 ns          137 ns
// dynamic<logger_with_spdlog>                51124 ns         1426 ns
// dynamic<logger_with_tbb_bounded_queue>     49653 ns          508 ns
// dynamic<logger_with_tbb_queue>              2211 ns         1816 ns
// dynamic<logger_with_boost_lockfree_queue>   5669 ns         4766 ns
// dynamic<logger_with_boost_asio>             6548 ns         5342 ns
// dynamic<logger_with_mutex>                 21222 ns          537 ns
// dynamic<logger_with_coroutines>             1320 ns          537 ns
// dynamic<logger_with_condition_variable>      961 ns          742 ns
// dynamic<logger_with_semaphore>              1056 ns          791 ns
// dynamic<logger_with_atomic>                 1012 ns          723 ns
// dynamic<logger_with_tbb_allocator>           850 ns          596 ns

#define _CRT_SECURE_NO_WARNINGS 1

#include "logger.hpp"
#include <benchmark/benchmark.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <thread>
#include <system_error>
#include <semaphore>

template <class Logger>
std::shared_ptr<Logger> get_logger() {
  static std::weak_ptr<Logger> wp;
  auto sp = wp.lock();
  if (!sp) {
    static std::mutex mutex;
    const std::lock_guard lock{mutex};
    sp = wp.lock();
    if (!sp) {
      sp = std::make_shared<Logger>();
      wp = sp;
    }
  }
  return sp;
}

// Make sure we're posting a dynamically allocated string.
static_assert(std::string{}.capacity() < 20);

template <class T>
static void dynamic(benchmark::State& state) {
  auto logger = get_logger<T>();
  const std::string s{"12345678901234567890\n"};
  for (auto _ : state)
    logger->post(s);
}

template <class T>
static void literal(benchmark::State& state) {
  auto logger = get_logger<T>();
  for (auto _ : state)
    logger->post("12345678901234567890\n");
}

constexpr int threads = 16;
constexpr benchmark::IterationCount count = 100'000;

#ifdef logger
BENCHMARK(literal<logger>)->Threads(threads)->Iterations(count);
BENCHMARK(dynamic<logger>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_spdlog
BENCHMARK(dynamic<logger_with_spdlog>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_tbb_bounded_queue
BENCHMARK(dynamic<logger_with_tbb_bounded_queue>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_tbb_queue
BENCHMARK(dynamic<logger_with_tbb_queue>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_boost_lockfree_queue
BENCHMARK(dynamic<logger_with_boost_lockfree_queue>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_boost_asio
BENCHMARK(dynamic<logger_with_boost_asio>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_mutex
BENCHMARK(dynamic<logger_with_mutex>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_coroutines
BENCHMARK(dynamic<logger_with_coroutines>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_condition_variable
BENCHMARK(dynamic<logger_with_condition_variable>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_semaphore
BENCHMARK(dynamic<logger_with_semaphore>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_atomic
BENCHMARK(dynamic<logger_with_atomic>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_tbb_allocator
BENCHMARK(dynamic<logger_with_tbb_allocator>)->Threads(threads)->Iterations(count);
#endif

#ifdef logger_with_concurrentqueue
BENCHMARK(dynamic<logger_with_concurrentqueue>)->Threads(threads)->Iterations(count);
#endif

template <class T>
std::jthread create_logger() {
  std::binary_semaphore started{0};
  std::jthread thread{[semaphore = &started](std::stop_token stop) {
    semaphore->release();
    get_logger<T>()->run(stop);
  }};
  started.acquire();
  return thread;
}

class reporter : public benchmark::ConsoleReporter {
public:
  reporter() : stdout_{_dup(_fileno(stdout))} {
    if (stdout_ == -1) {
      throw std::system_error{
        std::error_code{errno, std::system_category()},
        "Could not duplicate stdout handle"
      };
    }
    auto null = freopen(
#ifdef _WIN32
      "NUL",
#else
      "/dev/null",
#endif
      "w", stdout);
  }

  reporter(reporter&& other) = delete;
  reporter(const reporter& other) = delete;
  reporter& operator=(reporter&& other) = delete;
  reporter& operator=(const reporter& other) = delete;

  ~reporter() {
    fflush(stdout);
    (void)_dup2(stdout_, _fileno(stdout));
    _close(stdout_);
    benchmark::ConsoleReporter::ReportRuns(reports_);
  }

  void ReportRuns(const std::vector<Run>& reports) override {
    reports_.insert(end(reports_), begin(reports), end(reports));
  }

private:
  int stdout_;
  std::vector<Run> reports_;
};

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return EXIT_FAILURE;

  std::list<std::jthread> logger_threads;
#ifdef logger
  logger_threads.emplace_back(create_logger<logger>());
#endif
#ifdef logger_with_tbb_bounded_queue
  logger_threads.emplace_back(create_logger<logger_with_tbb_bounded_queue>());
#endif
#ifdef logger_with_tbb_queue
  logger_threads.emplace_back(create_logger<logger_with_tbb_queue>());
#endif
#ifdef logger_with_boost_lockfree_queue
  logger_threads.emplace_back(create_logger<logger_with_boost_lockfree_queue>());
#endif
#ifdef logger_with_boost_asio
  logger_threads.emplace_back(create_logger<logger_with_boost_asio>());
#endif
#ifdef logger_with_mutex
  logger_threads.emplace_back(create_logger<logger_with_mutex>());
#endif
#ifdef logger_with_coroutines
  logger_threads.emplace_back(create_logger<logger_with_coroutines>());
#endif
#ifdef logger_with_condition_variable
  logger_threads.emplace_back(create_logger<logger_with_condition_variable>());
#endif
#ifdef logger_with_semaphore
  logger_threads.emplace_back(create_logger<logger_with_semaphore>());
#endif
#ifdef logger_with_atomic
  logger_threads.emplace_back(create_logger<logger_with_atomic>());
#endif
#ifdef logger_with_tbb_allocator
  logger_threads.emplace_back(create_logger<logger_with_tbb_allocator>());
#endif
#ifdef logger_with_concurrentqueue
  logger_threads.emplace_back(create_logger<logger_with_concurrentqueue>());
#endif

  reporter reporter;
  benchmark::RunSpecifiedBenchmarks(&reporter);
  for (auto& thread : logger_threads) {
    thread.request_stop();
    thread.join();
  }
}
