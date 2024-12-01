#include <iostream>
#include <chrono>
#include "thread_pool.h"

using namespace managed;

int main() {
    ThreadPool pool(4); // Create a thread pool with 4 threads

    // Enqueue some tasks
    auto task1 = pool.enqueue([]() {
        std::cout << "Task 1 is running\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 1;
    });

    auto task2 = pool.enqueue([](int x) {
        std::cout << "Task 2 is running with arg: " << x << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return x * 2;
    }, 42);

    auto task3 = pool.enqueue([] {
        std::cout << "Task 3 is running\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });

    // Wait for tasks to complete and retrieve their results
    std::cout << "Result of Task 1: " << task1.get() << "\n";
    std::cout << "Result of Task 2: " << task2.get() << "\n";

    task3.get(); // Wait for task3 to complete
    std::cout << "Task 3 completed\n";

    return 0;
}

