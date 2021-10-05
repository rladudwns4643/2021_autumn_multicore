#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

constexpr int CNT = 50'000'000;
int error_cnt = 0;
volatile int* bound;
volatile bool done = false;
mutex mylock;

void Thread1() {
	for (int i = 0; i < CNT / 2; ++i) {
		mylock.lock();
		*bound = -(1 + *bound);
		mylock.unlock();
	}
	done = true;
}

void Thread2() {
	while (done == false) {
		mylock.lock();
		int v = *bound; //v가 -65536 or 65535가 되면서 error_cnt 증가 (싱글스레드에서는 발생하지 않음)
		mylock.unlock();
		if ((v != 0) && (v != -1)) {
			cout << hex << v << ", ";
			error_cnt++;
		}
	}
}

int main() {
	int arr[32];
	long long temp = reinterpret_cast<long long>(arr + 16);
	temp = temp & 0xFFFFFFFFFFFFFFC0;
	temp = temp - 2;
	bound = reinterpret_cast<int*>(temp);
	*bound = 0;
	//bound = new int(0);
	vector<thread> workers;
	workers.emplace_back(Thread1);
	workers.emplace_back(Thread2);
	for (auto& th : workers) {
		th.join();
	}
	cout << "Tot: " << error_cnt << endl;
	//delete bound;
}