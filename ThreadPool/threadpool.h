//#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// 任务抽象基类
class Task
{
public:
	// 用户可以自定义任意任务类型 从Task继承 重写run方法 实现自定义任务处理
	virtual void run() = 0;
};

// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED, // 固定数量的线程
	MODE_CACHED, // 线程数量可以动态增长
};

class Thread
{
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void()>;
	// 线程构造
	Thread(ThreadFunc func);
	// 启动线程
	~Thread();
	void start();
private:
	ThreadFunc func_;
};

/* example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task
{
void run(){// 任务代码}
}

pool.submitTask(std::make_shared<MyTask>());
*/


class ThreadPool
{
public:
	// 线程池构造
	ThreadPool();

	// 线程池析构
	~ThreadPool();

	// 设置线程池的工作模式
	void setMode(PoolMode mode);

	// 设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshHold);

	// 给线程池提交任务
	void submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc();
private:
	std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	int initThreadSize_; // 初始线程数量
	// 用户可能传临时对象进来作为任务
	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_; // 任务队列上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	PoolMode poolMode_; // 当前线程池的工作模式
};

#endif
