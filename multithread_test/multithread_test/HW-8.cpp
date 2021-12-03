/*
STL SET<int>을 만능객체를 사용하여 무잠금으로 구현
 !1. LIST 벤치마크 프로그램을 사용하여 성능 측정
	-> 루프횟수 4000회
 !2. thread 개수가 바뀔 때 LFUnivesal객체의 log을 클리어
 !3. mutex로 구현한 것과 속도비교
	-> 비교 case 1, 2, 4
*/

/*
release_x64_LFUniversal_Set
1: 3ms
2: 1ms
4: 2ms
8: 3ms
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <set>
#include <vector>
#include <chrono>
using namespace std;

constexpr int thread_cnt = 8;

enum MethodType { INSERT, ERASE, CONTAINS, CLEAR, VERIFY };
typedef int InputValue;
typedef int Response;

class Invocation {
public:
	MethodType t;
	InputValue v;
};

class SeqObj_Set {
public:
	set<int> m_s;
	Response apply(Invocation invoc) {
		int res = -1;
		switch (invoc.t) {
		case MethodType::INSERT: {
			if (m_s.count(invoc.v) != 0) {
				return res;
			}
			m_s.insert(invoc.v);
			break;
		}
		case MethodType::ERASE: {
			if (m_s.count(invoc.v) == 0) {
				return res;
			}
			res = m_s.erase(invoc.v);
			break;
		}
		case MethodType::CONTAINS: {
			m_s.count(invoc.v);
			break;
		}
		case MethodType::CLEAR: {
			m_s.clear();
			break;
		}
		case MethodType::VERIFY: {
			if (m_s.size() < 20) break;
			int cnt{ 0 };
			for (auto v : m_s) {
				cout << v << ", ";
				cnt++;
				if (cnt >= 20) break;
			}
			cout << endl;
			break;
		}
		}
		return res;
	}
};

class node;
class Consensus {
private:
	int64_t INIT = -1;
	atomic_int64_t next;
public:
	Consensus() {
		next = INIT;
	}

	node* decide(node* n) {
		int64_t v = reinterpret_cast<int64_t>(n);
		atomic_compare_exchange_strong(&next, &INIT, v);
		v = next;
		return reinterpret_cast<node*>(v);
	};
};

class node {
public:
	Invocation invoc; //받은 명령
	Consensus decideNext;
	node* next;
	volatile int seq;

	node() : seq(0), next(nullptr) {}

	node(const Invocation& input_invoc) {
		invoc = input_invoc;
		next = nullptr;
		seq = 0;
	}

	~node() {}

	node* max(node** n) {
		node* ret = n[0];
		for (int i = 0; i < thread_cnt; ++i) {
			if (ret < n[i])	ret = n[i];
		}
		return ret;
	}
};

class LFUniversal {
private:
	node* head[thread_cnt];
	node tail;
public:
	LFUniversal() {
		tail.seq = 1;
		for (int i = 0; i < thread_cnt; ++i) head[i] = &tail;
	}

	Response apply(Invocation invoc, int t_id) {
		node* prefer = new node(invoc);
		while (prefer->seq == 0) {
			node* before = tail.max(head);
			node* after = before->decideNext.decide(prefer);
			before->next = after;
			after->seq = before->seq + 1;
			head[t_id] = after;
		}
		SeqObj_Set obj;
		node* cur = tail.next;
		while (cur != nullptr && (cur != prefer) && (cur->invoc.t != VERIFY)) {
			obj.apply(cur->invoc);
			cur = cur->next;
		}
		return obj.apply(invoc);
	}
};

LFUniversal u;

void Benchmark(int num_threads, int my_id)
{
	const int NUM_TEST = 4000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 2) {
		case 0: u.apply(Invocation{ INSERT, i }, my_id);
			break;
		case 1: u.apply(Invocation{ ERASE, i }, my_id);
			break;
		case 2: u.apply(Invocation{ CONTAINS, i }, my_id);
			break;
		}
	}
}

int main() {
	for (int i = 1; i <= thread_cnt; i *= 2) {
		vector<thread> worker;
		u.apply(Invocation{ CLEAR, 0 }, 0);

		auto start_t = chrono::system_clock::now();
		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i, j);
		}
		for (auto& th : worker) {
			th.join();
		}
		auto end_t = chrono::system_clock::now();
		auto exec_t = end_t - start_t;
		//값 확인을 위해 set의 상위 20개 출력

		u.apply(Invocation{ VERIFY,0 }, 0);
		cout << "THREAD CNT: " << i <<
			" exec_t: " << chrono::duration_cast<chrono::milliseconds>(exec_t).count() << "ms\n---\n";
	}
}