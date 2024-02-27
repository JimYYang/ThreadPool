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
#include <unordered_map>
#include <iostream>
#include <thread>
#include <future>

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
	using ThreadFunc = std::function<void(int)>;

	// 线程构造
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();

	// 启动线程
	void start();

	// 获取线程id
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程id 在线程回收的时候使用
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

	// 设置线程池cached模式下线程数量上限阈值
	void setThreadMaxThreshHold(int threshHold);

	// 给线程池提交任务 用户调用该接口 传入任务对象 生产任务
	// 使用可变参模板变参 使得submitTask可以接收任意任务函数和任意数量的参数
	// 返回值future<>
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// 打包任务 放到任务队列中
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();
		// 获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		// 用户提交任务 阻塞时间最长不能超过1s 否则判断提交任务失败 返回
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
			return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
		{
			// 说明notFull_等待1s 条件依然没有满足
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]() -> RType {return RType(); }
			);
			(*task)();
			return task->get_future();
		}

		// 如果有空余 把任务放到队列中
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;

		// 因为放了新任务 队列肯定不空 在notEmpty_上通知 分配线程执行任务
		notEmpty_.notify_all();

		// cached模式 任务处理比较紧急 场景：小而快的任务
		// 需要根据任务数量和空闲线程数量 判断是否需要创建新的线程出来
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadMaxThreshHold_)
		{
			std::cout << "create new thread:" << std::this_thread::get_id() << std::endl;
			// 创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// 启动线程
			threads_[threadId]->start();
			// 修改线程个数相关的变量
			curThreadSize_++;
			idleThreadSize_++;
		}
		// 必须拿到Result之后 才能析构task
		return result;

	}

	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc(int threadId);

	// 检查线程池的运行状态
	bool checkRunningState()const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	int initThreadSize_; // 初始线程数量
	std::atomic_int curThreadSize_; // 记录当前线程池线程总数量
	int threadMaxThreshHold_; // 线程数量上限阈值
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量

	// 用户可能传临时对象进来作为任务 所以需要使用指针指针
	// Task任务 就是函数对象
	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_; // 任务队列上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 等待线程资源全部回收

	PoolMode poolMode_; // 当前线程池的工作模式
	std::atomic_bool isPoolRunning_; // 表示线程池当前的启动状态
};

#endif

