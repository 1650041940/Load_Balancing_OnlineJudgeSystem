// 主要实现运行功能的头文件
#pragma once

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "../comm/Log.hpp"
#include "../comm/Util.hpp"

namespace ns_Runner
{
    using namespace std;
    using namespace ns_Log;
    using namespace ns_Util;
    class Runner
    {
    public:
        Runner() {}
        ~Runner() {}

        // 提供设置进程占用资源大小的接口
        static void SetProLimit(int _cpu_limit, int _mem_limit)
        {
            struct rlimit cpu_limit;
            cpu_limit.rlim_cur = _cpu_limit;
            cpu_limit.rlim_max = RLIM_INFINITY;
            setrlimit(RLIMIT_CPU, &cpu_limit);

            struct rlimit mem_limit;
            mem_limit.rlim_cur = _mem_limit * 1024; // KB转字节
            mem_limit.rlim_max = RLIM_INFINITY;
            setrlimit(RLIMIT_AS, &mem_limit);
        }

        // 传入参数：文件名，程序运行时可以使用的最大CPU资源上限（cpu_limit)，程序运行时可以使用的最大内存大小（mem_limit, 传入KB）
        static int Run(const string &file_name, int cpu_limit, int mem_limit)
        {
            /*
                程序运行，有三种情况
                1、代码运行完毕，结果正确
                2、代码运行完毕，结果错误
                3、代码运行错误

                OJ系统需要考虑代码运行完毕后的结果是否正确吗？——不考虑，运行结果是否正确由测试用例决定！
                我们只需要考虑程序能否运行完毕

                一个程序在启动的时候，默认有三个文件被打开
                1、标准输入：OJ系统中不考虑
                2、标准输出：程序运行完成后输出的结果，保存在stdin文件中
                3、标准错误：运行时的错误信息，保存在stderr中
            */
            string _execute = PathUtil::Exe(file_name); // 可执行程序
            string _stdin = PathUtil::Stdin(file_name);
            string _stdout = PathUtil::Stdout(file_name);
            string _stderr = PathUtil::Stderr(file_name);

            // 打开标准输入，标准输入，标准错误
            umask(0);
            int _stdin_fd = open(_stdin.c_str(), O_CREAT | O_RDONLY, 0644);
            int _stdout_fd = open(_stdout.c_str(), O_CREAT | O_WRONLY, 0644);
            int _stderr_fd = open(_stderr.c_str(), O_CREAT | O_WRONLY, 0644);

            if (_stdin_fd < 0 || _stdout_fd < 0 || _stderr_fd < 0)
            {
                Log(ERROR) << "运行时打开标准文件失败！" << "\n";
                return -1;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                Log(ERROR) << "运行时创建子进程失败！" << "\n";
                close(_stdin_fd);
                close(_stdout_fd);
                close(_stderr_fd);
                return -2;
            }
            else if (pid == 0)
            {
                // 子进程，重定向标准文件以及程序替换处理任务
                dup2(_stdin_fd, 0);
                dup2(_stdout_fd, 1);
                dup2(_stderr_fd, 2);

                // 采用execl函数进行程序替换，不调用系统调用（调用系统调用使用execlp）
                SetProLimit(cpu_limit, mem_limit); // 可执行程序运行前，限制资源占用
                execl(_execute.c_str() /*我要执行谁*/, _execute.c_str() /*我想在命令行上如何执行该程序（此处是直接运行）*/, nullptr);
                exit(1);
            }
            else
            {
                // 父进程
                close(_stdin_fd);
                close(_stdout_fd);
                close(_stderr_fd);
                int status = 0; // 程序的运行状态
                pid_t res = waitpid(pid, &status, 0);
                /*
                    程序运行异常，一定会收到信号！

                    返回值 > 0：程序运行异常，退出时收到了信号，返回值就是对应的信号编号
                    返回值 = 0：程序运行正常结束，运行结果保存在了对应的临时文件中
                    返回值 < 0：内部错误
                */
                (void)res;
                Log(INFO) << "运行完毕，收到的信号：" << (status & 0x7F) << endl;
                return status & 0x7F; // 信号储存在0x7F中
            }
        }
    };
}
