#include <string_view>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "concurrentqueue/concurrentqueue.h"

using namespace std::chrono_literals;

// Prints the `text` to stdout, appends '\n' and flushes the stream in a thread-safe manner.
// Should not be used by the final logger implementation. Useful for debugging.
void print(std::string_view text);

// Helper function, that reverses a singly linked list.
template <class T>
constexpr T* reverse(T* head) noexcept {
    T* list = nullptr;
    while (head) {
        const auto next = head->next;
        head->next = list;
        list = head;
        head = next;
    }
    return list;
}

// =================================================================================================
// Multithreading
// =================================================================================================
// Task: Implement `logger` as a multiple producers, singele consumer queue as efficient as you can.

//class logger {
//public:
//	// Queues the message. Called from multiple threads.
//	void post(std::string text) {
//		std::lock_guard<std::mutex> lock(mutex_);
//		queue_.emplace_back(std::move(text));
//	}
//
//	// Processes messages. Called from a single thread.
//	void run(std::stop_token stop) {
//		while (true) {
//			while (queue_.empty()) {
//				if (stop.stop_requested())
//					return;
//				std::this_thread::sleep_for(100ms);
//			}
//			const auto& e = std::exchange(queue_, {});
//			//for (const auto& e : std::exchange(queue_, {}))
//			//{
//			//	//if (e. == nullptr) continue;
//			//	std::fputs(e.data(), stdout); // 
//			//}
//			std::fflush(stdout);
//		}
//	}
//
//private:
//	std::deque<std::string> queue_;
//	mutable std::mutex mutex_;
//};

class logger_with_mutex {
public:
    // Queues the message. Called from multiple threads.
    void post(std::string text) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.emplace_back(std::move(text));
        }
        cv_.notify_one(); // Wake up consumer if sleeping
    }

    // Processes messages. Called from a single thread.
    void run(std::stop_token stop) {
        std::deque<std::string> local_queue;

        while (!stop.stop_requested()) {
            // Wait for work or stop request
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] {
                    return stop.stop_requested() || !queue_.empty();
                    });

                if (stop.stop_requested() && queue_.empty()) {
                    return;
                }

                // Swap queues to minimize lock time
                local_queue.swap(queue_);
            }

            // Process messages outside of lock
            for (const auto& msg : local_queue) {
                std::fputs(msg.data(), stdout);
            }
            std::fflush(stdout);
            local_queue.clear();
        }
    }

private:
    std::deque<std::string> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};


// credits to https://github.com/cameron314/concurrentqueue
class logger_with_concurrentqueue {
public:
    void post(std::string text) {
        queue_.enqueue(std::move(text));
        // enqueue — lock-free for MPSC (ConcurrentQueue)
    }

    void run(std::stop_token stop) {
        std::string msg;
        while (!stop.stop_requested()) {
            // try to get message
            if (queue_.try_dequeue(msg)) {
                std::fputs(msg.data(), stdout);
                std::fflush(stdout);
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // Выгребаем остатки
        while (queue_.try_dequeue(msg)) {
            std::fputs(msg.data(), stdout);
        }
        std::fflush(stdout);
    }

private:
    moodycamel::ConcurrentQueue<std::string> queue_;
};

#define logger_with_mutex logger_with_mutex
#define logger_with_concurrentqueue logger_with_concurrentqueue
#define logger_with_test logger_with_concurrentqueue
