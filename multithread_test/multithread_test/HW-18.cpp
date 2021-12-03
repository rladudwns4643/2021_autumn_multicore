/*
* HW-18
* 5600x 4.4ghz 6c12t
release_x64_lazy_synchronization_SkipList
1: 586
2: 362
4: 183
8: 129
*/

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;
constexpr int NUM_LV = 10;


class SKNODE {
public:
	int value;
	SKNODE* volatile next[NUM_LV];
	int top_lv; // ~ NUM_LEVEL - 1
	volatile bool fully_linked; //최적화로 인한 순서 변경 금지
	volatile bool is_removed;
	recursive_mutex n_lock;
	SKNODE() : value(0), top_lv(0), fully_linked(false), is_removed(false) {
		for (auto& n : next) n = nullptr;
	}
	SKNODE(int x, int top) : value(x), top_lv(top), fully_linked(false), is_removed(false) {
		for (auto& n : next) n = nullptr;
	}
	~SKNODE() {}
};

class LazySK_SET {
	SKNODE head, tail;
public:
	LazySK_SET() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	~LazySK_SET() {
		Init();
	}
	void Init() {
		while (head.next[0] != &tail) {
			SKNODE* p = head.next[0];
			head.next[0] = p->next[0];
			delete p;
		}
		for (auto& n : head.next) {
			n = &tail;
		}
	}

	int Find(int x, SKNODE* pred[], SKNODE* curr[]) { //찾은 위치 return
		int found_lv = -1;
		int cur_lv = NUM_LV - 1;
		pred[cur_lv] = &head;
		while (true) {
			curr[cur_lv] = pred[cur_lv]->next[cur_lv];
			while (curr[cur_lv]->value < x) {
				pred[cur_lv] = curr[cur_lv];
				curr[cur_lv] = curr[cur_lv]->next[cur_lv];
			}
			if ((found_lv == -1) && curr[cur_lv]->value == x) found_lv = cur_lv; //처음 찾은 cur_lv 사용
			if (cur_lv == 0) return found_lv;
			cur_lv--;
			pred[cur_lv] = pred[cur_lv + 1];
		}
	}

	bool Add(int x) {
		int new_level = 0;
		for (int i = 0; i < NUM_LV; ++i) {
			new_level = i;
			if (rand() % 2 == 0) break;
		}
		SKNODE* preds[NUM_LV], * currs[NUM_LV];
		while (true) {
			int found_lv = Find(x, preds, currs);
			if (found_lv != -1) {
				SKNODE* found_node = currs[found_lv];
				if (!found_node->is_removed) {
					while (!found_node->fully_linked) {}
					return false;
				}
				continue;
			}
			SKNODE* pred, * curr;
			bool valid{ true };
			int max_lock_lv{ -1 };
			for (int i = 0; valid && (i <= new_level); ++i) {
				pred = preds[i];
				curr = currs[i];
				pred->n_lock.lock();
				max_lock_lv = i;
				valid = (!pred->is_removed) && (!curr->is_removed) && (pred->next[i] == curr);
			}
			if (!valid) {
				for (int i = 0; i <= max_lock_lv; ++i) {
					preds[i]->n_lock.unlock();
				}
				continue;
			}
			SKNODE* new_node = new SKNODE(x, new_level);
			for (int i = 0; i <= new_level; ++i) {
				new_node->next[i] = currs[i];
			}
			for (int i = 0; i <= new_level; ++i) {
				preds[i]->next[i] = new_node;
			}
			new_node->fully_linked = true;
			for (int i = 0; i <= max_lock_lv; ++i) {
				preds[i]->n_lock.unlock();
			}
			return true;
		}
	}

	bool Remove(int x) {
		//target의 유효성 검사
		SKNODE* target{}; //remove 될 node
		SKNODE* preds[NUM_LV], * currs[NUM_LV];
		int found_lv = Find(x, preds, currs);
		if (found_lv == -1) return false;
		target = currs[found_lv];
		if ((target->is_removed == true) || (target->fully_linked == false) || (found_lv == -1) || (target->top_lv != found_lv)) return false;

		//target을 지우기 위해 다른 쓰레드와 충돌 검사 (find시 lock을 걸지 않기 때문에)
		target->n_lock.lock();
		if (target->is_removed == true) {
			target->n_lock.unlock();
			return false;
		}
		target->is_removed = true;

		//링크 재조정 (is_removed가 true이기 때문에 다른 쓰레드는 접근할 수 없다)
		while (true) {
			bool is_invalid{ false };
			int max_lock_lv{ -1 };
			for (int i = 0; i <= target->top_lv; ++i) {
				preds[i]->n_lock.lock();
				max_lock_lv = i;
				if ((preds[i]->is_removed == true) || (preds[i]->next[i] != target)) {
					//valid 검사 실패(누군가 노드를 바꿈 -> 다시 find부터)
					is_invalid = true;
					break;
				}
			}
			if (is_invalid == true) {
				for (int i = 0; i <= max_lock_lv; ++i) {
					preds[i]->n_lock.unlock();
				}
				Find(x, preds, currs);
				continue;
			}
			for (int i = 0; i <= target->top_lv; ++i) {
				preds[i]->next[i] = target->next[i];
			}
			for (int i = 0; i <= max_lock_lv; ++i) {
				preds[i]->n_lock.unlock();
			}
			target->n_lock.unlock();
			return true;
		}
	}

	bool Contains(int x) {
		SKNODE* preds[NUM_LV], * currs[NUM_LV];
		int found_lv = Find(x, preds, currs);
		return (found_lv != -1 && currs[found_lv]->fully_linked && !currs[found_lv]->is_removed);
	}

	void Verify() {
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next[0];
		}
		cout << endl;
	}
};

LazySK_SET myset;

void Benchmark(int num_threads) {
	const int NUM_TEST = 4'000'000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		//cout << x << " ";
		switch (rand() % 3) {
		case 0: myset.Add(x); break;
		case 1: myset.Remove(x); break;
		case 2: myset.Contains(x); break;
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