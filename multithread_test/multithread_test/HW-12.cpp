/* HW-12
release_x64_lock-free_stack
1: 350
2: 371
4: 591
8: 895

release_x64_coarse-grained_stack
1: 434
2: 655
4: 933
8: 1262

release_x64_null-mutex_stack
1: 340
2: -
4: -
8: -
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;
const int NUM_TEST = 10'000'000;

class NODE {
public:
	int key;
	NODE* next;
	NODE() {};
	NODE(int key_val) : key(key_val), next(nullptr) {};
	~NODE() {};
};

class STPTR {
public:
	NODE* volatile ptr;
	int		volatile stamp;
};

class LF_STACK {
	NODE* volatile top;
	bool CAS(NODE* volatile* ptr, NODE* old_ptr, NODE* new_ptr) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int64_t volatile*>(ptr),
			reinterpret_cast<int64_t*>(&old_ptr),
			reinterpret_cast<int64_t>(new_ptr)
		);
	}
public:
	LF_STACK() : top(nullptr) {}
	~LF_STACK() { Init(); }
	void Init() {
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}

	void Push(int input) {
		NODE* e = new NODE{ input };
		while (true) {
			NODE* ptr = top;
			e->next = ptr;
			if (CAS(&top, ptr, e) == true) {
				return;
			}
		}
	}

	int Pop() {
		while (true) {
			if (top == nullptr) { return -2; }
			NODE* ptr = top;
			int t = ptr->key;
			if (CAS(&top, ptr, ptr->next) == true) {
				return t;
			}
		}
	}

	void Verify() {
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) break;
			cout << p->key << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

LF_STACK mystack;

void Benchmark(int num_threads) {
	for (int i = 1; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || (i < 1000)) mystack.Push(i);
		else mystack.Pop();
	}
}

int main() {
	for (int i = 1; i <= THREAD_COUNT; i *= 2) {
		vector<thread> worker;
		mystack.Init();

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
		mystack.Verify();

		cout << "THREAD CNT: " << i <<
			" exec_t: " << duration_cast<milliseconds>(exec_t).count() << "ms\n---\n";
	}
}