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
	threadMaxThreshHold_(THREAD_MAX_THRESHHOLD),
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
		//std::shared_ptr<Task> task;
		Task task;
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
			task(); // ִ��function<void()> ע������ȡ�������Ǻ������� ��������ָ��
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



// ------�̷߳���ʵ��

int Thread::generateId_ = 0;
// �̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func),
	threadId_(generateId_++)
{

}
// �߳�����
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