#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

void Test1()
{
    vector<int> v = {1, 2, 2, 3};
    int out = Solution().uniqueCount(v);
    if (out == 3) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    vector<int> v;
    int out = Solution().uniqueCount(v);
    if (out == 0) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
