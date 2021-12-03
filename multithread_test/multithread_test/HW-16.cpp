/*
* HW-16
release_x64_coarse_grained_synchronization_SkipList
1: 481
2: 519
4: 600
8: 774
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;
constexpr int NUM_LV = 10;

class null_mutex {
	void lock() {};
	void unlock() {};
};

class SKNODE {
public:
	int value;
	SKNODE* next[NUM_LV];
	int top_lv; // ~ NUM_LEVEL - 1
	SKNODE() : value(0), top_lv(0) {
		for (auto& n : next) n = nullptr;
	}
	SKNODE(int x, int top) : value(x), top_lv(top) {
		for (auto& n : next) n = nullptr;
	}
	~SKNODE() {}
};

class CSK_SET {
	SKNODE head, tail;
	null_mutex c_lock;
public:
	CSK_SET() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	~CSK_SET() {
		Init();
	}
	void Init() {
		while (head.next[0] != &tail) {
			SKNODE* p = head.next[0];
			head.next[0] = p->next[0];
			delete p;
		}
		for (auto& n : head.next) {
			n = &tail;
		}
	}

	void Find(int x, SKNODE* pred[], SKNODE* curr[]) {
		int cur_lv = NUM_LV - 1;
		pred[cur_lv] = &head;
		while (true) {
			curr[cur_lv] = pred[cur_lv]->next[cur_lv];
			while (curr[cur_lv]->value < x) {
				pred[cur_lv] = curr[cur_lv];
				curr[cur_lv] = curr[cur_lv]->next[cur_lv];
			}
			if (cur_lv == 0) return;
			cur_lv--;
			pred[cur_lv] = pred[cur_lv + 1];
		}
	}

	bool Add(int x) {
		SKNODE* pred[NUM_LV], * curr[NUM_LV];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value == x) {
			c_lock.unlock();
			return false;
		}
		else {
			int new_level = 0;
			for (int i = 0; i < NUM_LV; ++i) {
				new_level = i;
				if (rand() % 2 == 0) break;
			}
			SKNODE* new_node = new SKNODE(x, new_level);
			for (int i = 0; i <= new_level; ++i) {
				new_node->next[i] = curr[i];
				pred[i]->next[i] = new_node;
			}
			c_lock.unlock();
			return true;
		}
	}

	bool Remove(int x) {
		SKNODE* pred[NUM_LV], * curr[NUM_LV];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			int top_lv = curr[0]->top_lv;
			for (int i = 0; i <= top_lv; ++i) {
				pred[i]->next[i] = curr[i]->next[i];
			}
			delete curr[0];
			c_lock.unlock();
			return true;
		}
	}

	bool Contains(int x) {
		SKNODE* pred[NUM_LV], * curr[NUM_LV];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			c_lock.unlock();
			return true;
		}
	}

	void Verify() {
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next[0];
		}
		cout << endl;
	}
};

CSK_SET myset;

void Benchmark(int num_threads) {
	const int NUM_TEST = 4'000'000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 3) {
		case 0: myset.Add(x); break;
		case 1: myset.Remove(x); break;
		case 2: myset.Contains(x); break;
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