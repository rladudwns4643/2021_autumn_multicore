//�Ӽ� -> C++ -> ��� -> omp Ȱ��ȭ -> ��


//x86���� �����ؾ� ��
#include <omp.h>
#include <iostream>
using namespace std;

int main() {
	int g_num_threads{ 0 };
	int sum{ 0 };
//���ھ��� ������ŭ �����
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