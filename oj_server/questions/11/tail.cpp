#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

static bool EqMat(const vector<vector<int>> &a, const vector<vector<int>> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
    {
        if (a[i].size() != b[i].size()) return false;
        for (size_t j = 0; j < a[i].size(); j++)
            if (a[i][j] != b[i][j]) return false;
    }
    return true;
}

void Test1()
{
    vector<vector<int>> m = {{1, 2, 3}, {4, 5, 6}};
    auto out = Solution().transpose(m);
    vector<vector<int>> exp = {{1, 4}, {2, 5}, {3, 6}};
    if (EqMat(out, exp)) cout << "通过用例1... OK" << endl;
    else cout << "没有通过用例1" << endl;
}

void Test2()
{
    vector<vector<int>> m = {{7}};
    auto out = Solution().transpose(m);
    vector<vector<int>> exp = {{7}};
    if (EqMat(out, exp)) cout << "通过用例2... OK" << endl;
    else cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
