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
    const string input = R"OJ_IN(3
3
5
A % 10 20 30 20 10
B = A[0] + A[-1]
C = 0.5 * ( A[0] + B[0] )
3
5
Z = 0.5 * Y[0] + 0.5 * B[0]
Y % 50 10 50 10 50
B % 10 50 10 50 15
3
5
A % 1 2 3 4 5
B = A[-2]
C = B[2])OJ_IN";
    const string expect = R"OJ_OUT(DATA SET #1
B % 10 30 50 50 30
C % 10 25 40 35 20
DATA SET #2
Z % 30 30 30 30 32
DATA SET #3
B % 0 0 1 2 3
C % 1 2 3 0 0)OJ_OUT";
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
