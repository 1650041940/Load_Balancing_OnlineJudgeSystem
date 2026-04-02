#ifndef COMPILER_ONLINE
#include "header.cpp"
#endif

static string Normalize(string s)
{
    // normalize newlines
    for (size_t i = 0; i + 1 < s.size(); i++)
        if (s[i] == '\r' && s[i + 1] == '\n') { s.erase(i, 1); }
    // trim trailing spaces per line
    string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size())
    {
        size_t j = s.find('\n', i);
        if (j == string::npos) j = s.size();
        size_t end = j;
        while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\t')) end--;
        out.append(s.substr(i, end - i));
        if (j < s.size()) out.push_back('\n');
        i = j + 1;
    }
    // trim final newlines
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

static string RunOne(const string& input)
{
    istringstream iss(input);
    ostringstream oss;
    Solve(iss, oss);
    return oss.str();
}

static void Test1()
{
    const string input = R"OJ_IN(2
152
1
radfsdoc M 3 57-58 102-114 23-47
1
100
4
swapfile I 3 5-10 80-95 25-50
smallfile M 2 1-4 11-14
bigfile M 2 15-24 51-60
tinyfile M 1 61-64
2)OJ_IN";
    const string expect = R"OJ_OUT(DATA SET #1
radfsdoc M 1 1-38
DATA SET #2
tinyfile M 1 1-4
swapfile I 3 5-10 25-50 80-95
bigfile M 2 15-24 51-60
smallfile M 1 61-67)OJ_OUT";
    const string got = RunOne(input);
    if (Normalize(got) == Normalize(expect))
    {
        cout << "通过用例1, 样例通过 ... OK!" << endl;
    }
    else
    {
        cout << "没有通过用例1, 样例不匹配" << endl;
        cout << "[expect]\n" << expect << endl;
        cout << "[got]\n" << got << endl;
    }
}

int main()
{
    Test1();
    return 0;
}
