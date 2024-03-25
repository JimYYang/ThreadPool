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

// Any���ͣ����Խ���������������
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// ������캯��������Any�����������͵�����
	template<typename T>
	Any(T data)
		:base_(std::make_unique<Derive<T>>(data))
	{}

	// ��������ܰ�Any����洢��������ȡ����
	template<typename T>
	T cast_()
	{
		// ����ָ��ת��Ϊ������ָ�� ֻ�л���ָ��ȷʵָ������������ʱ�����ȷ
		// dynamic_cast ֧��RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			std::cout << "++++++++" << std::endl;
			throw "type is incompatible";
		}
		return pd->data_;
	}

private:
	// ��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// ����������
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data)
			:data_(data)
		{}
		T data_;
	};

	// ����һ�������ָ��
	std::unique_ptr<Base> base_;
	
};

// ʵ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// �ȴ��ź�������Դ û����Դ�Ļ���������ǰ�߳�
		cond_.wait(lock, [&]() -> bool {
			return resLimit_ > 0;
			});
		resLimit_--;
	}

	// ����һ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

// Task����ǰ������
class Task;

// ʵ�� �����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// setVal ��ȡ����ִ����ķ���ֵ
	void setVal(Any any);

	// get���� �û��������������ȡtask�ķ���ֵ
	Any get();
private:
	Any any_; // �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<Task> task_; // ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч
};

// ����������
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	// �û������Զ��������������� ��Task�̳� ��дrun���� ʵ���Զ���������
	virtual Any run() = 0;
private:
	Result* result_; // ����������ָ�� ����ύ������ Result�������ڱ�Task��

};

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

	// ���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

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
	std::queue<std::shared_ptr<Task>> taskQue_; // �������
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

//