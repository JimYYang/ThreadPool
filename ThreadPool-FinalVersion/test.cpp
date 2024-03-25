/*
* 1.ʹ�̳߳��ύ���������
*	submitTask(fun1, 1, 2);
*	submitTask(fun2, 1, 2, 3);
*	submitTask�ɱ��ģ��
* 2.ʹ��c++������ packaged_task future decltype�����̳߳ش���
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
 