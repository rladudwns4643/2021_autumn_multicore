//속성 -> C++ -> 언어 -> omp 활성화 -> 네


//x86으로 실행해야 함
#include <omp.h>
#include <iostream>
using namespace std;

int main() {
	int g_num_threads{ 0 };
	int sum{ 0 };
//논리코어의 개수만큼 실행됨
#pragma omp parallel
	{
		int num_threads{ omp_get_num_threads() };
		for (int i = 0; i < 50'000'000 / num_threads; ++i) {
#pragma omp critical
			sum += 2;
		}
		g_num_threads = omp_get_num_threads();
	}
	cout << "therad cnt: " << g_num_threads << endl;
	cout << "sum: " << sum << endl;
}