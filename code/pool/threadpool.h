/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>                // 互斥量
#include <condition_variable>   // 条件变量
#include <queue>                // 容器-队列
#include <thread>               // 线程库
#include <functional>           // 回调
// 定义类，线程池
class ThreadPool {
public:
    // 有参构造函数，创建 threadCount 参数，如果不指定这个参数的值，则初始化为 8，创建 8 个线程
    // 初始化 pool_ 成员，std::make_shared<Pool>() 创建出来
    // explicit 关键字，防止构造函数操作时进行隐式转换
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);    // 测试用的
            // 创建 threadCount 个子线程
            for(size_t i = 0; i < threadCount; i++) {
                // std::thread().detach(); 创建一个线程，并设置线程分离，第一对括号中是每创建一个线程要执行的代码
                std::thread([pool = pool_] {    //将成员 pool_ 赋值给 pool(thread头文件中定义) 变量
                    std::unique_lock<std::mutex> locker(pool->mtx); //创建一个锁 locker，获取 pool 中的锁
                    // while 死循环，每个线程创建出来要不断地从队列里取数据，取到数据以后进行处理
                    while(true) {
                        // 判断队列不为空
                        if(!pool->tasks.empty()) {
                            auto task = std::move(pool->tasks.front()); // 从任务队列中取一个任务
                            pool->tasks.pop();  // 将取出的任务从队列中移除掉
                            locker.unlock();    // tasks 共享任务队列，对其操作需要加锁和解锁
                            task();             // 执行任务的函数 <functional>
                            locker.lock();
                        } 
                        // 判断队列为空
                        else if(pool->isClosed) break;  // 判断池子是否关闭，如果池子关闭，退出 while 循环，该线程结束并销毁
                        // 队列为空，池子没关闭
                        else pool->cond.wait(locker);   // 条件变量调用 wait，阻塞在此，被唤醒后继续执行 while
                    }
                }).detach();    // 设置线程分离，不需要父线程进行释放
            }
    }

    // 无参构造函数，默认实现
    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true; // 置为 true，表示关闭线程池
            }
            pool_->cond.notify_all();   // 把所有当前正在休眠的进程都唤醒，isClosed 为 true 则退出线程池
        }
    }

    template<class F>       // 模板，F--task 任务的类型
    // 向队列中添加任务
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));    // 把任务添加到池子的任务队列中（生产）
        }
        pool_->cond.notify_one();   // 条件变量唤醒一个睡眠的线程（消费
    }

private:
    // 定义结构体，池子
    struct Pool {
        std::mutex mtx;                         // 互斥锁
        std::condition_variable cond;           // 条件变量
        bool isClosed;                          // 是否关闭
        std::queue<std::function<void()>> tasks;// 队列（保存任务）
    };
    std::shared_ptr<Pool> pool_;                // 池子
};


#endif //THREADPOOL_H