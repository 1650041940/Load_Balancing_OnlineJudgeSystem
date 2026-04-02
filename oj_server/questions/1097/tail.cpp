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
    const string input = R"OJ_IN(1 aaaa
a 2
3 aaa
a 2
aa 5
aaa 6
4 abc
a 1
bc 2
ab 4
c 1
3 abcd
ab 10
bc 20
cd 30
3 abcd
cd 100
abc 1000
bcd 10000)OJ_IN";
    const string expect = R"OJ_OUT(8
7
5
40
-1)OJ_OUT";
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
