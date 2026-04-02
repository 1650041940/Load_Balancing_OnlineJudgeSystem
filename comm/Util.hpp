// 公共类文件
#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <atomic>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "boost/algorithm/string.hpp"

namespace ns_Util
{
    using namespace std;
    class TimeUtil
    {
    public:
        // 获取时间戳（秒级）
        static string GetTimeStamp()
        {
            struct timeval _time;
            gettimeofday(&_time, nullptr);
            return to_string(_time.tv_sec); // 累积到现在的秒数
        }
        // 获取时间戳（毫秒级）
        static string GetTimeMs()
        {
            struct timeval _time;
            gettimeofday(&_time, nullptr);
            return to_string(_time.tv_sec * 1000 + _time.tv_usec / 1000);
        }
    };

    const string temp_path = "./temp/"; // 公共的编译路径

    class PathUtil
    {
    public:
        // 添加路径，返回值：添加后的完整路径；传入参数：文件名，准备添加的后缀
        static string AddSuffix(const string &file_name, const string &suffix)
        {
            string path_name = temp_path;
            path_name += file_name;
            path_name += suffix;
            printf("path_name:%s\n", path_name.c_str());
            return path_name;
        }

        // 编译时所生成的的三个临时文件

        // 生成源文件的完整文件名（源文件所在路径+后缀）
        // eg：1234 -> ./temp/1234.cpp
        static string Src(const string &file_name)
        {
            return AddSuffix(file_name, ".cpp");
        }

        // 生成可执行程序的完整文件名（可执行程序所在路径+后缀）
        static string Exe(const string &file_name)
        {
            return AddSuffix(file_name, ".exe");
        }

        // 生成编译时错误的完整文件名（编译时错误所在路径+后缀）
        static string CompilerError(const string &file_name)
        {
            return AddSuffix(file_name, ".compile_error");
        }

        // 程序运行时所生成的三个临时文件

        // 生成标准输入的完整文件名（标准输入所在路径+后缀）
        static string Stdin(const string &file_name)
        {
            return AddSuffix(file_name, ".stdin");
        }
        // 生成标准输出的完整文件名（标准输出所在路径+后缀）
        static string Stdout(const string &file_name)
        {
            return AddSuffix(file_name, ".stdout");
        }
        // 生成标准错误的完整文件名（标准错误所在路径+后缀）
        static string Stderr(const string &file_name)
        {
            return AddSuffix(file_name, ".stderr");
        }
    };

    class FileUtil
    {
    public:
        // 判断是否编译成功
        static bool IsFileExists(const string &path_name)
        {
            struct stat st;

            // stat函数，获取某文件的信息，获取成功返回0，即说明有该文件，也就说明编译成功；否则返回-1
            if (stat(path_name.c_str(), &st) == 0)
            {
                // 获取文件属性成功，文件存在
                return true;
            }
            return false;
        }

        // 形成具有唯一性的文件名（没有路径与后缀）
        // 毫秒级时间戳和原子性递增：保证文件名唯一性
        static string UniqFileName()
        {
            static atomic_uint id(0); // atomic是C++提供的原子性计数器
            id++;
            string ms = TimeUtil::GetTimeMs();
            string uniq_id = to_string(id);
            return ms + "-" + uniq_id;
        }

        // 以用户代码形成源文件，再写入临时目录temp中
        // 传递参数：准备写入的目标文件，源代码
        static bool WriteFile(const string &target, const string &code)
        {
            printf("target:%s\n", target.c_str());
            ofstream out(target); // 以输出方式打开target, ofstream用于写文件操作
            if (!out.is_open())
            {
                return false;
            }
            out.write(code.c_str(), code.size());
            out.close();
            return true;
        }

        // 读取文件并返回
        // 传递参数：源文件，准备接收读取数据的文件，读取的数据是否换行
        static bool ReadFile(const std::string &target, std::string *content, bool keep = false)
        {
            (*content).clear();
            ifstream in(target); // 以输入方式打开target, ifstream用于读文件操作
            if (!in.is_open())
            {
                return false;
            }
            string line;
            // getline——按行读取，内部重载了强制类型转换，从in中读取，读到line中
            while (getline(in, line))
            {
                (*content) += line;
                // getline中并不保存行分隔符，但有时候需要\n
                (*content) += (keep ? "\n" : "");
            }
            in.close();
            return true;
        }
    };

    class StringUtil
    {
    public:
        /*
            str: 输入型参数，目标要切分的字符串
            res: 输出型，保存切分完毕的结果
            sep: 指定的分割符
         */
        static void SpiltStr(const string &str, vector<string> *res, string &sep)
        {
            // 采用boost库进行切割
            // 传入参数：切割后传给谁，切割源文件，分割符（is_any_of说明遇到这个分割符就切割），是否压缩（on表示压缩，即遇到多个分割符合并为1个；off表示不压缩，不合并）
            boost::split((*res), str, boost::is_any_of(sep), boost::algorithm::token_compress_on);
        }
    };
}