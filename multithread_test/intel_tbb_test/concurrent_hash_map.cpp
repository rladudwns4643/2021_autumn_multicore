//count string

#include <tbb/concurrent_hash_map.h>
#include <tbb/parallel_for.h>
#include <unordered_map>
#include <string>
#include <iostream>
#include <chrono>

using namespace std;
using namespace tbb;
using namespace chrono;

constexpr size_t n = 50'000'000;
string d[n];

int CountStr_single() {
	unordered_map<string, int> t;
	for (auto& s : d) {
		t[s]++;
	}
	return t["BCD"];
}

int CountStr_tbb() {
	concurrent_hash_map<string, atomic_int> t;
	parallel_for(size_t(0), n, [&t](int i) {
		concurrent_hash_map<string, atomic_int>::accessor acc;
		t.insert(acc, d[i]);
		acc->second++;
		});
	concurrent_hash_map<string, atomic_int>::const_accessor acc;
	t.find(acc, "BCD");
	return acc->second;
}

int main() {

	for (auto& s : d) {
		int len{ rand() % 5 };
		for (int i = 0; i < len; ++i) {
			s += ('A' + rand() % 26);
		}
	}
	{
		auto start_t = system_clock::now();
		int res = CountStr_single();
		auto end_t = system_clock::now();
		auto e_t = end_t - start_t;

		cout << "V: " << res << " time: " << duration_cast<milliseconds>(e_t).count() << endl;
	}
	{
		auto start_t = system_clock::now();
		int res = CountStr_tbb();
		auto end_t = system_clock::now();
		auto e_t = end_t - start_t;

		cout << "V: " << res << " time: " << duration_cast<milliseconds>(e_t).count() << endl;
	}
}