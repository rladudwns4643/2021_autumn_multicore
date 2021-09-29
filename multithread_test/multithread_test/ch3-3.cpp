#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

constexpr int CNT = 50'000'000;
const int MAX_THREAD = 2;
volatile int* bound;
volatile bool done = false;
int error_cnt = 0;

void Thread1() {
	for (int i = 0; i < CNT / 2; ++i) {
		*bound = -(1 + *bound);
		done = true;
	}
}

void Thread2() {
	while (done == false) {
		int v = *bound;
		if ((v != 0) && (v != -1)) error_cnt++;
	}
}

int main() {
	bound = new int(0);
	vector<thread> workers;
	workers.emplace_back(Thread1);
	workers.emplace_back(Thread2);
	for (auto& th : workers) {
		th.join();
	}
	cout << "Tot: " << error_cnt << endl;
	delete bound;
}

//ÇÏ´ÂÁß