#include <tbb/parallel_for.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace tbb;
using namespace chrono;

atomic_int sum = 0;

constexpr int CNT = 8;

int main() {
	{
		sum = 0;
		size_t n{ 50'000'000 };
		auto s_t = system_clock::now();
		parallel_for(size_t(0), n, [&](int i) {
			sum += 2;
			});
		auto e_t = system_clock::now();
		auto ee_t = e_t - s_t;

		cout << "SUM: " << sum << "\nExec_time: " << duration_cast<milliseconds>(ee_t).count() << "ms\n";
	}
	cout << "---------------------------" << endl;
	{
		sum = 0;
		size_t n{ 50'000'000 };
		auto s_t = system_clock::now();
		vector<thread> vt;
		for (int i = 0; i < CNT; ++i) vt.emplace_back([&]() {for (int i = 0; i < 50'000'000/CNT; ++i) sum += 2; });
		for (auto& th : vt) th.join();
		auto e_t = system_clock::now();
		auto ee_t = e_t - s_t;

		cout << "SUM: " << sum << "\nExec_time: " << duration_cast<milliseconds>(ee_t).count() << "ms\n";
	}
}