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
    const string input = R"OJ_IN(NOW IS THE TIME FOR ALL GOOD MEN TO COME TO THE AID OF THEIR COUNTRY.)OJ_IN";
    const string expect = R"OJ_OUT(NW S TH M FR L GD C Y.)OJ_OUT";
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
