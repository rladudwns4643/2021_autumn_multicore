/*
release_x64_Coarse_Grain_QUEUE
1: 467
2: 616
4: 753
8: 1035
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;

class NODE {
public:
	int key;
	NODE* volatile next;
	NODE() {};
	NODE(int key_val) : key(key_val), next(nullptr) {};
	~NODE() {};
};

class C_QUEUE {
	NODE* head, * tail;
	mutex c_lock;
	mutex enq_lock;
	mutex deq_lock;

public:
	C_QUEUE() {
		head = tail = new NODE(0);
	}
	~C_QUEUE() {
		Init();
	}
	void Init() {
		while (head != tail) {
			NODE* p = head;
			head = head->next;
			delete p;
		}
	}

	void Enq(int x) {
		enq_lock.lock();

		NODE* n = new NODE(x);
		tail->next = n;
		tail = n;

		enq_lock.unlock();
	}

	int Deq() {
		int result{ 0 };
		deq_lock.lock();

		if (head->next == nullptr) {
			cout << "ERROR: EMPTY QUEUE" << endl;
			deq_lock.unlock();
			return -1;
		}
		result = head->next->key;
		head = head->next;

		deq_lock.unlock();
		return result;
	}

	bool Contains(int x) {

	}

	void Verify() {
		NODE* p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) break;
			cout << p->key << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

C_QUEUE myqueue;

void Benchmark(int num_threads) {
	const int NUM_TEST = 10'000'000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || i < 1000 / THREAD_COUNT) myqueue.Enq(i);
		else myqueue.Deq();
	}
}

int main() {
	for (int i = 1; i <= THREAD_COUNT; i *= 2) {
		vector<thread> worker;
		myqueue.Init();

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
		myqueue.Verify();

		cout << "THREAD CNT: " << i <<
			" exec_t: " << duration_cast<milliseconds>(exec_t).count() << "ms\n---\n";
	}
}