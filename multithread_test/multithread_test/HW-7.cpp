/*
release_x64_non_blocking_stnchronization
1: 1687
2: 939
4: 518
8: 336
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;

constexpr long long LSB_MASK = 0x7FFFFFFFFFFFFFFE;
constexpr long long MAX_INT = 0x7FFFFFFFFFFFFFFF;
constexpr long long MIN_INT = 0x8000000000000000;

class null_mutex {
public:
	void lock() {};
	void unlock() {};
};

class LFNODE {
public:
	int key;
	long long next; //next + marked

	LFNODE() {};
	LFNODE(int key_val) : key(key_val) {};
	~LFNODE() {};

	void set_next(LFNODE* p, bool marked) {
		next = reinterpret_cast<long long>(p);
		//next += marked;
		if (marked == true) {
			next++;
		}
	}

	LFNODE* get_next() {
		long long v = next;
		return reinterpret_cast<LFNODE*>(v & LSB_MASK);
	}

	LFNODE* get_next(bool* removed) {
		long long v = next;
		*removed = 1 == (v & 0x1);
		return reinterpret_cast<LFNODE*>(v & LSB_MASK);
	}

	bool CAS_NEXT(LFNODE* old_p, LFNODE* new_p, bool old_marked, bool new_marked) {
		long long old_v = reinterpret_cast<long long>(old_p);
		if (old_marked) {
			old_v++;
		}
		long long new_v = reinterpret_cast<long long>(new_p);
		if (new_marked) {
			new_v++;
		}
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int64_t*>(&next), &old_v, new_v);
	}
};

class LF_SET {
	LFNODE head, tail;
public:
	LF_SET() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.set_next(&tail, false);
	}
	~LF_SET() { /*Init();*/ };

	void Init() {
		while (head.get_next() != &tail) {
			LFNODE* ptr = head.get_next();
			head.set_next(ptr->get_next(), false);
			delete ptr;
		}
	}

	void Find(int find_v, LFNODE*& pred, LFNODE*& cur) {
		while (true) {
		retry:
			pred = &head;
			cur = pred->get_next();
			while (true) {
				bool removed{ false };
				LFNODE* succ = cur->get_next(&removed);
				while (removed == true) {
					bool ret = pred->CAS_NEXT(cur, succ, false, false);
					if (!ret) {
						goto retry;
					}
					cur = succ;
					succ = cur->get_next(&removed);
				}
				if (cur->key >= find_v) return;
				pred = cur;
				cur = cur->get_next();
			}
		}
	}

	bool Add(int key) {
		LFNODE* pred, * cur;
		while (true) {
			Find(key, pred, cur);
			if (cur->key == key) return false;
			else {
				LFNODE* new_node = new LFNODE(key);
				new_node->set_next(cur, false);
				if (true == pred->CAS_NEXT(cur, new_node, false, false)) {
					return true;
				}
			}
		}
	}

	bool Remove(int key) {
		LFNODE* pred, * cur;
		while (true) {
			Find(key, pred, cur);
			if (cur->key != key) return false;
			else {
				LFNODE* succ = cur->get_next();
				if (!pred->CAS_NEXT(cur, succ, false, false)) continue;
				return true;
			}
		}
	}

	bool Contains(int key) {
		bool removed{ false };
		LFNODE* pred, * cur;
		Find(key, pred, cur);
		cur->set_next(cur->get_next(&removed), removed);
		return cur->key == key && !removed;
	}

	void Verify() {
		LFNODE* p = head.get_next();
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->key << ", ";
			p = p->get_next();
		}
		cout << endl;
	}
};

LF_SET myset;
void Benchmark(int num_threads) {
	const int NUM_TEST = 4'000'000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 3) {
		case 0: myset.Add(x);
			break;
		case 1: myset.Remove(x);
			break;
		case 2: myset.Contains(x);
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