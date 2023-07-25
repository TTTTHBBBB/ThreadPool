#include<iostream>
#include<memory>
using namespace std;
struct A {
	int a;
	void func() {
		auto f = [*this] { 
			cout << a << endl;
		};
		f();
	}
};
int main()
{
	//shared_ptr<int> ptr(new int(10),
	//	[](int* p) {delete p; });
	//cout << *ptr << endl;
	//constexpr auto lmb = [](int a) {
	//	return a * a;
	//};
	//cout << lmb(20) << endl;
	//A a(10);
	//a.func();
	return 0;
}