#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include "safe_queue.h"
#include <future>
#include <functional>

class ThreadPool {
public:
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    explicit ThreadPool(const int n_threads = 4) 
    : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false) {

    }

    void init() {
        for(int i = 0; i < (int) m_threads.size(); i++) {
            m_threads.at(i) = std::thread(ThreadWorker(this, i)); // 分配工作线程
        }
    }

    void shutdown() {
        {
            // 对 m_shutdown 加锁
            std::unique_lock<std::mutex> lock(m_conditional_mutex);
            m_shutdown = true;
            m_conditional_lock.notify_all(); // 通知，唤醒所有工作线程
        }
        for(int i = 0; i < (int) m_threads.size(); i++) if(m_threads.at(i).joinable()) {
            m_threads.at(i).join();
        }
    }

    template<typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        // Create a function with bounded parameter ready to excute
         std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // Encapsulate it into a shared pointer in order to be able to copy construct
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // Wrap packaged task into void function
        std::function<void()> wrapper_func = [task_ptr]() {
            (*task_ptr)();
        };

        // 队列通用安全封包函数，并压入安全队列
        m_queue.enqueue(wrapper_func);
        
        // 唤醒一个g等待中的线程
        m_conditional_lock.notify_one();

        // 返回a先前注册的任务指针
        return task_ptr->get_future();
    }
    
    ~ThreadPool() {
        if(m_shutdown) return ;
        m_shutdown = true;
        m_conditional_lock.notify_all(); // 通知，唤醒所有工作线程
        for(int i = 0; i < (int) m_threads.size(); i++) if(m_threads.at(i).joinable()) {
            m_threads.at(i).join();
        }
    }

private:

    bool m_shutdown; // 线程池是否关闭

    safe_queue<std::function<void()>> m_queue; // 执行函数安全队列，即任务队列

    std::vector<std::thread> m_threads; // 工作线程队列

    std::mutex m_conditional_mutex; // 线程休眠锁互斥变量

    std::condition_variable m_conditional_lock; // 线程环境锁，可以让线程处于休眠或者唤醒线程

    class ThreadWorker {
    public:
    ThreadWorker(ThreadPool *pool, const int id) 
    : m_pool(pool), m_id(id) {}

    void operator()() {
        std::function<void()> func; // 定义基础函数

        bool dequeued; // 是否正在取出队列中的元素

        while(true) {
            {
                // 为线程环境加锁，负责访问工作线程的休眠和唤醒
                std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);

                // 加锁后访问 m_shutdown，保证不会出现
                // m_shutdown从0到1的过程中出现线程访问任务队列
                if(m_pool->m_shutdown) break;

                // 如果任务队列为空，阻塞当前线程
                if(m_pool->m_queue.empty()) {
                    m_pool->m_conditional_lock.wait(lock);
                }

                // 取出任务队列中的元素
                dequeued = m_pool->m_queue.dequeue(func);
            }

            if(dequeued) {
                func();
            }
        }

    }

    private:
        int m_id; // 工作id
        ThreadPool *m_pool; // 所属线程池
    };
};

#endif