#include <tbb/task.h>
#include <tbb/task_scheduler_observer.h>

#include <string>
#include <iostream>
#include <chrono>

using namespace std;
using namespace chrono;
using namespace tbb;

long fib(long n) { if (n < 2) return n; else return fib(n - 1) + fib(n - 2); }
//use tbb
class fib_task : public task {
public:
	const long n;
	long* const sum;
	fib_task(long n_, long* sum_) :n(n_), sum(sum_) {}
	task* execute() { //overrides virtual function task::execute
		if (n < CutOff) { *sum = fib(n); } //task���� ������� ������ ���� �� ���ϸ� �̱۷� ���
		else {
			long x, y;
			fib_task& a = *new(allocate_child()) fib_task(n - 1, &x);
			fib_task& b = *new(allocate_child()) fib_task(n - 2, &y);
			set_ref_count(3); //
			spawn(b);
			spawn_and_wait_for_all(a);
			*sum = x + y;
		}
		return NULL;
	}
};

long fib(long n) {
	long sum;
	//���ఴü
	fib_task& a = *new(task::allocate_root()) fib_task(n, &sum);
	//���ఴü ����
	task::spawn_root_and_wait(a);
	return sum;
}

