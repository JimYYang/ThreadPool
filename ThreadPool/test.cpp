#include "threadpool.h"
#include <chrono>
#include <iostream>
using namespace std;
using ULL = unsigned long long;

class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin),
		end_(end)
	{}
	Any run()
	{
		std::cout << "tid: " << std::this_thread::get_id() << "begin" << std::endl;
		//std::this_thread::sleep_for(std::chrono::seconds(5));
		ULL sum = 0;
		for (ULL i = begin_; i <= end_; i++)
		{
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};

int main()
{
	{
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		ULL sum1 = res1.get().cast_<ULL>();
		ULL sum2 = res2.get().cast_<ULL>();
		ULL sum3 = res3.get().cast_<ULL>();

		// ���ֽ����� Ȼ������̷߳�������
		// �ȴ����߳�ִ��������� ���ؽ��
		// ���̺߳ϲ����

		cout << "---------" << (sum1 + sum2 + sum3) << endl;

	}
	getchar();

	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//std::this_thread::sleep_for(std::chrono::seconds(5));
	//getchar();
	return 0;
}
