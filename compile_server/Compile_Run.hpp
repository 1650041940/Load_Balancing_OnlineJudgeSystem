// 整合编译及运行的头文件，三个主要功能
// 1、适配用户请求，定制通信协议字段
// 2、正确的调用compile and run
// 3、形成唯一的文件名
#pragma once

#include "Compiler.hpp"
#include "Runner.hpp"
#include "../comm/Log.hpp"
#include "../comm/Util.hpp"

#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>

namespace ns_Compile_and_Run
{
    using namespace std;
    using namespace ns_Compiler;
    using namespace ns_Runner;
    using namespace ns_Log;
    using namespace ns_Util;

    class CompileAndRun
    {
    public:
        CompileAndRun() {}
        ~CompileAndRun() {}

        // 清理临时文件
        static void RemoveTempFile(const string &file_name)
        {
            // 清理文件的个数是不确定的，但是有哪些我们是知道的
            string _src = PathUtil::Src(file_name);
            if (FileUtil::IsFileExists(_src))
                unlink(_src.c_str());

            string _compiler_error = PathUtil::CompilerError(file_name);
            if (FileUtil::IsFileExists(_compiler_error))
                unlink(_compiler_error.c_str());

            string _execute = PathUtil::Exe(file_name);
            if (FileUtil::IsFileExists(_execute))
            {
                if (unlink(_execute.c_str()) != 0)
                {
                    Log(ERROR) << "Failed to delete file: " << _execute << ". Error: " << strerror(errno);
                }
            }

            string _stdin = PathUtil::Stdin(file_name);
            if (FileUtil::IsFileExists(_stdin))
                unlink(_stdin.c_str());

            string _stdout = PathUtil::Stdout(file_name);
            if (FileUtil::IsFileExists(_stdout))
                unlink(_stdout.c_str());

            string _stderr = PathUtil::Stderr(file_name);
            if (FileUtil::IsFileExists(_stderr))
                unlink(_stderr.c_str());
        }
        /*
            code > 0：进程收到信号，运行时异常导致崩溃
            code < 0：整个过程中其他非运行报错（如代码为空、编译报错等）
            code = 0：整个过程全部正常完成
        */
        // 根据传来的状态码发回请求结果
        static string CodeToDesc(int status_code, const string &file_name)
        {
            std::string desc;
            switch (status_code)
            {
            case 0:
                desc = "编译运行成功";
                break;
            case -1:
                desc = "提交的代码是空";
                break;
            case -2:
                desc = "未知错误";
                break;
            case -3:
                FileUtil::ReadFile(PathUtil::CompilerError(file_name), &desc, true); // 编译错误更想知道错误的原因
                break;
            case SIGABRT: // 6
                desc = "内存超过范围";
                break;
            case SIGFPE: // 8
                desc = "浮点数溢出";
                break;
            case SIGXCPU: // 24
                desc = "CPU使用超时";
                break;
            default:
                desc = "未知错误: " + std::to_string(status_code);
                break;
            }
            return desc;
        }
        /*
            输入：
            1、code：用户提交的代码
            2、input：用户给自己提交的代码所做的输入，不做处理
            3、cpu_limit：CPU运行时间限制
            4、mem_limit：内存限制，即空间限制

            输出（必填）：
            1、status：状态码
            2、reason：请求结果
            输出（选填）：
            1、stdout：用户程序运行完毕后的结果
            2、stderr：用户程序如果运行出错的错误结果
        */
        static void Start(const string &in_json, string *out_json)
        {
            // 1、把输入来的字符串反序列化
            Json::Value in_value;
            Json::Reader reader;
            reader.parse(in_json, in_value); // 把in_json反序列化成in_value

            string code = in_value["code"].asString();
            string input = in_value["input"].asString();
            int cpu_limit = in_value["cpu_limit"].asInt();
            int mem_limit = in_value["mem_limit"].asInt();

            int status_code = 0; // 程序整个过程的结果码
            int run_res = 0;     // 程序运行的结果
            Json::Value out_value;
            string file_name; // 唯一性的文件名（没有路径与后缀）

            if (code.size() == 0)
            {
                printf("代码为空!!\n");
                status_code = -1; // 代码为空
                goto END;
            }

            // 形成具有唯一性的文件名（没有路径与后缀），唯一性通过毫秒级时间戳和原子性递增唯一值来决定）
            file_name = FileUtil::UniqFileName();

            // 把代码写入到一个cpp文件中，以便后面编译和运行
            if (!FileUtil::WriteFile(PathUtil::Src(file_name), code))
            {
                printf("写入失败！！\n");
                status_code = -2; // 未知错误
                goto END;
            }

            // 编译
            if (!compiler::complie(file_name))
            {
                status_code = -3; // 编译时出错
                goto END;
            }

            // 运行
            status_code = Runner::Run(file_name, cpu_limit, mem_limit);

        // 所有的处理都在END之后
        END:
            out_value["status"] = status_code;
            out_value["reason"] = CodeToDesc(status_code, file_name);
            if (status_code == 0)
            {
                // 程序全过程全部成功
                string _stdout;
                FileUtil::ReadFile(PathUtil::Stdout(file_name), &_stdout, true);
                out_value["stdout"] = _stdout;

                string _stderr;
                FileUtil::ReadFile(PathUtil::Stderr(file_name), &_stderr, true);
                out_value["stderr"] = _stderr;
            }
            // printf("status_code:%d\n", status_code);

            // 执行全部完毕，序列化
            Json::StyledWriter writer; // 序列化成多行的字符串
            *out_json = writer.write(out_value);

            RemoveTempFile(file_name); // 清理文件
        }
    };
};