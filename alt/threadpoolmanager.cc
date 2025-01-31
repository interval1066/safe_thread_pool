#include <iostream>
#include <cstdlib>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <stdexcept>
#include <memory>
#include <atomic>

/**
 *    Task Struct:
 *
 *        A Task struct is introduced to hold both the priority and the task function.
 *
 *        The operator< is overloaded to ensure that higher-priority tasks are placed at the top of the priority queue.
 *
 *    Priority Queue:
 *
 *        The std::priority_queue is used to store tasks. It automatically orders tasks based on their priority.
 *
 *    Enqueue Method:
 *
 *        The enqueue method now takes an additional priority parameter. Tasks are pushed into the priority queue with their 
 * associated priority.
 *
 *    Worker Threads:
 *
 *        Worker threads now retrieve tasks from the priority queue, ensuring that higher-priority tasks are executed first.
 *
 *    Main Function:
 *
 *        The main function demonstrates task prioritization by enqueuing tasks with different priorities and retrieving their
 * results.
 *
 * Notes
 *
 *    The std::priority_queue ensures that tasks with higher priority values are executed first. If you want lower values to
 * represent higher priority, you can modify the operator< in the Task struct.
 *
 *    This implementation assumes that the priority is an integer. You can extend it to support other types of priorities if
 * needed.
 *
 *    The resize and shutdown methods remain unchanged, ensuring that the thread pool can still dynamically adjust its size and 
 * shut down gracefully.
 *
 * This implementation provides a flexible thread pool with support for task prioritization, allowing you to control the
 * order in which tasks are executed based
 *
 * on their importance.
 */

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(int priority, F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    void resize(size_t newSize);
    void shutdown();

private:
    struct Task {
        int priority;
        std::function<void()> function;

        bool operator<(const Task& other) const {
            return priority < other.priority; // Higher priority tasks come first
        }
    };

    void addWorker();
    void removeWorker();

    std::vector<std::thread> workers;
    std::priority_queue<Task> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> currentThreadCount;
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false), currentThreadCount(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        addWorker();
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

template<class F, class... Args>
auto ThreadPool::enqueue(int priority, F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        if (!task) {
            throw std::runtime_error("Null task encountered");
        }

        tasks.push({priority, [task]() { (*task)(); }});
    }
    condition.notify_one();
    return res;
}

void ThreadPool::addWorker() {
    workers.emplace_back([this] {
        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lock(this->queueMutex);
                this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                if (this->stop && this->tasks.empty()) {
                    return;
                }
                task = std::move(this->tasks.top());
                this->tasks.pop();
            }

            task.function();
        }
    });
    ++currentThreadCount;
}

void ThreadPool::removeWorker() {
    if (currentThreadCount > 0) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();
        stop = false;
        currentThreadCount = 0;
    }
}

void ThreadPool::resize(size_t newSize) {
    if (newSize == currentThreadCount) {
        return;
    }

    if (newSize > currentThreadCount) {
        for (size_t i = currentThreadCount; i < newSize; ++i) {
            addWorker();
        }
    } else {
        for (size_t i = newSize; i < currentThreadCount; ++i) {
            removeWorker();
        }
    }
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();
    currentThreadCount = 0;
}

int main() {
    ThreadPool pool(4);

    auto future1 = pool.enqueue(1, [](int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return a + b;
    }, 1, 2);

    auto future2 = pool.enqueue(3, [](int a, int b) {
        return a * b;
    }, 3, 4);

    auto future3 = pool.enqueue(2, [](int a, int b) {
        return a - b;
    }, 5, 3);

    std::cout << "Result 1: " << future1.get() << std::endl;
    std::cout << "Result 2: " << future2.get() << std::endl;
    std::cout << "Result 3: " << future3.get() << std::endl;

    pool.resize(8); // Increase the number of threads
    std::cout << "Resized to 8 threads" << std::endl;

    pool.resize(2); // Decrease the number of threads
    std::cout << "Resized to 2 threads" << std::endl;

    pool.shutdown();

    return EXIT_SUCCESS;
}

