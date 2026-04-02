// 通常是和数据交互的模块，例如对题库进行增删查改（文件版，MySQL版），对外提供访问数据的接口
// MySQL版本题库
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
#include "include/mysql.h"

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

    const string oj_questions = "oj_questions";
    const string host = "127.0.0.1"; // IP地址
    const string user = "oj_client"; // MySQL用户
    const string passwd = "123456";  // 用户密码
    const string db = "oj";          // 数据库名称
    const int port = 3306;           // 端口号

    class Model
    {
    public:
        Model()
        {
        }
        // 查询数据库的核心代码
        // 参数：想要执行的sql语句
        bool QueryMysql(const string &sql, vector<Question> *out)
        {
            // 1、创建mysql句柄
            MYSQL *my = mysql_init(nullptr);

            // 2、连接数据库
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0))
            {
                Log(FATAL) << "连接数据库失败! " << mysql_error(my) << "\n";
                return false;
            }

            // 一定要设置该链接的编码格式, 要不然会出现乱码问题
            mysql_set_character_set(my, "utf8");

            Log(INFO) << "连接数据库成功!" << "\n";

            // 3、下发sql命令
            if (0 != mysql_query(my, sql.c_str()))
            {
                Log(WARNING) << sql << " sql命令下发失败!" << "\n";
                return false;
            }

            // 4、提取结果
            MYSQL_RES *res = mysql_store_result(my);

            // 5、分析结果
            int rows = mysql_num_rows(res);   // 获得行数量
            int cols = mysql_num_fields(res); // 获得列数量

            Question q;

            for (int i = 0; i < rows; i++)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                q.number = row[0];
                q.title = row[1];
                q.star = row[2];
                q.desc = row[3];
                q.header = row[4];
                q.tail = row[5];
                q.cpu_limit = atoi(row[6]);
                q.mem_limit = atoi(row[7]);

                out->push_back(q);
            }
            // 释放结果空间
            free(res);
            // 关闭mysql连接
            mysql_close(my);

            return true;
        }
        bool GetAllQuestions(vector<Question> *out)
        {
            string sql = "select * from ";
            sql += oj_questions;
            return QueryMysql(sql, out);
        }

        bool GetOneQuestion(const string &number, Question *q)
        {
            bool res = false;
            std::string sql = "select * from ";
            sql += oj_questions;
            sql += " where number=";
            sql += number;
            vector<Question> result;
            if (QueryMysql(sql, &result))
            {
                if (result.size() == 1)
                {
                    *q = result[0];
                    res = true;
                }
            }
            return res;
        }

        ~Model() {}
    };
}
