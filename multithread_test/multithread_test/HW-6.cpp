/*
release_x64_lazy_synchronization_with_shared_ptr
1: 7100
2: X
4: X
8: X
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;

class null_mutex {
public:
	void lock() {};
	void unlock() {};
};

class SPNODE {
public:
	int key;
	shared_ptr<SPNODE> next = nullptr;
	mutex node_lock;
	bool removed = false;

	SPNODE() : next(nullptr) {};
	SPNODE(int key_val) : key(key_val), next(nullptr) {};
	void Lock() {
		node_lock.lock();
	}
	void unLock() {
		node_lock.unlock();
	}
	~SPNODE() {};
};

class SPZ_SET {
	shared_ptr<SPNODE> head, tail;
	mutex g_lock;
public:
	SPZ_SET() {
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7FFFFFFF);
		head->next = tail;
	}
	~SPZ_SET() {
		Init();
	};

	void Init() {
		head->next = tail;
	}

	bool validate(const shared_ptr<SPNODE>& pred, const shared_ptr<SPNODE>& curr) {
		return !pred->removed
			&& !curr->removed
			&& pred->next == curr;
	}

	bool Add(int key) {
		while (true) {
			shared_ptr<SPNODE> pred, cur;
			pred = head;
			cur = pred->next;
			while (cur->key < key) {
				pred = cur;
				cur = cur->next;
			}
			pred->Lock();
			cur->Lock();
			while (validate(pred, cur)) {
				if (cur->key == key && !cur->removed) {
					pred->unLock();
					cur->unLock();
					return false;
				}
				else {
					if (cur->removed) {
						cur->removed = false;
						pred->unLock();
						cur->unLock();
						return true;
					}
					shared_ptr<SPNODE> node = make_shared<SPNODE>(key);
					node->next = cur;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = node;
					pred->unLock();
					cur->unLock();
					return true;
				}
			}
			pred->unLock();
			cur->unLock();
		}
	}
	bool Remove(int key) {
		while (true) {
			shared_ptr<SPNODE> pred, cur;
			pred = head;
			cur = pred->next;
			while (cur->key < key) {
				pred = cur;
				cur = cur->next;
			}
			pred->Lock();
			cur->Lock();
			while (validate(pred, cur)) {
				if (cur->key != key) {
					pred->unLock();
					cur->unLock();
					return false;
				}
				else {
					cur->removed = true;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = cur->next;
					pred->unLock();
					cur->unLock();
					return true;
				}
			}
			pred->unLock();
			cur->unLock();
		}
	}
	bool Contains(int key) {
		shared_ptr<SPNODE> curr;
		curr = head;
		while (curr->key < key) {
			curr = curr->next;
		}
		return curr->key == key && !curr->removed;
	}
	void Verify() {
		shared_ptr<SPNODE> p;
		p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == tail) break;
			cout << p->key << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

SPZ_SET myset;
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