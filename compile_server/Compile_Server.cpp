// 使用compile_run的方法，完成基于网络请求式的编译并运行服务

#include "Compile_Run.hpp"
#include "../comm/httplib.h"

using namespace ns_Compile_and_Run;
using namespace httplib;

// 编译服务随时可能被多个人请求，必须保证传递上来的代码形成源文件的时候，源文件名具有唯一性，否则多个用户间可能会相互影响
// ./compile_server 端口号
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "输入错误，你应该这么输入：./compile_server 端口号" << endl;
        return 1;
    }
    Server svr;
    // 客户端未来以post方式请求服务端
    // 传入参数：请求名称（如果http请求做文本匹配时，如果发现该名称，调用后面的函数），回调函数（此处为了方便，用lambda表达式设置，request和response详见头文件定义）
    svr.Post("/compile_and_run", [](const Request &req, Response &resp)
             {
                 // POST通过正文部分传递参数
                 string in_json = req.body; // 用户请求的服务正文是我们想要的json string
                 string out_json;
                 if (!in_json.empty())
                 {
                     CompileAndRun::Start(in_json, &out_json);       // 编译并运行
                     // 设置响应内容（即响应正文）：第一个参数是准备响应什么回去（字符串），第二个参数是响应文本的格式（content-type，可以查映射表)
                     resp.set_content(out_json, "application/json;charset=utf-8");
                 } });

    //  搭建tcp服务器，监听某IP地址上的所有网卡的8080端口，等待客户端传来数据。若接收到了客户端的请求数据，则服务端创建一个线程去处理这个请求。
    svr.listen("0.0.0.0", atoi(argv[1])); // 启动http服务
    return 0;
}

/*
    本地测试代码
    Json::Value in_value;
    string in_json;
    in_value["code"] = R"(#include<iostream>
        int main()
        {
            printf("你好\n");
            return 0;
        }
    )";
    in_value["input"] = "";
    in_value["cpu_limit"] = 1;
    in_value["mem_limit"] = 10240 * 3;
    Json::FastWriter writer;
    in_json = writer.write(in_value);

    string out_json;
    CompileAndRun::Start(in_json, &out_json);
    cout << out_json << endl;
    return 0;
*/