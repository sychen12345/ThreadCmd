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
                    dispatch_cond_.wait(lock, [this]
                                        { return !dispatch_queue_.empty(); });
                    auto task = dispatch_queue_.front();
                    dispatch_queue_.pop();
                    lock.unlock();

                    // 执行任务
                    process_command(task);
                }
            });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cond_.notify_all();
    for (auto &thread : threads_)
    {
        thread.join();
    }

    dispatch_cond_.notify_all();
    for (auto &thread : dispatch_threads_)
    {
        thread.join();
    }
}

void ThreadPool::process_command(const Command &command)
{
    std::cout << "Processing command: " << command.name << " with seq " << command.seq << "   " << std::this_thread::get_id()
              << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 将完成的命令加入到已完成队列中
    std::unique_lock<std::mutex> done_lock(done_mutex_);
    done_queue_.push(command);
    done_counts_[command.batch_id]++;
    std::cout << "done queue size = " << done_queue_.size() << "  " << done_counts_[command.batch_id] << "/"
              << task_counts_[command.batch_id] << "  批次是   " << command.batch_id << std::endl;

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
void ThreadPool::submit_tasks(const std::vector<Command> &commands, Callback callback)
{
    // 记录批次 ID 和命令数

    int batch_id = ++batch_id_;
    task_counts_[batch_id] = (int)commands.size();

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
auto callback = [](const std::vector<Command> &commands, const std::string &result)
{
    std::cout << "function "
              << " " << commands[0].batch_id << ": " << result << "   " << std::this_thread::get_id() << std::endl;
};
void execute_commands()
{
    ThreadPool pool(10);

    std::vector<Command> commands1{{1, 1, "cmd1"}, {1, 2, "cmd2"}, {1, 3, "cmd3"}, {1, 4, "cmd4"}, {1, 5, "cmd5"}};
    // 提交第二批命令并设置回调函数
    std::vector<Command> commands2{{2, 6, "cmd6"}, {2, 7, "cmd7"}};

    pool.submit_tasks(commands1, std::bind(callback, std::placeholders::_1, std::placeholders::_2));

    // 提交第二批命令并设置回调函数

    pool.submit_tasks(commands2, std::bind(callback, std::placeholders::_1, std::placeholders::_2));
}
int main()
{
    std::thread t1(execute_commands);
    t1.join();
    while (1)
    { /* code */
    }

    return 0;
}
