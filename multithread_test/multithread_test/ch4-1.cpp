/*
peterson algorithm
*/

#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

volatile int sum;
constexpr int CNT = 50'000'000;
const int MAX_THREAD = 2;
mutex mylock;

atomic<char> flag[2] = { 0,0 }; //lock 여부 확인
volatile int victim = 0; //deadlock 탈출용

void p_lock(int my_id) {
	int other = 1 - my_id;
	flag[my_id] = 1;
	victim = my_id;
	//atomic_thread_fence(std::memory_order_seq_cst);
	while ((flag[other].fetch_add(0) == 1) && (my_id == victim));
}

void p_unlock(int my_id) {
	flag[my_id] = false;
}


void worker(int thread_cnt, int my_id) {
	const int loop_cnt = CNT / thread_cnt;
	for (auto i = 0; i < loop_cnt; ++i) {
		//p_lock(my_id);
		mylock.lock();
		sum = sum + 2;
		//p_unlock(my_id);
		mylock.unlock();
	}
}

int main() {
	for (int i = 2; i <= MAX_THREAD; i *= 2) {
		sum = 0;
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i, j);
		}
		for (auto& th : workers) {
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "thread cnt: " << i;
		cout << " exec_time: " << duration_cast<milliseconds>(du_t).count();
		cout << " sum = " << sum << endl;
	}
}

/*
release_x64_peterson_atomic_thread_fence (빠름)
2 thread: 665

release_x64_atomic
2 thread: 715
*/