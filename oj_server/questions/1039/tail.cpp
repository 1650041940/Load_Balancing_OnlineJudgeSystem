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
963174258
178325649
254689731
821437596
496852317
735961824
589713462
317246985
642598173
060104050
200000001
008305600
800407006
006000300
700901004
500000002
040508070
007206900

534678912
672195348
198342567
859761423
426853791
713924856
961537284
287419635
345286179
010900605
025060070
870000902
702050043
000204000
490010508
107000056
040080210
208001090)OJ_IN";
    const string expect = R"OJ_OUT(Yes
No)OJ_OUT";
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
