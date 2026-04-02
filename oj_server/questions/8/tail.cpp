#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

void Test1()
{
    int out = Solution().gcd(12, 18);
    if (out == 6) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    int out = Solution().gcd(7, 3);
    if (out == 1) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
