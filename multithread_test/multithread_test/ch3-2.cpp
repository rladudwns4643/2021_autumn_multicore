#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

constexpr int CNT = 50'000'000;
volatile int x, y;
const int MAX_THREAD = 2;
int trace_x[CNT], trace_y[CNT];

void Thread1() {
	for (int i = 0; i < CNT; ++i) {
		x = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_y[i] = y;
	}
}

void Thread2() {
	for (int i = 0; i < CNT; ++i) {
		y = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_x[i] = x;
	}
}

int main() {
	vector<thread> workers;
	workers.emplace_back(Thread1);
	workers.emplace_back(Thread2);
	for (auto& th : workers) {
		th.join();
	}

	int count = 0;
	for (int i = 0; i < CNT; ++i) {
		if (trace_x[i] != trace_x[i + 1]) continue;
		if (trace_y[trace_x[i]] != trace_y[trace_x[i] + 1]) continue;
		if (trace_y[trace_x[i]] != i) continue;
		count++;
	}
	cout << "Tot: " << count << endl;
}