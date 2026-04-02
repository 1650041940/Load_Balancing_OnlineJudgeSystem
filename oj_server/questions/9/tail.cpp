#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

static bool EqVec(const vector<int> &a, const vector<int> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) if (a[i] != b[i]) return false;
    return true;
}

void Test1()
{
    auto out = Solution().sort3(3, 1, 2);
    if (EqVec(out, {1, 2, 3})) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    auto out = Solution().sort3(-1, -3, 2);
    if (EqVec(out, {-3, -1, 2})) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
