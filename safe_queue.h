#ifndef _SAFE_QUEUE_H_
#define _SAFE_QUEUE_H_

#include <queue>
#include <thread>
#include <mutex>

template<typename T>
class safe_queue {
public:
    safe_queue() = default;
    safe_queue(safe_queue &&rhs) {}
    ~safe_queue() = default;

    bool empty() {
        // 互斥信号变量加锁，防止 m_queue 被改变
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_queue.empty();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_queue.size();
    }

    void enqueue(T &t) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_queue.emplace(t);
    }

    bool dequeue(T &t) {
        std::unique_lock<std::mutex> lock(m_mutex);

        if(m_queue.empty()) {
            return false;
        }
        t = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
};

#endif