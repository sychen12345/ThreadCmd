//
// Created by chensiyuan on 2023/5/12.
//

#ifndef YXSOUTHMODULE_THREADPOOL_H
#define YXSOUTHMODULE_THREADPOOL_H
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>

// 定义命令结构体
struct Command
{
    int batch_id; // 批次 ID
    int seq;      // 序列号
    std::string name;
};
using Callback = std::function<void(const std::vector<Command> &, const std::string &)>;
class UartCmd
{
};
class UartDevice
{
    virtual void addRwCommand(const std::vector<std::shared_ptr<UartCmd>> &uartCmd)
    {
        for (const auto &command : uartCmd)
        {
            Command cmd{};
            std::unique_lock<std::mutex> lock;
        }
    }
};

class ThreadPool
{
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    void submit_tasks(const std::vector<Command> &commands, Callback callback);

private:
    void process_command(const Command &command);
    std::vector<std::thread> threads_;
    std::vector<std::thread> dispatch_threads_; // 用于分发任务的线程

    std::queue<Command> task_queue_;
    std::queue<Command> dispatch_queue_; // 用于分发任务的队列
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_ = false;

    std::map<int, int> task_counts_; // 批次 ID 对应的命令数量
    std::queue<Command> done_queue_; // 已完成的命令队列
    std::mutex done_mutex_;          // 已完成的命令队列的互斥锁

    std::map<int, Callback> callbacks_; // 批次 ID 对应的回调函数

    int batch_id_ = 0; // 批次 ID 计数器

    std::mutex dispatch_mutex_;             // 分发任务队列的互斥锁
    std::condition_variable dispatch_cond_; // 分发任务条件变量

    std::mutex batch_id_mutex_; // batch_id_ 的互斥锁

    std::map<int, int> done_counts_;
};

#endif // YXSOUTHMODULE_THREADPOOL_H
