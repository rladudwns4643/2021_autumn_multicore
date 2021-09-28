#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

const int MAX_THREAD = 16;
volatile int sum[MAX_THREAD];
constexpr int CNT = 50'000'000;
mutex mylock;

void worker(int thread_cnt, const int thread_id) {
	const int loop_cnt = CNT / thread_cnt;
	for (auto i = 0; i < loop_cnt; ++i) {
		sum[thread_id] += 2;
	}
}

int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		int total = 0;
		for (auto& s : sum) {
			s = 0;
		}
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i, j);
		}
		for (auto& th : workers) {
			th.join();
		}
		for (auto& s : sum) {
			total += s;
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "thread cnt: " << i << " ";
		cout << "exec_time: " << duration_cast<milliseconds>(du_t).count();
		cout << " sum : " << total << endl;
	}
}

/*
release-x64-unlock //vs의 최적화로 다코어를 활용하지 않음
1 thread: 3ms
2 thread: 0ms
3 thread: 1ms
4 thread: 3ms

with volatile
1 thread: 15ms
2 thread: 313ms	//wrong
3 thread: 428ms	//wrong
4 thread: 435ms	//wrong
*/

/*
release-x64-lock
1 thread: 570ms
2 thread: 703ms
3 thread: 861ms
4 thread: 1249ms

with volatile
1 thread: 551ms
2 thread: 715ms
3 thread: 863ms
4 thread: 1286ms
*/

/*
release-x64-atomic<int>;
1 thread: 611ms
2 thread: 773ms
3 thread: 893ms
4 thread: 1324ms
*/