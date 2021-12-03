/* HW-13
release_x64_SPLF_Queue
1: 350
2: 371
4: 591
8: 895
*/

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;


class NODE {
public:
	int value;
	NODE* volatile next;

	NODE() : next(nullptr) {}
	NODE(int x) : value(x), next(nullptr) {}
	~NODE() {}
};

class null_mutex {
public:
	void lock() {}
	void unlock() {}
};

struct STAMP_POINTER {
	NODE* volatile ptr;
	int volatile stamp;
};

class SPLF_QUEUE {
	STAMP_POINTER head;
	STAMP_POINTER tail;

public:
	SPLF_QUEUE()
	{
		head.ptr = tail.ptr = new NODE(0);
		head.stamp = tail.stamp = 0;
	}

	~SPLF_QUEUE() { Init(); }

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}

	bool STAMP_CAS(STAMP_POINTER* next, NODE* old_node, NODE* new_node, int old_stamp, int new_stamp)
	{
		STAMP_POINTER old_p{ old_node, old_stamp };
		STAMP_POINTER new_p{ new_node, new_stamp };
		long long new_value = *(reinterpret_cast<long long*>(&new_p));
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong*>(next),
			reinterpret_cast<long long*>(&old_p),
			new_value);
	}

	void Init()
	{
		while (head.ptr != tail.ptr) {
			NODE* p = head.ptr;
			head.ptr = head.ptr->next;
			delete p;
		}
	}

	void Enq(int x)
	{
		NODE* e = new NODE(x);
		while (true) {
			STAMP_POINTER last = tail;
			NODE* next = last.ptr->next;
			if (last.ptr != tail.ptr) continue;
			if (nullptr == next) {
				if (CAS(last.ptr->next, nullptr, e)) {
					STAMP_CAS(&tail, last.ptr, e, last.stamp, last.stamp + 1);
					return;
				}
			}
			else
				STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp + 1);
		}
	}

	int Deq()
	{
		while (true) {
			STAMP_POINTER first = head;
			STAMP_POINTER last = tail;
			NODE* next = first.ptr->next;
			if ((first.ptr != head.ptr) || (first.stamp != head.stamp)) continue;
			if (nullptr == next) return -1;
			if (first.ptr == last.ptr) {
				STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp + 1);
				continue;
			}
			int value = next->value;
			if (false == STAMP_CAS(&head, first.ptr, next, first.stamp, first.stamp + 1)) continue;
			delete first.ptr;
			return value;
		}
	}

	bool Contains(int x)
	{

	}

	void Verify()
	{
		NODE* p = head.ptr->next;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) break;
			cout << p->value << ", ";
			p = p->next;
			if (p == tail.ptr) break;
		}
		cout << endl;
	}

};

SPLF_QUEUE myqueue;

void Benchmark(int num_threads)
{
	const int NUM_TEST = 10000000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || i < 32 / num_threads)
			myqueue.Enq(i);
		else
			myqueue.Deq();
	}
}

int main()
{
	for (int i = 1; i <= 8; i = i * 2) {
		vector<thread> worker;
		myqueue.Init();

		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i);
		}
		for (auto& th : worker)
			th.join();
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		myqueue.Verify();
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";
	}
}