#include "threadpool.h"
#include <chrono>
#include <iostream>
using namespace std;

class MyTask : public Task
{
public:
	void run()
	{
		std::cout << "tid: " << std::this_thread::get_id() << "begin" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;

	}
};

int main()
{
	ThreadPool pool;
	pool.start();

	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	//std::this_thread::sleep_for(std::chrono::seconds(5));
	getchar();
	return 0;
}
