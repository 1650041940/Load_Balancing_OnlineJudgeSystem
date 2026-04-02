#pragma once

#include <iostream>
#include <string>

#include "Util.hpp"

namespace ns_Log
{
    using namespace std;
    using namespace ns_Util;

    enum
    {
        INFO,
        DEBUG,
        WARNING,
        // 后面两个属于比较严重的错误
        ERROR,
        FATAL
    };

    // 传入参数：日志等级，文件名，错误所在行
    // 想制作一个开放式日志——log(...) << "message"
    // 使用内联函数，减少函数跳转
    inline ostream &log(const string &level, const string &file_name, int line)
    {
        // 添加日志等级
        string message = "[";
        message += level;
        message += "]";

        // 添加文件名
        message += "[";
        message += file_name;
        message += "]";

        // 添加错误所在行
        message += "[";
        message += to_string(line);
        message += "]";

        // 添加时间戳
        message += "[";
        message += TimeUtil::GetTimeStamp();
        message += "]";

        // cout内部本质是包含缓冲区的
        cout << message; // 不要endl刷新缓冲区
        return cout;
    }

//__FILE__表示当前文件的路径和文件名, __LINE__表示当前行号
// #把宏参数变为一个字符串，开放式日志
#define Log(level) log(#level, __FILE__, __LINE__)
}