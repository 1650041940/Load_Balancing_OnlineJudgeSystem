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
    vector<int> nums = {2, 7, 11, 15};
    auto ans = Solution().twoSum(nums, 9);
    if (EqVec(ans, {0, 1}) || EqVec(ans, {1, 0}))
        cout << "通过用例1... OK" << endl;
    else
        cout << "没有通过用例1" << endl;
}

void Test2()
{
    vector<int> nums = {3, 2, 4};
    auto ans = Solution().twoSum(nums, 6);
    if (EqVec(ans, {1, 2}) || EqVec(ans, {2, 1}))
        cout << "通过用例2... OK" << endl;
    else
        cout << "没有通过用例2" << endl;
}

int main()
{
    Test1();
    Test2();
    return 0;
}
