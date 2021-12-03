/* HW-11
release_x64_coarse-grained_stack
1: 434
2: 655
4: 933
8: 1262
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

class C_STACK {
	NODE* top;
	mutex g_lock;

public:
	C_STACK() : top(nullptr) {}
	~C_STACK() {
		Init();
	}
	void Init() {
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}

	void Push(int input) {
		NODE* e = new NODE{ input };
		g_lock.lock();
		e->next = top;
		top = e;
		g_lock.unlock();
	}

	int Pop() {
		g_lock.lock();
		if (top == nullptr) {
			g_lock.unlock();
			return -2;
		}
		int t = top->key;
		NODE* ptr = top;
		top = top->next;
		delete ptr;
		g_lock.unlock();
		return t;
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

C_STACK mystack;

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