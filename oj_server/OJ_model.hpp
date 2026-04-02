// 通常是和数据交互的模块，例如对题库进行增删查改（文件版，MySQL版），对外提供访问数据的接口
// 根据题目list文件，加载所有的题目信息至内存中
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstdlib>
#include <cassert>

#include "../comm/Log.hpp"
#include "../comm/Util.hpp"

namespace ns_model
{
    using namespace std;
    using namespace ns_Log;
    using namespace ns_Util;

    struct Question
    {
        string number; // 题目的唯一编号
        string title;  // 题目的标题
        string star;   // 题目的难度——简单/中等/困难
        int cpu_limit; // 题目的时间限制（s)
        int mem_limit; // 题目的空间限制（KB）
        string desc;   // 题目的题干描述
        string header; // 题目预设给用户在线编辑器的代码
        string tail;   // 题目的测试用例，需要和header拼接，形成完整代码
    };

    const string questions_list = "./questions/questions.list";
    const string question_comm_path = "./questions/";

    class Model
    {
    public:
        Model()
        {
            assert(LoadQuestionList(questions_list)); // 判断是否加载了每个题目的unordered_map配置文件
        }

        bool LoadQuestionList(const string &questions_list)
        {
            // 加载每个题目的配置文件：根据传过来的questions_list找到所有题目列表（eg：./questions/questions.list），再根据其中内容找到特定编号的题目
            ifstream in(questions_list); // 打开传过来的题目表
            if (!in.is_open())
            {
                Log(FATAL) << " 加载题库失败，请检查是否存在题库文件！" << "\n";
                return false;
            }

            string line;
            string sep = " ";
            while (getline(in, line)) // 按行读取表中题目信息
            {
                vector<string> res;
                StringUtil::SpiltStr(line, &res, sep);
                if (res.size() != 5) // 信息个数不对
                {
                    Log(WARNING) << "加载部分题目失败，文件信息个数错误，请检查文件格式！" << "\n";
                    continue;
                }

                // 把数据放在一个单个的q中
                Question q;
                q.number = res[0];
                q.title = res[1];
                q.star = res[2];
                q.cpu_limit = std::stoi(res[3]);
                q.mem_limit = std::stoi(res[4]);

                string question_path = question_comm_path;
                question_path += q.number;
                question_path += "/";

                FileUtil::ReadFile(question_path + "desc.txt", &(q.desc), true);
                FileUtil::ReadFile(question_path + "header.cpp", &(q.header), true);
                FileUtil::ReadFile(question_path + "tail.cpp", &(q.tail), true);

                questions.insert({q.number, q});
            }
            Log(INFO) << "题库加载中……加载成功！" << "\n";
            in.close();
            return true;
        }

        bool GetAllQuestions(vector<Question> *out)
        {
            if (questions.size() == 0)
            {
                Log(ERROR) << "题库为空，获取题库失败！" << "\n";
                return false;
            }
            for (const auto &q : questions)
            {
                out->push_back(q.second); // first：key（题目编号） second：value（题目详细信息）
            }
            return true;
        }

        bool GetOneQuestion(const string &number, Question *q)
        {
            const auto &iter = questions.find(number);
            if (iter == questions.end())
            {
                // 找到最后也没找到该编号的题目
                Log(ERROR) << "未找到" << number << "号题目！" << "\n";
                return false;
            }
            (*q) = iter->second;
            return true;
        }

        ~Model() {}

    private:
        unordered_map<string, Question> questions; // 题目编号：对应题目详细信息
    };
}
