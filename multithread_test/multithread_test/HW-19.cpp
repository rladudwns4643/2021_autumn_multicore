//non_blocking_synchronization_skip_list

#pragma region x86
/*
release_x86_non_blocking_stnchronization_skiplist
1: 527
2: 385
4: 195
8: 140
*/
/*
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int THREAD_COUNT = 8;

constexpr int NUM_LV = 10;

constexpr int LSB_MASK = 0xFFFFFFFE;

class null_mutex {
public:
	void lock() {};
	void unlock() {};
};

class LFSKNODE {
public:
	int key;
	int next[NUM_LV]; //next + marked
	int toplv;
	LFSKNODE() : key(0), toplv(0) {};
	LFSKNODE(int x, int _toplv) : key(x), toplv(_toplv) {};
	~LFSKNODE() {};

	void set_next(int level, LFSKNODE* p, bool marked) {
		int new_value = reinterpret_cast<int>(p);
		if (marked == true) new_value++;
		next[level] = new_value;
	}

	LFSKNODE* get_next(int level) {
		return reinterpret_cast<LFSKNODE*>(next[level] & LSB_MASK);
	}

	LFSKNODE* get_next(int level, bool* removed) {
		int v = next[level];
		*removed = 1 == (v & 0x1);
		return reinterpret_cast<LFSKNODE*>(v & LSB_MASK);
	}

	bool CAS_NEXT(int level, LFSKNODE* old_p, LFSKNODE* new_p, bool old_marked, bool new_marked) {
		int old_v = reinterpret_cast<int>(old_p);
		if (old_marked) old_v++;
		int new_v = reinterpret_cast<int>(new_p);
		if (new_marked) new_v++;
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&next[level]), &old_v, new_v);
	}

	bool is_removed(int level) {
		return 1 == (next[level] & 0x1);
	}
};

class LFSK_LIST {
	LFSKNODE head, tail;
public:
	LFSK_LIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		for (int i = 0; i < NUM_LV - 1; ++i) {
			head.set_next(i, &tail, false);
		}
	}
	~LFSK_LIST() { Init(); };

	void Init() {
		while (head.get_next(0) != &tail) {
			LFSKNODE* ptr = head.get_next(0);
			head.set_next(0, ptr->get_next(0), false);
			delete ptr;
		}
		for (int i = 0; i < NUM_LV; ++i) {
			head.set_next(i, &tail, false);
		}
	}

	bool Find(int x, LFSKNODE* preds[], LFSKNODE* currs[]) {
		while (true) {
		retry:
			int cur_lv = NUM_LV - 1;
			preds[cur_lv] = &head;
			while (true) {
				currs[cur_lv] = preds[cur_lv]->get_next(cur_lv);
				while (true) {
					bool removed{ false };
					LFSKNODE* succ = currs[cur_lv]->get_next(cur_lv, &removed);
					while (removed == true) {
						bool ret = preds[cur_lv]->CAS_NEXT(cur_lv, currs[cur_lv], succ, false, false);
						if (!ret) { goto retry; }
						currs[cur_lv] = succ;
						succ = currs[cur_lv]->get_next(cur_lv, &removed);
					}
					if (currs[cur_lv]->key < x) {
						preds[cur_lv] = currs[cur_lv];
						currs[cur_lv] = succ;
					}
					else break;
				}
				if (cur_lv == 0) {
					return currs[0]->key == x;
				}
				cur_lv--;
				preds[cur_lv] = preds[cur_lv + 1];
			}
		}
	}

	bool Add(int key) {
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		while (true) {
			bool found = Find(key, preds, currs);
			if (found == true) return false;

			int new_level = 0;
			for (int i = 0; i < NUM_LV; ++i) {
				new_level = i;
				if (rand() % 2 == 0) break;
			}

			LFSKNODE* new_node = new LFSKNODE(key, new_level);
			for (int i = 0; i <= new_level; ++i) {
				LFSKNODE* succ = currs[i];
				new_node->set_next(i, succ, false);
			}

			LFSKNODE* pred = preds[0];
			LFSKNODE* curr = currs[0];
			new_node->set_next(0, curr, false);
			if (!pred->CAS_NEXT(0, curr, new_node, false, false)) {
				continue;
			}
			for (int i = 1; i <= new_level; ++i) {
				while (true) {
					pred = preds[i];
					curr = currs[i];
					if (pred->CAS_NEXT(i, curr, new_node, false, false)) break;
					Find(key, preds, currs);
				}
			}
			return true;
		}
	}

	bool Remove(int key) {
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		bool found = Find(key, preds, currs);
		if (found != true) return false;
		LFSKNODE* target = currs[0];
		int target_top_lv = target->toplv;
		for (int i = target_top_lv; i > 0; --i) {
			bool removed{ false };
			LFSKNODE* succ = target->get_next(i, &removed);
			while (removed == false) {
				target->CAS_NEXT(i, succ, succ, false, true);
				succ = target->get_next(i, &removed); //CAS 실패시 최신 succ, 최신 removed을 사용하기 위함
			}
		}
		LFSKNODE* succ = target->get_next(0);
		while (true) {
			bool delete_succ = target->CAS_NEXT(0, succ, succ, false, true);
			if (delete_succ == true) {
				Find(key, preds, currs); //리스트 정리
				return true;
			}
			bool removed{ false };
			succ = target->get_next(0, &removed);
			if (removed == true) return false;
		}
	}

	bool Contains(int key) {
		bool removed{ false };
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		preds[NUM_LV - 1] = &head;

		for (int i = NUM_LV - 1; i >= 0; --i) {
			currs[i] = preds[i]->get_next(i);
			while (true) {
				LFSKNODE* succ = currs[i]->get_next(i, &removed);
				while (removed == true) {
					currs[i] = currs[i]->get_next(i);
					succ = currs[i]->get_next(i, &removed);
				}
				if (currs[i]->key < key) {
					preds[i] = currs[i];
					currs[i] = succ;
				}
				else break;
			}
			if (i == 0) {
				return currs[0]->key == key;
			}
			preds[i - 1] = preds[i];
		}
		return currs[0]->key == key;
	}

	void Verify() {
		LFSKNODE* p = head.get_next(0);
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->key << ", ";
			p = p->get_next(0);
		}
		cout << endl;
	}
};

LFSK_LIST myset;
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
*/
#pragma endregion

#pragma region x64
/*
* HW-19
* 5600x 4.4ghz 6c12t
release_x64_non_blocking_stnchronization_skiplist
1: 623
2: 382
4: 223
8: 132

release_x64_coarse_grained_synchronization_SkipList
1: 481
2: 519
4: 600
8: 774

release_x86_Elimination_stack
1: 330
2: 378
4: 455
8: 470

release_x64_back-off_lock-free_stack
1: 330
2: 378
4: 455
8: 470

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
constexpr int LSB_MASK = 0xFFFFFFFE;

class LFSKNODE {
public:
	int key;
	int64_t next[NUM_LV]; //next + marked
	int toplv;
	LFSKNODE() : key(0), toplv(0) {};
	LFSKNODE(int x, int _toplv) : key(x), toplv(_toplv) {};
	~LFSKNODE() {};

	void set_next(int level, LFSKNODE* p, bool marked) {
		int64_t new_value = reinterpret_cast<int64_t>(p);
		if (marked == true) new_value++;
		next[level] = new_value;
	}

	LFSKNODE* get_next(int level) {
		return reinterpret_cast<LFSKNODE*>(next[level] & LSB_MASK);
	}

	LFSKNODE* get_next(int level, bool* removed) {
		int64_t v = next[level];
		*removed = 1 == (v & 0x1);
		return reinterpret_cast<LFSKNODE*>(v & LSB_MASK);
	}

	bool CAS_NEXT(int level, LFSKNODE* old_p, LFSKNODE* new_p, bool old_marked, bool new_marked) {
		int64_t old_v = reinterpret_cast<int64_t>(old_p);
		if (old_marked) old_v++;
		int64_t new_v = reinterpret_cast<int64_t>(new_p);
		if (new_marked) new_v++;
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int64_t*>(&next[level]), &old_v, new_v);
	}

	bool is_removed(int level) {
		return 1 == (next[level] & 0x1);
	}
};

class LFSK_LIST {
	LFSKNODE head, tail;
public:
	LFSK_LIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		for (int i = 0; i < NUM_LV - 1; ++i) {
			head.set_next(i, &tail, false);
		}
	}
	~LFSK_LIST() { Init(); };

	void Init() {
		while (head.get_next(0) != &tail) {
			LFSKNODE* ptr = head.get_next(0);
			head.set_next(0, ptr->get_next(0), false);
			delete ptr;
		}
		for (int i = 0; i < NUM_LV; ++i) {
			head.set_next(i, &tail, false);
		}
	}

	bool Find(int x, LFSKNODE* preds[], LFSKNODE* currs[]) {
		while (true) {
		retry:
			int cur_lv = NUM_LV - 1;
			preds[cur_lv] = &head;
			while (true) {
				currs[cur_lv] = preds[cur_lv]->get_next(cur_lv);
				while (true) {
					bool removed{ false };
					LFSKNODE* succ = currs[cur_lv]->get_next(cur_lv, &removed);
					while (removed == true) {
						bool ret = preds[cur_lv]->CAS_NEXT(cur_lv, currs[cur_lv], succ, false, false);
						if (!ret) { goto retry; }
						currs[cur_lv] = succ;
						succ = currs[cur_lv]->get_next(cur_lv, &removed);
					}
					if (currs[cur_lv]->key < x) {
						preds[cur_lv] = currs[cur_lv];
						currs[cur_lv] = succ;
					}
					else break;
				}
				if (cur_lv == 0) {
					return currs[0]->key == x;
				}
				cur_lv--;
				preds[cur_lv] = preds[cur_lv + 1];
			}
		}
	}

	bool Add(int key) {
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		while (true) {
			bool found = Find(key, preds, currs);
			if (found == true) return false;

			int new_level = 0;
			for (int i = 0; i < NUM_LV; ++i) {
				new_level = i;
				if (rand() % 2 == 0) break;
			}

			LFSKNODE* new_node = new LFSKNODE(key, new_level);
			for (int i = 0; i <= new_level; ++i) {
				LFSKNODE* succ = currs[i];
				new_node->set_next(i, succ, false);
			}

			LFSKNODE* pred = preds[0];
			LFSKNODE* curr = currs[0];
			new_node->set_next(0, curr, false);
			if (!pred->CAS_NEXT(0, curr, new_node, false, false)) {
				continue;
			}
			for (int i = 1; i <= new_level; ++i) {
				while (true) {
					pred = preds[i];
					curr = currs[i];
					if (pred->CAS_NEXT(i, curr, new_node, false, false)) break;
					Find(key, preds, currs);
				}
			}
			return true;
		}
	}

	bool Remove(int key) {
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		bool found = Find(key, preds, currs);
		if (found != true) return false;
		LFSKNODE* target = currs[0];
		int target_top_lv = target->toplv;
		for (int i = target_top_lv; i > 0; --i) {
			bool removed{ false };
			LFSKNODE* succ = target->get_next(i, &removed);
			while (removed == false) {
				target->CAS_NEXT(i, succ, succ, false, true);
				succ = target->get_next(i, &removed); //CAS 실패시 최신 succ, 최신 removed을 사용하기 위함
			}
		}
		LFSKNODE* succ = target->get_next(0);
		while (true) {
			bool delete_succ = target->CAS_NEXT(0, succ, succ, false, true);
			if (delete_succ == true) {
				Find(key, preds, currs); //리스트 정리
				return true;
			}
			bool removed{ false };
			succ = target->get_next(0, &removed);
			if (removed == true) return false;
		}
	}

	bool Contains(int key) {
		bool removed{ false };
		LFSKNODE* preds[NUM_LV], * currs[NUM_LV];
		preds[NUM_LV - 1] = &head;

		for (int i = NUM_LV - 1; i >= 0; --i) {
			currs[i] = preds[i]->get_next(i);
			while (true) {
				LFSKNODE* succ = currs[i]->get_next(i, &removed);
				while (removed == true) {
					currs[i] = currs[i]->get_next(i);
					succ = currs[i]->get_next(i, &removed);
				}
				if (currs[i]->key < key) {
					preds[i] = currs[i];
					currs[i] = succ;
				}
				else break;
			}
			if (i == 0) {
				return currs[0]->key == key;
			}
			preds[i - 1] = preds[i];
		}
		return currs[0]->key == key;
	}

	void Verify() {
		LFSKNODE* p = head.get_next(0);
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->key << ", ";
			p = p->get_next(0);
		}
		cout << endl;
	}
};

LFSK_LIST myset;
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
#pragma endregion