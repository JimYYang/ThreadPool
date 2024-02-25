#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;

// ---- 线程池方法实现
// 线程池构造
ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	poolMode_(PoolMode::MODE_FIXED)
{
}

// 线程池析构
ThreadPool::~ThreadPool()
{

}

// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}

// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
	taskQueMaxThreshHold_ = threshHold;
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
	// 必须拿到Result之后 才能析构task
	return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	// 记录初始线程的个数
	initThreadSize_ = initThreadSize;
	
	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		// 创建thread线程对象的时候 把线程函数给到thread对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}
	// 启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start(); // 需要执行一个线程函数
	}
}

// 定义线程函数 线程池中的所有线程从任务队列中消费任务
void ThreadPool::threadFunc()
{
	//std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
	////std::cout << std::this_thread::get_id() << std::endl;
	//std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;

			// 等待netEmpty_条件
			notEmpty_.wait(lock, [&]() ->bool {return taskQue_.size() > 0; });

			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;

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

	}
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
// 线程构造
Thread::Thread(ThreadFunc func)
	:func_(func)
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
	std::thread t(func_); // c++11中 线程对象t 和线程函数func_ 出作用域析构
	// 设置为分离线程 启动一个线程执行func_ t析构后不影响线程函数的执行
	t.detach(); // pthread_detach
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
