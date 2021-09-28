#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

const int MAX_THREAD = 16;
volatile int sum[MAX_THREAD * 16];

constexpr int CNT = 50'000'000;
mutex mylock;


void worker(int thread_cnt, const int thread_id) {
	//cout << "thread id: " << this_thread::get_id() << endl;

	const int loop_cnt = CNT / thread_cnt;
	for (auto i = 0; i < loop_cnt; ++i) {
		sum[thread_id * 16] += 2;
	}
}

int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		int total_sum = 0;
		for (auto& v : sum) {
			v = 0;
		}
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i, j);
		}
		for (auto& th : workers) {
			th.join();
		}
		for (auto v : sum) total_sum += v;
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "thread cnt: " << i << endl;
		cout << "exec_time: " << duration_cast<milliseconds>(du_t).count();
		cout << "sum = " << sum << endl;
	}
}