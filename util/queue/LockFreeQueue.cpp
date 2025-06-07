// lockfree_queue.hpp
#pragma once
#include <concurrentqueue/concurrentqueue.h>
#include <functional>

template <typename T>
class LockFreeQueue {
public:
    void push(const T& item) {
        queue_.enqueue(item);
    }

    bool pop(T& item) {
        return queue_.try_dequeue(item);
    }

    size_t size() const {
        return queue_.size_approx();
    }

private:
    moodycamel::ConcurrentQueue<T> queue_;
};