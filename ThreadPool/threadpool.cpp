#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 4;

// �̳߳ع���
ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	poolMode_(PoolMode::MODE_FIXED)
{
}

// �̳߳�����
ThreadPool::~ThreadPool()
{

}

// �����̳߳صĹ���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}

// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
	taskQueMaxThreshHold_ = threshHold;
}

// ���̳߳��ύ���� �û����øýӿ� ����������� ��������
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
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
		return;
	}

	// ����п��� ������ŵ�������
	taskQue_.emplace(sp);
	taskSize_++;

	// ��Ϊ���������� ���п϶����� ��notEmpty_��֪ͨ �����߳�ִ������
	notEmpty_.notify_all();
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	// ��¼��ʼ�̵߳ĸ���
	initThreadSize_ = initThreadSize;
	
	// �����̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		// ����thread�̶߳����ʱ�� ���̺߳�������thread����
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}
	// ���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start(); // ��Ҫִ��һ���̺߳���
	}
}

// �����̺߳��� �̳߳��е������̴߳������������������
void ThreadPool::threadFunc()
{
	//std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
	////std::cout << std::this_thread::get_id() << std::endl;
	//std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;

			// �ȴ�netEmpty_����
			notEmpty_.wait(lock, [&]() ->bool {return taskQue_.size() > 0; });

			std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

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
			task->run();
		}

	}
}

// �̷߳���ʵ��
// �̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
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
	std::thread t(func_); // c++11�� �̶߳���t ���̺߳���func_ ������������
	// ����Ϊ�����߳� ����һ���߳�ִ��func_ t������Ӱ���̺߳�����ִ��
	t.detach(); // pthread_detach
}

