#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

namespace managed
{
class ThreadPool {
public:
    // Constructor: Initialize the thread pool with the maximum number of threads
    explicit ThreadPool(std::size_t maxThreads)
        : stopFlag(false) {
        for (std::size_t i = 0; i < maxThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    { // Acquire lock to access task queue
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] {
                            return stopFlag || !tasks.empty();
                        });

                        if (stopFlag && tasks.empty()) {
                            return;
                        }

                        task = std::move(tasks.front());
                        tasks.pop();
                    } // Release lock here

                    task(); // Execute the task
                }
            });
        }
    }

    // Destructor: Join all threads and cleanup
    ~ThreadPool() {
        { // Signal all threads to stop
            std::unique_lock<std::mutex> lock(queueMutex);
            stopFlag = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Add a task to the thread pool
    template <typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        using ReturnType = decltype(func(args...));

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<ReturnType> result = task->get_future();

        { // Lock the task queue to add a new task
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stopFlag) {
                throw std::runtime_error("ThreadPool is stopping, cannot enqueue new tasks.");
            }

            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one(); // Notify one thread to handle the task
        return result;
    }

private:
    std::vector<std::thread> workers;           // Worker threads
    std::queue<std::function<void()>> tasks;    // Task queue

    std::mutex queueMutex;                      // Mutex for synchronizing access to the queue
    std::condition_variable condition;          // Condition variable for thread synchronization
    std::atomic<bool> stopFlag;                 // Flag to signal stopping the pool
};
}

