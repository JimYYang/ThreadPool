#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 60;

// ---- �̳߳ط���ʵ��
// �̳߳ع���
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

// �̳߳�����
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;

	// �ȴ��̳߳��������е��̷߳��� ������״̬������ & ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]() ->bool {return threads_.size() == 0; });
}

// �����̳߳صĹ���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshHold;
}

// �����̳߳�cachedģʽ���߳�����������ֵ
void ThreadPool::setThreadMaxThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
		threadMaxThreshHold_ = threshHold;
}
// ���̳߳��ύ���� �û����øýӿ� ����������� ��������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// �߳�ͨ�� �ȴ��������δ�����������������ȴ�ʱ�� ���������ʱ�� ��ʱ����������з�����
	//while (taskQue_.size() == taskQueMaxThreshHold_)
	//{
	//	notFull_.wait(lock); // ������Ҫ�ȴ������������ɹ� ����Ҫ��ȡtaskQueMtx_���������������������
	//}
	// �û��ύ���� ����ʱ������ܳ���1s �����ж��ύ����ʧ�� ����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
		return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
		}))
	{
		// ˵��notFull_�ȴ�1s ������Ȼû������
		std::cerr << "task queue is full, submit task fail." << std::endl;
		// return task->getResult(); ���Result����task ��������ǲ��е�
		// �߳�ִ�����task�� task�ͱ�������
		return Result(sp, false);
	}

	// ����п��� ������ŵ�������
	taskQue_.emplace(sp);
	taskSize_++;

	// ��Ϊ���������� ���п϶����� ��notEmpty_��֪ͨ �����߳�ִ������
	notEmpty_.notify_all();

	// cachedģʽ �������ȽϽ��� ������С���������
	// ��Ҫ�������������Ϳ����߳����� �ж��Ƿ���Ҫ�����µ��̳߳���
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadMaxThreshHold_)
	{
		std::cout << "create new thread:" << std::this_thread::get_id() << std::endl;
		// �������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// �����߳�
		threads_[threadId]->start();
		// �޸��̸߳�����صı���
		curThreadSize_++;
		idleThreadSize_++;
	}
	// �����õ�Result֮�� ��������task
	return Result(sp);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	// �����̳߳ص�����״̬
	isPoolRunning_ = true;

	// ��¼��ʼ�̵߳ĸ���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	
	// �����̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		// ����thread�̶߳����ʱ�� ���̺߳�������thread����
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int id = ptr->getId();
		threads_.emplace(id, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
	}
	// ���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start(); // ��Ҫִ��һ���̺߳���
		idleThreadSize_++; // ��¼��ʼ�����̵߳�����
	}
}

// �����̺߳��� �̳߳��е������̴߳������������������
void ThreadPool::threadFunc(int threadId) // �̺߳����������� �߳�Ҳ�����
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	// ����ȴ���������ִ����� �̳߳زſ��Ի����߳���Դ
	//while (isPoolRunning_)
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
			
			// cachedģʽ�� �п����Ѿ������˺ܶ��߳� ���ǿ���ʱ�䳬��60s
			// Ӧ�ðѶ�����̻߳��յ�������initThreadSize_����Ҫ���գ�
			// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� >60s
			// ��+˫���ж�
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					// ��ʼ�����߳�
					threads_.erase(threadId);
					std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
					exitCond_.notify_all();
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// ��ѯ ÿ1���ӷ���һ�� �ж��ǳ�ʱ���ػ����������ִ�з���
					// ����������ʱ����
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto curTime = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(curTime - lastTime);
						if (dur.count() > THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							// ��ʼ�����߳�
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
							return; // �̺߳������� �߳̽���
						}
					}
				}
				else
				{
					// �ȴ�notEmpty_����
					notEmpty_.wait(lock);
				}
				// �̳߳�Ҫ���� �����߳���Դ
				//if (!isPoolRunning_)
				//{
				//	// ��ʼ�����߳�
				//	threads_.erase(threadId);
				//	std::cout << "threadId:" << std::this_thread::get_id() << " has been erased!" << std::endl;
				//	exitCond_.notify_all();
				//	return; // �����̺߳��� ���ǽ�����ǰ�߳�
				//}
			}

			std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

			idleThreadSize_--;

			// �����������ȡһ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// �������ʣ������ ֪ͨ�����߳�ȡ����ִ������
			if (!taskQue_.empty())
			{
				notEmpty_.notify_all();
			}
			// ȡ��һ������ ��Ҫ����֪ͨ ֪ͨ���Լ�����������
			notFull_.notify_all();
		} // ȡ�������Ӧ�ð����ͷŵ�
		// ��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			//task->run(); // ִ�����񣻰�����ķ���ֵsetVal��������Result
			task->exec();
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
}


// ����̳߳ص�����״̬
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


// ------Task����ʵ��

Task::Task()
	:result_(nullptr)
{
	
}

void Task::exec()
{
	if (result_ != nullptr)
		result_->setVal(run()); // ���﷢����̬����
}

void Task::setResult(Result* res)
{
	result_ = res;
}

// ------�̷߳���ʵ��

int Thread::generateId_ = 0;
// �̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func),
	threadId_(generateId_++)
{

}
// �����߳�
Thread::~Thread()
{

}
// �����߳�
void Thread::start()
{
	// ����һ���߳� ִ��һ���̺߳���
	std::thread t(func_, threadId_); // c++11�� �̶߳���t ���̺߳���func_ ������������
	// ����Ϊ�����߳� ����һ���߳�ִ��func_ t������Ӱ���̺߳�����ִ��
	t.detach(); // pthread_detach
}

// ��ȡ�߳�id
int Thread::getId() const
{
	return threadId_;
}

// ------Result����ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid/*���������һ����Ĭ��ֵ����*/)
	:isValid_(isValid),
	task_(task)
{
	task_->setResult(this);
}

void Result::setVal(Any any)
{
	this->any_ = std::move(any);
	sem_.post(); // �Ѿ���ȡ���ź����ķ���ֵ �����ź�����Դ
}

Any Result::get()
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task�������û��ִ���� ����������û��߳�
	return std::move(any_);
}

//
