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

// ����������
class Task
{
public:
	// �û������Զ��������������� ��Task�̳� ��дrun���� ʵ���Զ���������
	virtual void run() = 0;
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
	using ThreadFunc = std::function<void()>;
	// �̹߳���
	Thread(ThreadFunc func);
	// �����߳�
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

	// ���̳߳��ύ����
	void submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc();
private:
	std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	int initThreadSize_; // ��ʼ�߳�����
	// �û����ܴ���ʱ���������Ϊ����
	std::queue<std::shared_ptr<Task>> taskQue_; // �������
	std::atomic_int taskSize_; // ��������
	int taskQueMaxThreshHold_; // �������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	PoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
};

#endif
