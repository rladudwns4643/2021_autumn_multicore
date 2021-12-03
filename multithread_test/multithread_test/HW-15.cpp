/* HW-15
* 5600x 6core 4.2ghz
release_x86_Elimination_stack
1: 330
2: 378
4: 455
8: 470

release_x64_coarse-grained_stack
1: 434
2: 655
4: 933
8: 1262

release_x64_nullmutex_stack
1: 450
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

const int THREAD_COUNT = 16;
const int NUM_TEST = 10'000'000;

class NODE {
public:
	int key;
	NODE* next;
	NODE() {};
	NODE(int key_val) : key(key_val), next(nullptr) {};
	~NODE() {};
};

constexpr int EMPTY = 0;
constexpr int WAITING = 0x40000000;
constexpr int BUSY = 0x80000000;

class LockFreeExchanger {
	atomic<unsigned int> slot;
public:
	LockFreeExchanger() {}
	~LockFreeExchanger() {}
	int exchange(int value, bool* time_out, bool* busy) {
		while (true) {

			unsigned int cur_slot = slot;
			unsigned int slot_state = cur_slot & 0xC0000000;
			unsigned int slot_value = cur_slot & 0x3FFFFFFF; //atomic 한 읽기를 위해 cur_slot 사용
			switch (slot_state) {
			case EMPTY: {
				unsigned int new_slot = WAITING & value;
				if (atomic_compare_exchange_strong(&slot, &cur_slot, new_slot) == true) {
					for (int i = 0; i < 10; ++i) { //timeout = 10tick
						if (slot & 0xC0000000 == BUSY) {
							int ret_val = slot & 0x3FFFFFFF;
							slot = EMPTY;
							return ret_val;
						}
					}
					if (atomic_compare_exchange_strong(&slot, &new_slot, EMPTY) == true) {
						*time_out = true;
						*busy = false;
						return 0; // timeout으로 종료
					}
					else {
						//다른 쓰레드가 busy로 만들었음
						int ret_val = slot & 0x3FFFFFFF;
						slot = EMPTY;
						return ret_val;
					}
				}
				else {
					continue; //WAITING일 경우 들어오니까 최신의 상태를 받아와 WAITING으로 옮김
				}
				break;
			}
			case WAITING: {
				unsigned int new_value = BUSY | value;
				if (atomic_compare_exchange_strong(&slot, &cur_slot, new_value) == true) {
					return slot_value;
				}
				else {
					//경합이 심함
					*time_out = false;
					*busy = true;
					return 0;
				}
				break;
			}
			case BUSY: {
				*time_out = false;
				*busy = true;
				return 0;
				break;
			}
			}
		}
	}
};

class EliminationArray {
	int range;
	int _num_threads;
	LockFreeExchanger exchanger[THREAD_COUNT / 2];
public:
	EliminationArray() { range = 1; _num_threads = THREAD_COUNT; };
	EliminationArray(int num_threads) : _num_threads(num_threads) { range = 1; }
	~EliminationArray() {}
	void SetThreads(int num_threads) {
		_num_threads = num_threads;
	}
	int Visit(int value, bool* time_out) {
		int slot = rand() % range;
		bool busy;
		int ret = exchanger[slot].exchange(value, time_out, &busy);
		if ((*time_out == true) && (range > 1)) range--;
		if ((busy == true) && (range <= _num_threads / 2)) range++;
		return ret;
	}
};

class LFEL_STACK {
	EliminationArray _ea;

	NODE* volatile top;
	bool CAS(NODE* volatile* ptr, NODE* old_ptr, NODE* new_ptr) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int64_t volatile*>(ptr),
			reinterpret_cast<int64_t*>(&old_ptr),
			reinterpret_cast<int64_t>(new_ptr)
		);
	}
public:
	LFEL_STACK() : top(nullptr) {}
	~LFEL_STACK() { Init(0); }
	void Init(int num_thread) {
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
		_ea.SetThreads(num_thread);
	}

	void Push(int input) {
		NODE* e = new NODE{ input };
		while (true) {
			NODE* ptr = top;
			e->next = ptr;
			if (CAS(&top, ptr, e) == true) {
				return;
			}
			else {
				bool time_out;
				int ret = _ea.Visit(input, &time_out);
				if (ret == -1 && time_out == false) return;
			}
		}
	}

	int Pop() {
		while (true) {
			NODE* ptr = top;
			if (ptr == nullptr) { return -2; }
			int t = ptr->key;
			if (CAS(&top, ptr, ptr->next) == true) {
				//delete ptr; //aba
				return t;
			}
			else {
				bool time_out;
				int ret = _ea.Visit(-1, &time_out);
				if (ret != -1 && time_out == false) return ret;
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

LFEL_STACK mystack;

void Benchmark(int num_threads) {
	for (int i = 1; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || (i < 1000 / num_threads)) mystack.Push(i);
		else mystack.Pop();
	}
}

int main() {
	for (int i = 1; i <= THREAD_COUNT; i *= 2) {
		vector<thread> worker;
		mystack.Init(i);

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