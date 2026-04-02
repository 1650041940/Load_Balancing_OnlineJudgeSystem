#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "../comm/Util.hpp"

namespace ns_Log
{
    using namespace std;
    using namespace ns_Util;

    enum LogLevel
    {
        INFO,
        DEBUG,
        WARNING,
        ERROR,
        FATAL
    };

    // 全局的文件流对象
    static ofstream g_log_file;

    // 初始化日志文件
    void InitLogFile()
    {
        const string filename = "/home/wyj/负载均衡在线OJ项目/oj_server/oj_server.log";
        if (g_log_file.is_open())
            g_log_file.close();

        g_log_file.open(filename, ios::out | ios::app);
        if (!g_log_file.is_open())
        {
            cerr << "Failed to open log file: " << filename << endl;
        }
    }

    // 静态成员变量，用于跟踪日志文件是否已经被初始化
    static bool g_log_file_initialized = false;

    // 内联函数，用于构建日志消息
    inline ostream &log(const string &level, const string &file_name, int line)
    {
        if (!g_log_file_initialized)
        {
            InitLogFile();
            g_log_file_initialized = true;
        }

        // 使用stringstream来构建完整的消息
        ostringstream ss;
        ss << "[" << level << "]" << "[" << file_name << "]" << "[" << line << "]" << "[" << TimeUtil::GetTimeStamp() << "]: ";

        // 输出到文件
        g_log_file << ss.str();

        return g_log_file;
    }

// 定义 Log 宏
#define Log(level) log(#level, __FILE__, __LINE__)
}