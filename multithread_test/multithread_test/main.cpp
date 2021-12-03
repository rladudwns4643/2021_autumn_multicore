/*
release_x64_coarse_grained_set
1: 841
2: 858
4: 958
8: 1155
*/
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 33%, range: 1000)
1: 788
2: 816
4: 918
8: 1095
*/ 
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 60%, range: 1000)
1: 555
2: 570
4: 632
8: 780
*/
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 80%, range: 1000)
1: 331
2: 382
4: 415
8: 511
*/
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 98%, range: 2000)
1: 145
2: 137
4: 148
8: 187
*/
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 98%, range: 2000)
1: 185
2: 175
4: 178
8: 227
*/
/*
release_x64_coarse_grained_rw_lock_set (benchmark: contains() 98%, range: 4000)
1: 292
2: 276
4: 282
8: 262
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <shared_mutex>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;


class null_mutex {
public:
	void lock() {};
	void unlock() {};
};

class NODE {
public:
	int key;
	NODE* next;

	NODE() : next(nullptr) {};
	NODE(int key_val) : key(key_val), next(nullptr) {};
	~NODE() {};
};

class C_SET {
	NODE head, tail;
	shared_mutex g_lock;
public:
	C_SET() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~C_SET() {};

	void Init() {
		NODE* ptr;
		while (head.next != &tail) {
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}

	bool Add(int key) {
		NODE* pred, * cur;
		pred = &head;
		g_lock.lock();
		cur = pred->next;
		while (cur->key < key) {
			pred = cur;
			cur = cur->next;
		}
		if (cur->key == key) {
			g_lock.unlock();
			return false;
		}
		else {
			NODE* node = new NODE(key);
			node->next = cur;
			pred->next = node;
			g_lock.unlock();
			return true;
		}
	}
	bool Remove(int key) {
		NODE* pred, * cur;
		pred = &head;
		g_lock.lock();
		cur = pred->next;
		while (cur->key < key) {
			pred = cur;
			cur = cur->next;
		}
		if (cur->key != key) {
			g_lock.unlock();
			return false;
		}
		else {
			pred->next = cur->next;
			delete cur;
			g_lock.unlock();
			return true;
		}
	}
	bool Contains(int key) {
		NODE* cur;
		g_lock.lock_shared();
		cur = head.next;
		while (cur->key < key) {
			cur = cur->next;
		}
		if (cur->key != key) {
			g_lock.unlock_shared();
			return false;
		}
		else {
			g_lock.unlock_shared();
			return true;
		}
	}
	void Verify() {
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->key << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

C_SET myset;
void Benchmark(int num_threads) {
	const int NUM_TEST = 4'000'000;
	const int RANGE = 4000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 100) {
		case 0: myset.Add(x);
			break;
		case 1: myset.Remove(x);
			break;
		default: myset.Contains(x);
			break;
		}
	}
}

int main() {
	for (int i = 1; i <= THREAD_COUNT; i *= 2) {
		vector<thread> worker;
		myset.Init();

		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i);
		}
		for (auto& th : worker) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;
		//값 확인을 위해 set의 상위 20개 출력
		myset.Verify();

		cout << "THREAD CNT: " << i <<
			" exec_t: " << duration_cast<milliseconds>(exec_t).count() << "ms\n---\n";
	}
}