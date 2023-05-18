//
// Created by chensiyuan on 2023/5/12.
//

#include "ThreadPool.h"
ThreadPool::ThreadPool(size_t numThreads)
{
    for (size_t i = 0; i < numThreads; ++i)
    {
        threads_.emplace_back(
            [this]
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [this]
                               { return !task_queue_.empty() || stop_; });
                    if (stop_ && task_queue_.empty())
                    {
                        std::cout << "stop thread" << std::endl;
                        return;
                    }
                    auto task = task_queue_.front();
                    task_queue_.pop();
                    lock.unlock();

                    // 分发任务给其他线程池中的线程去完成
                    std::unique_lock<std::mutex> dispatch_lock(dispatch_mutex_);
                    dispatch_queue_.push(task);
                    dispatch_cond_.notify_one();
                }
            });

        // 添加用于分发任务的线程
        dispatch_threads_.emplace_back(
            [this]
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lock(dispatch_mutex_);
                    dispatch_cond_.wait(lock,
                                        [this]
                                        { return !dispatch_queue_.empty() || stop_; });
                    if (stop_ && dispatch_queue_.empty())
                    {
                        std::cout << "stop dispatch thread" << std::endl;
                        return;
                    }
                    auto task = dispatch_queue_.front();
                    dispatch_queue_.pop();
                    lock.unlock();

                    // 执行任务 在多线程环境中
                    process_command(task);
                }
            });
    }
}
ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        std::cout << "stop thread pool" << std::endl;
        stop_ = true;
    }
    cond_.notify_all();
    for (auto &thread : threads_)
    {
        thread.detach();
    }

    dispatch_cond_.notify_all();
    for (auto &thread : dispatch_threads_)
    {
        thread.detach();
    }
}
void ThreadPool::submit_tasks(const std::vector<Command> &commands, Callback callback)
{
    // 记录批次 ID 和命令数

    int batch_id = ++batch_id_;
    task_counts_[batch_id] = commands.size();

    // 将命令提交到任务队列
    for (const auto &command : commands)
    {
        Command cmd{batch_id, command.seq, command.name};
        std::unique_lock<std::mutex> lock(mutex_);
        task_queue_.push(cmd);
        cond_.notify_one();
    }

    // 将回调函数与批次 ID 关联起来
    callbacks_[batch_id] = callback;
}
void ThreadPool::process_command(const Command &command)
{
    // 处理命令
    std::cout << "Processing command: " << command.name << " with seq " << command.seq << "   "
              << std::this_thread::get_id() << std::endl;

    // 模拟处理时间
    // 模拟处理时间
    std::chrono::milliseconds duration(rand() % 10000);
    std::this_thread::sleep_for(duration);

    // 将完成的命令加入到已完成队列中
    std::unique_lock<std::mutex> done_lock(done_mutex_);
    done_queue_.push(command);
    done_counts_[command.batch_id]++;
    std::cout << "done queue size = " << done_queue_.size() << "  "
              << done_counts_[command.batch_id] << "/" << task_counts_[command.batch_id]
              << "  批次是   " << command.batch_id << "  " << std::this_thread::get_id()
              << std::endl;

    if (done_counts_[command.batch_id] == task_counts_[command.batch_id])
    {
        // 如果该批次的所有命令都已经处理完毕，则触发回调函数
        std::vector<Command> commands;
        while (!done_queue_.empty() && done_queue_.front().batch_id == command.batch_id)
        {
            commands.push_back(done_queue_.front());
            done_queue_.pop();
        }
        auto callback = callbacks_.find(command.batch_id);
        if (callback != callbacks_.end())
        {
            callback->second(commands, "success");
            callbacks_.erase(callback);
        }
    }
}
int main()
{
    ThreadPool pool(5);

    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // 提交第一批命令并设置回调函数
    std::vector<Command> commands1{{1, 1, "cmd1"}, {1, 2, "cmd2"}, {1, 3, "cmd3"}, {1, 4, "cmd4"}, {1, 5, "cmd5"}, {1, 6, "cmd6"}, {1, 7, "cmd7"}, {1, 8, "cmd8"}, {1, 9, "cmd9"}, {1, 10, "cmd10"}, {1, 11, "cmd11"}};
    pool.submit_tasks(commands1,
                      [](const std::vector<Command> &commands, const std::string &result)
                      {
                          std::cout << "-=======================" << commands[0].batch_id << ": "
                                    << result << "   " << std::this_thread::get_id() << std::endl;
                      });

    // 提交第二批命令并设置回调函数
    std::vector<Command> commands2{{2, 6, "cmd6"}, {2, 7, "cmd7"}};
    pool.submit_tasks(commands2,
                      [](const std::vector<Command> &commands, const std::string &result)
                      {
                          std::cout << "-------------- " << commands[0].batch_id << ": " << result
                                    << "  " << std::this_thread::get_id() << std::endl;
                      });

    // 模拟主线程继续执行其他任务
    std::this_thread::sleep_for(std::chrono::seconds(20));

    return 0;
}