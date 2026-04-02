#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

void Test1()
{
    auto out = Solution().fib(0);
    if (out == 0) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    auto out = Solution().fib(10);
    if (out == 55) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
