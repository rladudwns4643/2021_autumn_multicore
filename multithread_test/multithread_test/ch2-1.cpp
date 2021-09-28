#include <iostream>
#include <thread>
#include <mutex>
using namespace std;
int g_data = 0;
bool g_flag = false;
mutex ll;

void receive() {
	ll.lock();
	while (false == g_flag) {
		ll.unlock();
		ll.lock();
	}
	ll.unlock();
	cout << "V : " << g_data << endl;
}

void sender() {
	g_data = 999;
	ll.lock();
	g_flag = true;
	ll.unlock();
}

int main() {
	thread t1{ receive };
	thread t2{ sender };

	t1.join();
	t2.join();
}