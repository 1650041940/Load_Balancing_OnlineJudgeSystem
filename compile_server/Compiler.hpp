// 主要进行代码编译服务的头文件
#pragma once

#include <iostream>
#include <unistd.h>
#include <string>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../comm/Util.hpp"
#include "../comm/Log.hpp"

namespace ns_Compiler
{
    using namespace std;
    using namespace ns_Util;
    using namespace ns_Log;
    class compiler
    {
    public:
        compiler()
        {
        }
        ~compiler()
        {
        }

        // 返回值：编译成功返回true，编译失败返回false
        // 传入参数：欲编译的文件名
        static bool complie(const string &file_name)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                Log(ERROR) << "内部错误，创建子进程失败！\n";
                return false;
            }
            else if (pid == 0)
            {
                umask(0); // 清零权限掩码，使后面stderr文件设置的权限准确
                int _stderr = open(PathUtil::CompilerError(file_name).c_str(), O_CREAT | O_WRONLY, 0644);
                if (_stderr < 0)
                {
                    Log(WARNING) << "警告！没有成功形成stderror标准错误文件！" << "\n";
                    exit(1); // exit(0)表示正常退出，exit(x)（x不为0）都表示异常退出
                }
                // 把标准错误从显示器重定向到写入文件中，如果g++编译失败，会把标准错误打印到我们重定向的_stderr中而不是显示器
                dup2(_stderr, 2);

                // 子进程：负责调用编译器，完成对代码的编译工作
                // 利用execlp函数进行程序替换，程序替换，并不影响进程的文件描述符表
                // g++ -o 目标文件（形成可执行程序） 源文件 -std=c++11
                execlp("g++", "g++", "-o", PathUtil::Exe(file_name).c_str(), PathUtil::Src(file_name).c_str(), "-D", "COMPILER_ONLINE", /*带上宏，取消条件编译*/ "-std=c++11", nullptr /*execlp程序替换结束后，最后一定要有nullpte*/);
                // 程序替换成功后，不会走到下面
                Log(ERROR) << "启动g++编译器失败，可能是参数错误！" << "\n";
                exit(2);
            }
            else
            {
                // 父进程：判断编译是否成功
                waitpid(pid, nullptr, 0);

                // 走到此处，说明子进程已经结束，判断编译是否成功
                if (FileUtil::IsFileExists(PathUtil::Exe(file_name)))
                {
                    Log(INFO) << PathUtil::Src(file_name) << "文件编译成功！" << "\n";
                    return true;
                }
            }
            Log(ERROR) << "编译失败，未形成可执行程序！" << "\n";
            return false;
        }

    private:
    };
}