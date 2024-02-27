/*
* 1.使线程池提交任务更方便
*	submitTask(fun1, 1, 2);
*	submitTask(fun2, 1, 2, 3);
*	submitTask可变参模板
* 2.使用c++新特性 packaged_task future decltype来简化线程池代码
*/

#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include "threadpool.h"
using namespace std;

int sum1(int a, int b)
{
	return a + b;
}

int sum2(int a, int b, int c)
{
	return a + b + c;
}

int main()
{
	ThreadPool pool;
	pool.start(4);
	future<int> r1 = pool.submitTask(sum1, 1, 2);
	cout << r1.get() << endl;
	return 0;
}