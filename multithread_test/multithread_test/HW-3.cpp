/*
release_x64_find_grained_synchronization
1: 10155
2: 9702
4: 6073
8: 4487
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

/*C_SET*/
//class NODE {
//public:
//	int key;
//	NODE* next;
//
//	NODE() : next(nullptr) {};
//	NODE(int key_val) : key(key_val), next(nullptr) {};
//	~NODE() {};
//};

/*F_SET*/
class NODE {
public:
	int key;
	NODE* next;
	mutex node_lock;

	NODE() : next(nullptr) {};
	NODE(int key_val) : key(key_val), next(nullptr) {};
	void Lock() {
		node_lock.lock();
	}
	void unLock() {
		node_lock.unlock();
	}
	~NODE() {};
};

class C_SET {
	NODE head, tail;
	mutex g_lock;
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
		g_lock.lock();
		cur = head.next;
		while (cur->key < key) {
			cur = cur->next;
		}
		if (cur->key != key) {
			g_lock.unlock();
			return false;
		}
		else {
			g_lock.unlock();
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

class F_SET {
	NODE head, tail;
	mutex g_lock;
public:
	F_SET() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~F_SET() {};

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
		head.Lock();
		pred = &head;
		cur = pred->next;
		cur->Lock();
		while (cur->key < key) {
			pred->unLock();
			pred = cur;
			cur = cur->next;
			cur->Lock();
		}
		if (cur->key == key) {
			pred->unLock();
			cur->unLock();
			return false;
		}
		else {
			NODE* node = new NODE(key);
			node->next = cur;
			pred->next = node;
			pred->unLock();
			cur->unLock();
			return true;
		}
	}
	bool Remove(int key) {
		NODE* pred, * cur;
		head.Lock();
		pred = &head;
		cur = pred->next;
		cur->Lock();
		while (cur->key < key) {
			pred->unLock();
			pred = cur;
			cur = cur->next;
			cur->Lock();
		}
		if (cur->key != key) {
			pred->unLock();
			cur->unLock();
			return false;
		}
		else {
			pred->next = cur->next;
			cur->unLock();
			pred->unLock();
			delete cur;
			return true;
		}
	}
	bool Contains(int key) {
		NODE* pred, * cur;
		head.Lock();
		pred = &head;
		cur = pred->next;
		cur->Lock();
		while (cur->key < key) {
			pred->unLock();
			pred = cur;
			cur = cur->next;
			cur->Lock();
		}
		if (cur->key != key) {
			pred->unLock();
			cur->unLock();
			return false;
		}
		else {
			pred->unLock();
			cur->unLock();
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

F_SET myset;
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