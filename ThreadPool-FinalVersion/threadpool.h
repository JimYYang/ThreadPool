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

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED, // �̶��������߳�
	MODE_CACHED, // �߳��������Զ�̬����
};


class Thread
{
public:
	// �̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	// �̹߳���
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();

	// �����߳�
	void start();

	// ��ȡ�߳�id
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // �����߳�id ���̻߳��յ�ʱ��ʹ��
};

/* example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task
{
void run(){// �������}
}

pool.submitTask(std::make_shared<MyTask>());
*/


class ThreadPool
{
public:
	// �̳߳ع���
	ThreadPool();

	// �̳߳�����
	~ThreadPool();

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode);

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshHold);

	// �����̳߳�cachedģʽ���߳�����������ֵ
	void setThreadMaxThreshHold(int threshHold);

	// ���̳߳��ύ���� �û����øýӿ� ����������� ��������
	// ʹ�ÿɱ��ģ���� ʹ��submitTask���Խ������������������������Ĳ���
	// ����ֵfuture<>
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// ������� �ŵ����������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();
		// ��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		// �û��ύ���� ����ʱ������ܳ���1s �����ж��ύ����ʧ�� ����
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
			return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
		{
			// ˵��notFull_�ȴ�1s ������Ȼû������
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]() -> RType {return RType(); }
			);
			(*task)();
			return task->get_future();
		}

		// ����п��� ������ŵ�������
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)(); });
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
		return result;

	}

	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc(int threadId);

	// ����̳߳ص�����״̬
	bool checkRunningState()const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	int initThreadSize_; // ��ʼ�߳�����
	std::atomic_int curThreadSize_; // ��¼��ǰ�̳߳��߳�������
	int threadMaxThreshHold_; // �߳�����������ֵ
	std::atomic_int idleThreadSize_; // ��¼�����̵߳�����

	// �û����ܴ���ʱ���������Ϊ���� ������Ҫʹ��ָ��ָ��
	// Task���� ���Ǻ�������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // �������
	std::atomic_int taskSize_; // ��������
	int taskQueMaxThreshHold_; // �������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	std::condition_variable exitCond_; // �ȴ��߳���Դȫ������

	PoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_; // ��ʾ�̳߳ص�ǰ������״̬
};

#endif
