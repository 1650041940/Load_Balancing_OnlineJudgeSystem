#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

void Test1()
{
    bool out = Solution().isPrime(2);
    if (out) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    bool out = Solution().isPrime(15);
    if (!out) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
