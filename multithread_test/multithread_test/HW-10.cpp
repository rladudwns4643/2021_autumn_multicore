/* HW-10
release_x64_TSLFQUEUE
1: 345
2: 476
4: 671
8: 1005
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

class TSLFQUEUE {
	NODE* volatile head;
	NODE* volatile tail;

public:
	TSLFQUEUE() {
		head = tail = new NODE(0);
	}
	~TSLFQUEUE() {
		Init();
	}
	void Init() {
		while (head != tail) {
			NODE* p = head;
			head = head->next;
			delete p;
		}
	}

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<int64_t*>(&old_node),
			reinterpret_cast<int64_t>(new_node));
	}

	void Enq(int x) {
		NODE* e = new NODE(x);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;
			if (last != tail) continue;		//진입하고 tail이 바뀐경우 tail을 다시 읽어야 해서 continue
			if (next == nullptr) {			//last->next(last==tail) == nullptr 일경우 끝이므로 새로운 값 추가
				if (CAS(last->next, nullptr, e) == true) { //last->next(last==tail)이 nullptr일 경우 e로 바꾸는 것에 실패하면 다시, 성공하면
					CAS(tail, last, e);		//tail의 값이 last인 것을 확인하고 e로 바꿈
					return;
				}
			}
			else CAS(tail, last, next);
		}
	}

	int Deq() {
		int result{ 0 };
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first != head) continue;
			if (first == last) {
				if (next == nullptr) {
					//cout << "ERROR: EMPTY QUEUE" << endl;
					return -1;
				}
				CAS(tail, last, next);
				continue;
			}
			int value = next->key;
			if (CAS(head, first, next) == false)continue;
			delete first;
			return value;
		}
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

TSLFQUEUE myqueue;

void Benchmark(int num_threads) {
	const int NUM_TEST = 10'000'000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || (i < 2 / THREAD_COUNT)) myqueue.Enq(i);
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