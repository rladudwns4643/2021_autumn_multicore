//5600x, 6core, 4.4ghz

#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

constexpr int CNT = 10'000'000;
const int MAX_THREAD = 8;
volatile int sum;
mutex mylock;
volatile int flag{ 0 };

bool CAS(atomic_int* addr, int expected, int new_val) {
	return atomic_compare_exchange_strong(addr, &expected, new_val);
}
bool CAS(volatile int* addr, int expected, int new_val) {
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr), &expected, new_val);
}

void CAS_LOCK() {
	while (!CAS(&flag, 0, 1));
}

void CAS_UNLOCK() {
	while (!CAS(&flag, 1, 0));
}

void worker(int thread_cnt) {
	//cout << "thread id: " << this_thread::get_id() << endl;

	const int loop_cnt = CNT / thread_cnt;
	for (auto i = 0; i < loop_cnt; ++i) {
		CAS_LOCK();
		sum += 2;
		CAS_UNLOCK();
	}
}

int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		sum = 0;
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i);
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


//x64_release_with_mutex
/*
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
using namespace std;
using namespace chrono;
constexpr int CNT = 10'000'000;
int sum;
mutex mylock;

constexpr int MAX_THREAD = 8;

void worker(int thread_cnt) {
	int t{ CNT / thread_cnt };
	for (auto i = 0; i < t; ++i) {
		mylock.lock();
		sum = sum + 2;
		mylock.unlock();
	}
}

int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		sum = 0;
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i);
		}
		for (auto& th : workers) {
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "thread cnt: " << i << endl;
		cout << "exec_time: " << duration_cast<milliseconds>(du_t).count() << endl;
		cout << "sum = " << sum << endl;
		cout << "--------------" << endl;
	}
}
*/

//x64_release_no_mutex
/*
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
using namespace std;
using namespace chrono;
constexpr int CNT = 10'000'000;
int sum;
mutex mylock;

constexpr int MAX_THREAD = 8;

void worker(int thread_cnt) {
	int t{ CNT / thread_cnt };
	for (auto i = 0; i < t; ++i) {
		sum = sum + 2;
	}
}

int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		sum = 0;
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i);
		}
		for (auto& th : workers) {
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "thread cnt: " << i << endl;
		cout << "exec_time: " << duration_cast<milliseconds>(du_t).count() << endl;
		cout << "sum = " << sum << endl;
		cout << "--------------" << endl;
	}
}
*/

//x64_release_bakery
/*
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
using namespace std;
using namespace chrono;
constexpr int CNT = 10'000'000;
constexpr int MAX_THREAD = 8;

atomic_int sum;
mutex sum_lock;

atomic_bool flag[MAX_THREAD];
atomic_int label[MAX_THREAD];

int Max(int thread_num) {
	int maximum = label[0];
	for (int i = 0; i < thread_num; ++i) {
		if (maximum < label[i]) {
			maximum = label[i];
		}
	}

	return maximum;
}

int Compare(int i, int myId) {
	if (label[i] < label[myId]) return 1;
	else if (label[i] > label[myId]) return 0;
	else {
		if (i < myId) return 1;
		else if (i > myId) return 0;
		else return 0;
	}
}

void bakery_lock(int thread_id, int thread_num) {
	//flag true -> 번호받음 -> flag false
	flag[thread_id] = true;
	label[thread_id] = Max(thread_num) + 1;
	flag[thread_id] = false;
	//여기서 최적화를 통해 밑의 코드가 먼저 실행된다면?
	for (int i = 0; i < thread_num; ++i) {
		while (flag[i] == true);
		while ((label[i] != 0) && Compare(i, thread_id));
	}
}

void bakery_unlock(int myId) {
	label[myId] = 0;
}

void Process(int thread_id, int num_thread) {
	for (int i = 0; i < CNT / num_thread; ++i) {
		bakery_lock(thread_id,num_thread);
		sum += 1;
		bakery_unlock(thread_id);
	}
}

int main(void) {
	for (int thread_cnt = 1; thread_cnt <= MAX_THREAD; thread_cnt *= 2) {
		vector<thread> threads;

		// 변수 초기화
		for (auto& a : label) a = 0;
		for (auto& a : flag) a = false;
		sum = 0;

		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < thread_cnt; ++i) threads.emplace_back(Process, i, thread_cnt);
		for (auto& t : threads)	t.join();
		auto end_t = high_resolution_clock::now();
		auto exec_time = end_t - start_t;

		cout << "thread_cnt = " << thread_cnt << endl;
		cout << "time = " << duration_cast<milliseconds>(exec_time).count() << "ms\n";
		cout << "sum = " << sum << endl;
		cout << "----------" << endl;
	}
}
*/