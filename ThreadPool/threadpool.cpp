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
	threadMaxThreshHold_(200),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunning_(false)
{
}

// 线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	notEmpty_.notify_all();

	// 等待线程池里面所有的线程返回 有两种状态：阻塞 & 正在执行任务中
	std::unique_lock<std::mutex> lock(taskQueMtx_);
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
// 给线程池提交任务 用户调用该接口 传入任务对象 生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 线程通信 等待任务队列未被填满任务（设置最大等待时间 若超出最大时间 暂时不能向队列中放任务）
	//while (taskQue_.size() == taskQueMaxThreshHold_)
	//{
	//	notFull_.wait(lock); // 不仅需要等待该条件变量成功 还需要获取taskQueMtx_这把锁才能真正进入运行
	//}
	// 用户提交任务 阻塞时间最长不能超过1s 否则判断提交任务失败 返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
		return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
		}))
	{
		// 说明notFull_等待1s 条件依然没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		// return task->getResult(); 如果Result依赖task 这样设计是不行的
		// 线程执行完后task后 task就被析构了
		return Result(sp, false);
	}

	// 如果有空余 把任务放到队列中
	taskQue_.emplace(sp);
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
	return Result(sp);
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
	while (isPoolRunning_)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			
			// cached模式下 有可能已经创建了很多线程 但是空闲时间超过60s
			// 应该把多余的线程回收掉（超过initThreadSize_的需要回收）
			// 当前时间 - 上一次线程执行的时间 >60s
			// 锁+双重判断
			while (isPoolRunning_ && taskQue_.size() == 0)
			{
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
							return;
						}
					}
				}
				else
				{
					// 等待notEmpty_条件
					notEmpty_.wait(lock);
				}
				// 线程池要结束 回收线程资源
				//if (!isPoolRunning_)
				//{
				//	// 开始回收线程
				//	threads_.erase(threadId);
				//	std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
				//	exitCond_.notify_all();
				//	return; // 结束线程函数 就是结束当前线程
				//}
			}

			if (!isPoolRunning_)
			{
				break;
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
			//task->run(); // 执行任务；把任务的返回值setVal方法给到Result
			task->exec();
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
	// 跳出循环后 说明线程结束
	// 开始回收线程
	threads_.erase(threadId);
	std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
	exitCond_.notify_all();
}


// 检查线程池的运行状态
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


// ------Task方法实现

Task::Task()
	:result_(nullptr)
{
	
}

void Task::exec()
{
	if (result_ != nullptr)
		result_->setVal(run()); // 这里发生多态调用
}

void Task::setResult(Result* res)
{
	result_ = res;
}

// ------线程方法实现

int Thread::generateId_ = 0;
// 线程构造
Thread::Thread(ThreadFunc func)
	:func_(func),
	threadId_(generateId_++)
{

}
// 启动线程
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

// ------Result方法实现
Result::Result(std::shared_ptr<Task> task, bool isValid/*定义和声明一出给默认值即可*/)
	:isValid_(isValid),
	task_(task)
{
	task_->setResult(this);
}

void Result::setVal(Any any)
{
	this->any_ = std::move(any);
	sem_.post(); // 已经获取了信号量的返回值 增加信号量资源
}

Any Result::get()
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task任务如果没有执行完 这里会阻塞用户线程
	return std::move(any_);
}
