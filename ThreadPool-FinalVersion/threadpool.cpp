#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 60;

// ---- 线程池方法实现
// 线程池构造
ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	idleThreadSize_(0),
	curThreadSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	threadMaxThreshHold_(THREAD_MAX_THRESHHOLD),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunning_(false)
{
}

// 线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;

	// 等待线程池里面所有的线程返回 有两种状态：阻塞 & 正在执行任务中
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]() ->bool {return threads_.size() == 0; });
}

// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshHold;
}

// 设置线程池cached模式下线程数量上限阈值
void ThreadPool::setThreadMaxThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
		threadMaxThreshHold_ = threshHold;
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	// 设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程的个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		// 创建thread线程对象的时候 把线程函数给到thread对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int id = ptr->getId();
		threads_.emplace(id, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
	}
	// 启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start(); // 需要执行一个线程函数
		idleThreadSize_++; // 记录初始空闲线程的数量
	}
}

// 定义线程函数 线程池中的所有线程从任务队列中消费任务
void ThreadPool::threadFunc(int threadId) // 线程函数结束返回 线程也会结束
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	// 必须等待所有任务执行完成 线程池才可以回收线程资源
	//while (isPoolRunning_)
	for (;;)
	{
		//std::shared_ptr<Task> task;
		Task task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;

			// cached模式下 有可能已经创建了很多线程 但是空闲时间超过60s
			// 应该把多余的线程回收掉（超过initThreadSize_的需要回收）
			// 当前时间 - 上一次线程执行的时间 >60s
			// 锁+双重判断
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					// 开始回收线程
					threads_.erase(threadId);
					std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
					exitCond_.notify_all();
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// 轮询 每1秒钟返回一次 判断是超时返回还是有任务待执行返回
					// 条件变量超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto curTime = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(curTime - lastTime);
						if (dur.count() > THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							// 开始回收线程
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
							return; // 线程函数结束 线程结束
						}
					}
				}
				else
				{
					// 等待notEmpty_条件
					notEmpty_.wait(lock);
				}
			}

			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;

			idleThreadSize_--;

			// 从任务队列中取一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// 如果还有剩余任务 通知其他线程取出并执行任务
			if (!taskQue_.empty())
			{
				notEmpty_.notify_all();
			}
			// 取出一个任务 需要进行通知 通知可以继续生产任务
			notFull_.notify_all();
		} // 取完任务就应该把锁释放掉
		// 当前线程负责执行这个任务
		if (task != nullptr)
		{
			task(); // 执行function<void()> 注意这里取出来的是函数对象 不是智能指针
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
}


// 检查线程池的运行状态
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}



// ------线程方法实现

int Thread::generateId_ = 0;
// 线程构造
Thread::Thread(ThreadFunc func)
	:func_(func),
	threadId_(generateId_++)
{

}
// 线程析构
Thread::~Thread()
{

}
// 启动线程
void Thread::start()
{
	// 创建一个线程 执行一个线程函数
	std::thread t(func_, threadId_); // c++11中 线程对象t 和线程函数func_ 出作用域析构
	// 设置为分离线程 启动一个线程执行func_ t析构后不影响线程函数的执行
	t.detach(); // pthread_detach
}

// 获取线程id
int Thread::getId() const
{
	return threadId_;
}
