C++项目——负载均衡在线OJ系统
C++项目——负载均衡在线OJ系统
linux
linux
已关注
收录于 · linux服务器开发
5 人赞同了该文章
一、项目前言
本项目旨在模仿牛客、力扣等在线OJ平台，通过前后端结合的方式，构建出一个简易的负载均衡式的在线OJ系统。

前端构建出一个在线OJ题目练习网页，通过读取远端数据库的方式构建出题库（题库包含文件版和MySQL版本两个版本）；点击题目，可以跳转至代码输入页面，用户输入代码完毕，点击网页“运行”按钮，代码会交由后端CentOS 7云服务器，负载均衡地挑选主机进行编译运行代码，并将结果返回前端页面。

（PS：在后期的优化扩展中，笔者将客户服务端进程oj_server和编译运行服务端compile_server都进行了守护进程化，一旦用户启动，将一直在后台运行，除非客户主动关闭它，更符合实际情况）

项目运行步骤
1、运行oj_server可执行文件，Xshell云服务器后端加载前端网页和读取题库

运行后，日志文件显示成功加载题库

localhost:8080 前端首页

在前端首页点击“点击我开始编程” 进入题库界面

点击想要联系的题目标题 进入题目练习主页面
2、以 ./compile_server + 端口号，运行用来提供编译服务的“主机”，不同的端口号，象征着不同的主机（这些端口号可以在 oj_server / conf / service_machine.conf 中添加，后文会进行介绍）

本次注册了三个主机

后台同时运转三个主机

多次点击“提交”，从日志文件可以看出，后端负载均衡地选择了三个主机中的一台，不是把编译任务一直交给某一台“主机

二、全项目核心思路

本项目的核心思路可以由一句话总结：

先运行 oj_server，用来提供前端页面（前端页面向后端发送http请求），后端运行多个 compile_server 编译主机，oj_server 接收到请求后把请求再转发给 compile_server，compile_server经过编译运行后把结果响应回去，最终反映到前端页面上。
本项目四个核心模块构成：

1、公共模块：存放各个模块都能用到的公共文件。

2、编译与运行模块（可视为服务端）：适配用户请求，负责完成基于网络请求式的编译并运行服务。

3、OJ相关模块（可视为客户端）：完成连接数据库、获取题目列表，查看题目、编写题目界面，负载均衡等后端核心业务逻辑。

4、项目发布模块：只包含整合后的二进制可执行编译运行文件与OJ系统网页文件，专供用户使用。

三、公共模块
公共模块由三个文件组成：

1、httplib.h
引入的cpp-httplib第三方开源网络库，这个C++库提供了简单且高效的方法来创建HTTP服务器和客户端，使得开发人员能够更加轻松地构建Web应用程序、微服务和网络连接的应用。

2、Log.hpp
编写的开放式日志头文件，包含后，可以使用Log(日志等级) << "日志具体信息"来把程序每一步运行的结果显示在屏幕中，便于程序员观察。

（PS：由于两个进程都进行了守护进程化，日志不会再显示在屏幕上，而是输出到各自的日志文件中，这个Log.hpp废弃不再使用，贴出代码，仅供参考）

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
        return cout;     // 返回cout类型的引用
    }

//**FILE**表示当前文件的路径和文件名, **LINE**表示当前行号
// #把宏参数变为一个字符串，开放式日志
#define Log(level) log(#level, **FILE**, **LINE**)
}

日志显示效果

3、Util.hpp
编写的在后续模块中要用到的各类实用函数的头文件，会在后文多次使用。

（PS：为了让诸位能了解全项目的整体思路，笔者在此不贴出全代码，而是随着博客思路的介绍，在需要添加一些公共类的时候才写出，也是为了防止代码过长造成文章冗余，各位若想直接看全代码，可移步至笔者 Gitee 代码仓库一观）

linux c/c++全栈开发学习地址
xxetb.xet.tech/s/2L80ej
需要C/C++ Linux服务器架构师学习资料加qun812855908获取（资料包括C/C++，Linux，golang技术，Nginx，ZeroMQ，MySQL，Redis，fastdfs，MongoDB，ZK，流媒体，CDN，P2P，K8S，Docker，TCP/IP，协程，DPDK，ffmpeg等），免费分享

四、编译与运行模块（compile_server）

编译与运行模块作为对用户提交的代码进行编译和运行并反馈结果的模块，是服务端是本项目最核心的一部分，用户提交的代码能否正常运转都要仰仗此模块。

整体思路如下：

0、编译运行服务端日志模块
由于日志需要添加时间戳，而compile_server 和 oj_server 由于守护进程的原因又导致公用日志模块的废弃，所以我们需要在公共类头文件 Util.hpp 中添加第一个公共类：时间戳类 TimeUtil。

公共类 1：时间戳类 TimeUtil
class TimeUtil
{
public:
// 获取时间戳（秒级）
static string GetTimeStamp()
{
struct timeval \_time;
gettimeofday(&\_time, nullptr);
return to_string(\_time.tv_sec); // 累积到现在的秒数
}

        // 获取时间戳（毫秒级）
        static string GetTimeMs()
        {
            struct timeval _time;
            gettimeofday(&_time, nullptr);
            return to_string(_time.tv_sec * 1000 + _time.tv_usec / 1000);
        }

};
模块思路
该模块的思路大致与普通的日志模块相同，只是返回的不再是cout标准输出，而是利用一个 static ofstream 全局文件流对象，把日志写入到具体文件中。

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
        const string filename = "/home/wyj/负载均衡在线OJ项目/compile_server/compile_server.log";
        if (g_log_file.is_open())
            g_log_file.close();

        // 打开filename的文件，ios::out 是一个标志位，表示文件将以输出模式打开，即你可以向文件写入数据。
        // ios::app 也是一个标志位，表示如果文件已存在，则将文件指针定位到文件的末尾。这意味着任何新的写入操作都会发生在文件的末尾，而不是覆盖文件现有的内容。
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
#define Log(level) log(#level, **FILE**, **LINE**)

    // 使用示例:
    // Log(INFO) << "This is an info message." << endl;

}
1、建立 temp 文件夹保存临时文件
因为用户提交的代码实际是以文本形式传入后端的（由服务器序列化反序列化机制决定的），所以编译和运行模块一定要把文本文件其加上对应后缀，生成cpp源代码文件，同时在编译完毕后，也要生成exe可执行文件、stderr错误文件等各种临时文件，这些文件都需要建立一个temp文件夹进行保存，并在整个模块运行完毕后进行及时清除。

这个时候我们也就需要实现第2个公共类：添加文件后缀类：PathUtil

公共类2:：文件后缀类 PathUtil
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

2、Compiler.hpp：主要提供代码编译服务
编译代码，有可能编译成功，也有可能编译失败，所以我们需要两个进程，子进程负责利用execlp函数，调用编译器进行编译 cpp 文件；父进程负责判断编译的结果，只需要判断是否生成了 exe 可执行文件即可。
判断是否生成 exe 可执行文件，就需要实现一个公共函数：IsFileExists
公共函数 1：static bool IsFileExists(const string &path_name)

只需要利用 stat 函数查找是否存在 exe 后缀的同名文件即可：

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

2.1 Compiler.hpp 源代码：
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
{}
~compiler()
{}

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
                execlp("g++", "g++", "-o", PathUtil::Exe(file_name).c_str(), PathUtil::Src(file_name).c_str(), "-D", "COMPILER_ONLINE", /*带上宏，取消条件编译*/ "-std=c++11", nullptr /*execlp程序替换结束后，最后一定要有nullptr*/);
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
由于要进行编译，同时判断编译是否成功，所以利用 fork() 函数创建了子进程，父子进程分工完成编译服务。

子进程：负责调用编译器，完成对代码的编译工作。（利用execlp函数进行进程替换，先调用Util.hpp中的添加cpp后缀）

//传入参数：即将执行的新程序名 命令行的具体执行过程—— g++ -o 目标文件（形成可执行程序） 源文件 -std=c++11
execlp("g++", "g++", "-o", PathUtil::Exe(file_name).c_str()，PathUtil::Src(file_name).c_str(), "-D", "COMPILER_ONLINE", /_带上宏，取消条件编译_/ "-std=c++11", nullptr /_execlp程序替换结束后，最后一定要有nullpte_/);
父进程：判断编译是否成功。通过调用stat函数（在Util.hpp中进行调用简化代码结构），传入文件名+exe后缀来寻找temp文件夹路径下是否存在exe可执行文件，如果存在，说明cpp编译成功。

//传入参数：​文件路径（文件名），struct stat 类型的结构体
int stat(const char *path, struct stat *buf)
3、Runner.hpp：主要提供代码运行服务
在 Compiler.hpp 编译成功生成可执行文件后，自然需要去运行它

3.1 限制资源占用接口
首先，根据我们平常做OJ题的经验，一个正常的OJ判题系统，一定是会有对代码的时间复杂度和

空间复杂度的限制的，体现在后端代码上，就是限制进程的资源占用，即程序运行时可以使用的最大CPU资源上限（cpu_limit)，程序运行时可以使用的最大内存大小（mem_limit, 传入KB）。

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

3.2 运行函数Run
一个程序在启动的时候，会默认有三个文件被打开：

1、标准输入：OJ系统中不考虑

2、标准输出：程序运行完成后输出的结果，保存在stdin文件中

3、标准错误：运行时的错误信息，保存在stderr中

所以我们也要打开它

然后，运行模块并不需要关心程序的运行结果是否正确，这由题目的测试用例决定；我们只需要关心，程序是否能成功运行。

所以仍然是两个进程，子进程运行可执行文件，父进程通过传来的子进程信号来判断程序是否能成功运行。

程序只要运行完毕，一定会收到信号！

返回值 > 0：程序运行异常，退出时收到了信号，返回值就是对应的信号编号

返回值 = 0：程序运行正常结束，运行结果保存在了对应的临时文件中

返回值 < 0：内部错误

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
                Log(ERROR) << "运行时打开标准文件失败！" << endl;
                return -1;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                Log(ERROR) << "运行时创建子进程失败！" << endl;
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
                waitpid(pid, &status, 0);
                /*
                    程序运行异常，一定会收到信号！
                    返回值 > 0：程序运行异常，退出时收到了信号，返回值就是对应的信号编号
                    返回值 = 0：程序运行正常结束，运行结果保存在了对应的临时文件中
                    返回值 < 0：内部错误
                */
                Log(INFO) << "运行完毕，收到的信号：" << (status & 0x7F) << endl;
                return status & 0x7F; // 信号储存在0x7F中
            }

代码运行一定会调用Exe文件，同时一个程序在启动的时候，默认有三个文件被打开——标准输入、标准输出、标准错误，在Run主函数中一定要加上stdin，stdout，stderr后缀，并打开他们。

仍然与上面一样，需要创建父子进程分工：

子进程：调用exclp函数进行进程替换，前文的Compiler.cpp中，已经确保有Exe文件形成，因此直接运行即可（值得注意的是，可执行程序运行前，应该限制一个程序的资源占用）

execl(\_execute.c_str() /_我要执行谁_/, \_execute.c_str() /_我想在命令行上如何执行该程序（此处是直接运行）_/, nullptr);
父进程：子进程运行完毕，一定会收到信号，通过检测等待子进程的信号，来检测判断程序的运行结果。

4、Compile_Run.hpp：整合编译运行，提供外部接口，一键启动编译及运行
本头文件有三个主要功能：适配用户请求，定制通信协议字段；正确的调用compile and run；形成唯一的文件名。

4.1 技术选型——Json库
http 协议的请求和响应都是结构体字段，但是结构体字段在不同平台和不同语言上传输很容易出问题，为了解决这个问题，我们需要在服务器应用层把结构体字段序列化成字节流（类似字符串），层层传输到客户端，客户端再把字节流进行反序列化，解析出http的报头和正文。

序列化和反序列化可以我们自己实现，但是通常情况下都是引入成熟的序列化反序列化方案，Json 就是其中的一种：

Json 是一种最常见的序列化反序列化方案，其最大的特点是允许采用 {“key”, "value"}来构建一系列键值对，多对键值对之间用逗号分割，把结构体字段的信息利用键值对的方式组织起来，给每一段信息取个名字 key，通过key 找到对应的信息。
4.2 start函数：一键启动编译及运行
4.2.1 首先我们需要把oj_server传来的已经序列化完成的http请求正文反序列化成结构化字段：
1、code：用户提交的代码

2、input：用户给自己提交的代码所做的输入，不做处理

3、cpu_limit：CPU运行时间限制

4、mem_limit：内存限制，即空间限制

解析完后，这就是一次编译运行了，我们要为那些临时文件给个唯一的名字，不能没有名字，而时间戳就有唯一性，所以我们实现第2个公共函数——生成唯一文件名函数 UniqFileName()

公共函数2：生成唯一文件名函数 UniqFileName()

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

4.2.2 随后，我们把代码写入到一个cpp文件中，以便后面编译和运行，实现第3个公共函数——写入文件函数 WriteFile
公共函数3：写入文件函数 static bool WriteFile(const string &target, const string &code)

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

4.2.3 运行，处理，把最终结果按照键值对写好，序列化成字节流，作为htttp协议的响应正文，返回（不要忘了清理临时文件）：
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
                unlink(_execute.c_str());

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
                desc = "未知: " + std::to_string(status_code);
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
            run_res = Runner::Run(file_name, cpu_limit, mem_limit);
            if (run_res < 0)
            {
                status_code = -2; // 未知错误
            }
            else if (run_res > 0)
            {
                status_code = -4; // 程序运行崩溃
            }
            else
            {
                // 运行成功
                status_code = 0;
            }

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
            printf("status_code:%d", status_code);

            // 执行全部完毕，序列化
            Json::StyledWriter writer; // 序列化成多行的字符串
            *out_json = writer.write(out_value);

            RemoveTempFile(file_name); // 清理文件
        }
    };

};
5、Compile_Server.cpp：完成基于网络请求式的编译并运行服务
在打包给用户的可执行程序中，就会有由Compile_Server.cpp编译产生的compile_server可执行文件，用户通过“./compile_server 端口号”命令的方式，便可以在自己的云服务器后端启动编译运行服务端，不同的端口号，视作启动了不同的编译主机。

用户在输入完代码后，oj_server 端（后文介绍）会负载均衡式地挑选一个编译主机，把前端的 http 请求（方法为post）转发过来；

我们 httplib 类的服务类 svr 会收到请求，通过 post 函数进行处理，告诉服务器如何处理特定路径的请求，把请求的正文进行剥离，交给编译运行模块，编译运行模块返回序列化后的结果（包含状态码和请求的结果）作为响应正文。最后，设置响应正文。

一切就绪后，启动http网络服务开始监听客户端。（在所有路由注册完成后，才能启动服务器开始监听客户端的连接请求。如果在注册路由之前启动监听，服务器将无法正确处理请求，因为它还不知道如何处理这些请求。）

// 使用compile_run的方法，完成基于网络请求式的编译并运行服务

#include "Compile_Run.hpp"
#include "../comm/httplib.h"

using namespace ns_Compile_and_Run;
using namespace httplib;

// 编译服务随时可能被多个人请求，必须保证传递上来的代码形成源文件的时候，源文件名具有唯一性，否则多个用户间可能会相互影响
// ./compile_server 端口号
int main(int argc, char \*argv[])
{
if (argc != 2)
{
cerr << "输入错误，你应该这么输入：./compile_server 端口号" << endl;
return 1;
}
Server svr;
// 客户端未来以post方式请求服务端,所以响应也要用post方法
// 传入参数：请求名称（如果http请求做文本匹配时，如果发现该名称，调用后面的函数），回调函数（此处为了方便，用lambda表达式设置，request和response详见头文件定义）
svr.Post("/compile_and_run", [](const Request &req, Response &resp)
{
// POST通过正文部分传递参数
string in_json = req.body; // 用户请求的服务正文是我们想要的json string
string out_json;
if (!in_json.empty())
{
CompileAndRun::Start(in_json, &out_json); // 编译并运行
// 设置响应内容（即响应正文）：第一个参数是准备响应什么回去（字符串），第二个参数是响应文本的格式（content-type，可以查映射表)
resp.set_content(out_json, "application/json;charset=utf-8");
} });

    //  搭建tcp服务器，监听某IP地址上的所有网卡的8080端口，等待客户端传来数据。若接收到了客户端的请求数据，则服务端创建一个线程去处理这个请求。
    svr.listen("0.0.0.0", atoi(argv[1])); // 启动http服务
    return 0;

}
五、OJ_server（客户服务端）
OJ模块可以勉强算作客户服务端，是本项目最复杂的一部分，它集合了前后端、数据库等多个方面，整体编写思路（题库此处用数据库版）如下：

0、客户服务端日志模块
整体思路与编译运行服务端一致，仅写入的文件的路径不同

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
#define Log(level) log(#level, **FILE**, **LINE**)

    // 使用示例:
    // Log(INFO) << "This is an info message." << endl;

}
1、提前引入MySQL的头文件以及MySQL的动静态库为后续题库的加载做准备。

2、建立service_machine.conf配置文件

其中储存可以提供OJ服务的主机的套接字（IP地址+端口号，不同套接字，象征着不同的主机），为后续负载均衡地选择主机做准备。

3、建立题库
预先安装好云服务器的MySQL数据库，在数据库中创建可以远程登录的MySQL用户，同时为其赋权。

create user oj_client@'localhost' indentified by 'password';
grant all on oj.\* to oj_client@'localhost';
使用NavicatPremium 16连接远端云服务器的MySQL数据库，设计题库的表结构。

CREATE TABLE `oj_questions` (
`number` INT AUTO_INCREMENT PRIMARY KEY COMMENT '题目编号',
`title` VARCHAR(128) NOT NULL COMMENT '题目标题',
`star` VARCHAR(8) NOT NULL COMMENT '题目难度',
`desc` TEXT NOT NULL COMMENT '题目描述',
`header` TEXT NOT NULL COMMENT '对应题目预设给用户看的代码',
`tail` TEXT NOT NULL COMMENT '对应题目的测试用例代码',
`cpu_limit` INT DEFAULT 1 COMMENT '对应题目的最大时间复杂度',
`mem_limit` INT DEFAULT 50000 COMMENT '对应题目的最大空间复杂度'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
最后用文本方式写入题目的题干，测试用例，示范代码等。

4、编写前端网页
作为后端开发者，本项目重点不在于此，故不再多做赘述，把三个网页的html源码贴出，供参考。

4.1 OJ系统首页 index.html

<!DOCTYPE html>
<html lang="en">
 
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>我的个人OJ系统</title>
    <style>
        /* 起手式, 100%保证我们的样式设置可以不受默认影响 */
        * {
            /* 消除网页的默认外边距 */
            margin: 0px;
            /* 消除网页的默认内边距 */
            padding: 0px;
        }
 
        html,
        body {
            width: 100%;
            height: 100%;
        }
 
        .container .navbar {
            width: 100%;
            height: 50px;
            background-color: black;
            /* 给父级标签设置overflow，取消后续float带来的影响 */
            overflow: hidden;
        }
 
        .container .navbar a {
            /* 设置a标签是行内块元素，允许你设置宽度 */
            display: inline-block;
            /* 设置a标签的宽度,a标签默认行内元素，无法设置宽度 */
            width: 80px;
            /* 设置字体颜色 */
            color: white;
            /* 设置字体的大小 */
            font-size: large;
            /* 设置文字的高度和导航栏一样的高度 */
            line-height: 50px;
            /* 去掉a标签的下划线 */
            text-decoration: none;
            /* 设置a标签中的文字居中 */
            text-align: center;
        }
 
        /* 设置鼠标事件 */
        .container .navbar a:hover {
            background-color: green;
        }
 
        .container .navbar .login {
            float: right;
        }
 
        .container .content {
            /* 设置标签的宽度 */
            width: 800px;
            /* 用来调试 */
            /* background-color: #ccc; */
            /* 整体居中 */
            margin: 0px auto;
            /* 设置文字居中 */
            text-align: center;
            /* 设置上外边距 */
            margin-top: 200px;
        }
 
        .container .content .font_ {
            /* 设置标签为块级元素，独占一行，可以设置高度宽度等属性 */
            display: block;
            /* 设置每个文字的上外边距 */
            margin-top: 20px;
            /* 去掉a标签的下划线 */
            text-decoration: none;
            /* 设置字体大小
            font-size: larger; */
        }
    </style>
</head>
 
<body>
    <div class="container">
        <!-- 导航栏， 功能不实现-->
        <div class="navbar">
            <a href="/">首页</a>
            <a href="/all_questions">题库</a>
            <a href="#">竞赛</a>
            <a href="#">讨论</a>
            <a href="#">求职</a>
            <a class="login" href="#">登录</a>
        </div>
        <!-- 网页的内容 -->
        <div class="content">
            <h1 class="font_">欢迎来到我的OnlineJudge平台</h1>
            <p class="font_">这是我个人独立开发的一个在线OJ平台</p>
            <a class="font_" href="/all_questions">点击我开始编程</a>
        </div>
    </div>
</body>
 
</html>
4.2 题库网页 all_questions.html
<!DOCTYPE html>
<html lang="en">
 
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>在线OJ-题目列表</title>
    <style>
        /* 起手式, 100%保证我们的样式设置可以不受默认影响 */
        * {
            /* 消除网页的默认外边距 */
            margin: 0px;
            /* 消除网页的默认内边距 */
            padding: 0px;
        }
 
        html,
        body {
            width: 100%;
            height: 100%;
        }
 
        .container .navbar {
            width: 100%;
            height: 50px;
            background-color: black;
            /* 给父级标签设置overflow，取消后续float带来的影响 */
            overflow: hidden;
        }
 
        .container .navbar a {
            /* 设置a标签是行内块元素，允许你设置宽度 */
            display: inline-block;
            /* 设置a标签的宽度,a标签默认行内元素，无法设置宽度 */
            width: 80px;
            /* 设置字体颜色 */
            color: white;
            /* 设置字体的大小 */
            font-size: large;
            /* 设置文字的高度和导航栏一样的高度 */
            line-height: 50px;
            /* 去掉a标签的下划线 */
            text-decoration: none;
            /* 设置a标签中的文字居中 */
            text-align: center;
        }
 
        /* 设置鼠标事件 */
        .container .navbar a:hover {
            background-color: green;
        }
 
        .container .navbar .login {
            float: right;
        }
 
        .container .question_list {
            padding-top: 50px;
            width: 800px;
            height: 100%;
            margin: 0px auto;
            /* background-color: #ccc; */
            text-align: center;
        }
 
        .container .question_list table {
            width: 100%;
            font-size: large;
            font-family: 'Lucida Sans', 'Lucida Sans Regular', 'Lucida Grande', 'Lucida Sans Unicode', Geneva, Verdana, sans-serif;
            margin-top: 50px;
            background-color: rgb(243, 248, 246);
        }
 
        .container .question_list h1 {
            color: green;
        }
 
        .container .question_list table .item {
            width: 100px;
            height: 40px;
            font-size: large;
            font-family: 'Times New Roman', Times, serif;
        }
 
        .container .question_list table .item a {
            text-decoration: none;
            color: black;
        }
 
        .container .question_list table .item a:hover {
            color: blue;
            text-decoration: underline;
        }
 
        .container .footer {
            width: 100%;
            height: 50px;
            text-align: center;
            line-height: 50px;
            color: #ccc;
            margin-top: 15px;
        }
    </style>
</head>
 
<body>
    <div class="container">
        <!-- 导航栏， 功能不实现-->
        <div class="navbar">
            <a href="/">首页</a>
            <a href="/all_questions">题库</a>
            <a href="#">竞赛</a>
            <a href="#">讨论</a>
            <a href="#">求职</a>
            <a class="login" href="#">登录</a>
        </div>
        <div class="question_list">
            <h1>OnlineJuge题目列表</h1>
            <table>
                <tr>
                    <th class="item">编号</th>
                    <th class="item">标题</th>
                    <th class="item">难度</th>
                </tr>
                {{#question_list}}
                    <tr>
                        <td class="item">{{number}}</td>
                        <td class="item"><a href="/question/{{number}}">{{title}}</a></td>
                        <td class="item">{{star}}</td>
                    </tr>
                {{/question_list}}
            </table>
        </div>
        <div class="footer">
            <!-- <hr> -->
            <h4>@东方未明0108</h4>
        </div>
    </div>
 
</body>
 
</html>
4.3 题目练习主页面 one_question.html
<!DOCTYPE html>
<html lang="en">
 
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{{number}}.{{title}}</title>
    <!-- 引入ACE插件 -->
    <!-- 官网链接：https://ace.c9.io/ -->
    <!-- CDN链接：https://cdnjs.com/libraries/ace -->
    <!-- 使用介绍：https://www.iteye.com/blog/ybc77107-2296261 -->
    <!-- https://justcode.ikeepstudying.com/2016/05/ace-editor-%E5%9C%A8%E7%BA%BF%E4%BB%A3%E7%A0%81%E7%BC%96%E8%BE%91%E6%9E%81%E5%85%B6%E9%AB%98%E4%BA%AE/ -->
    <!-- 引入ACE CDN -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.2.6/ace.js" type="text/javascript"
        charset="utf-8"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.2.6/ext-language_tools.js" type="text/javascript"
        charset="utf-8"></script>
    <!-- 引入jquery CDN -->
    <script src="http://code.jquery.com/jquery-2.1.1.min.js"></script>
 
    <style>
        * {
            margin: 0;
            padding: 0;
        }
 
        html,
        body {
            width: 100%;
            height: 100%;
        }
 
        .container .navbar {
            width: 100%;
            height: 50px;
            background-color: black;
            /* 给父级标签设置overflow，取消后续float带来的影响 */
            overflow: hidden;
        }
 
        .container .navbar a {
            /* 设置a标签是行内块元素，允许你设置宽度 */
            display: inline-block;
            /* 设置a标签的宽度,a标签默认行内元素，无法设置宽度 */
            width: 80px;
            /* 设置字体颜色 */
            color: white;
            /* 设置字体的大小 */
            font-size: large;
            /* 设置文字的高度和导航栏一样的高度 */
            line-height: 50px;
            /* 去掉a标签的下划线 */
            text-decoration: none;
            /* 设置a标签中的文字居中 */
            text-align: center;
        }
 
        /* 设置鼠标事件 */
        .container .navbar a:hover {
            background-color: green;
        }
 
        .container .navbar .login {
            float: right;
        }
 
        .container .part1 {
            width: 100%;
            height: 600px;
            overflow: hidden;
        }
 
        .container .part1 .left_desc {
            width: 50%;
            height: 600px;
            float: left;
            overflow: scroll;
        }
 
        .container .part1 .left_desc h3 {
            padding-top: 10px;
            padding-left: 10px;
        }
 
        .container .part1 .left_desc pre {
            padding-top: 10px;
            padding-left: 10px;
            font-size: medium;
            font-family: 'Gill Sans', 'Gill Sans MT', Calibri, 'Trebuchet MS', sans-serif;
        }
 
        .container .part1 .right_code {
            width: 50%;
            float: right;
        }
 
        .container .part1 .right_code .ace_editor {
            height: 600px;
        }
 
        .container .part2 {
            width: 100%;
            overflow: hidden;
        }
 
        .container .part2 .result {
            width: 300px;
            float: left;
        }
 
        .container .part2 .btn-submit {
            width: 120px;
            height: 50px;
            font-size: large;
            float: right;
            background-color: #26bb9c;
            color: #FFF;
            /* 给按钮带上圆角 */
            /* border-radius: 1ch; */
            border: 0px;
            margin-top: 10px;
            margin-right: 10px;
        }
 
        .container .part2 button:hover {
            color: green;
        }
 
        .container .part2 .result {
            margin-top: 15px;
            margin-left: 15px;
        }
 
        .container .part2 .result pre {
            font-size: large;
        }
    </style>
</head>
 
<body>
    <div class="container">
        <!-- 导航栏， 功能不实现-->
        <div class="navbar">
            <a href="/">首页</a>
            <a href="/all_questions">题库</a>
            <a href="#">竞赛</a>
            <a href="#">讨论</a>
            <a href="#">求职</a>
            <a class="login" href="#">登录</a>
        </div>
        <!-- 左右呈现，题目描述和预设代码 -->
        <div class="part1">
            <div class="left_desc">
                <h3><span id="number">{{number}}</span>.{{title}}_{{star}}</h3>
                <pre>{{desc}}</pre>
            </div>
            <div class="right_code">
                <pre id="code" class="ace_editor"><textarea class="ace_text-input">{{pre_code}}</textarea></pre>
            </div>
        </div>
        <!-- 提交并且得到结果，并显示 -->
        <div class="part2">
            <div class="result"></div>
            <button class="btn-submit" onclick="submit()">提交代码</button>
        </div>
    </div>
    <script>
        //初始化对象
        editor = ace.edit("code");
 
        //设置风格和语言（更多风格和语言，请到github上相应目录查看）
        // 主题大全：http://www.manongjc.com/detail/25-cfpdrwkkivkikmk.html
        editor.setTheme("ace/theme/monokai");
        editor.session.setMode("ace/mode/c_cpp");
 
        // 字体大小
        editor.setFontSize(16);
        // 设置默认制表符的大小:
        editor.getSession().setTabSize(4);
 
        // 设置只读（true时只读，用于展示代码）
        editor.setReadOnly(false);
 
        // 启用提示菜单
        ace.require("ace/ext/language_tools");
        editor.setOptions({
            enableBasicAutocompletion: true,
            enableSnippets: true,
            enableLiveAutocompletion: true
        });
 
        function submit() {
            // alert("嘿嘿!");
            // 1. 收集当前页面的有关数据, 1. 题号 2.代码
            var code = editor.getSession().getValue();
            // console.log(code);
            var number = $(".container .part1 .left_desc h3 #number").text();
            // console.log(number);
            var judge_url = "/judge/" + number;
            // console.log(judge_url);
            // 2. 构建json，并通过ajax向后台发起基于http的json请求
            $.ajax({
                method: 'Post',   // 向后端发起请求的方式
                url: judge_url,   // 向后端指定的url发起请求
                dataType: 'json', // 告知server，我需要什么格式
                contentType: 'application/json;charset=utf-8',  // 告知server，我给你的是什么格式
                data: JSON.stringify({
                    'code': code,
                    'input': ''
                }),
                success: function (data) {
                    //成功得到结果
                    // console.log(data);
                    show_result(data);
                }
            });
            // 3. 得到结果，解析并显示到 result中
            function show_result(data) {
                // console.log(data.status);
                // console.log(data.reason);
                // 拿到result结果标签
                var result_div = $(".container .part2 .result");
                // 清空上一次的运行结果
                result_div.empty();
 
                // 首先拿到结果的状态码和原因结果
                var _status = data.status;
                var _reason = data.reason;
 
                var reason_lable = $("<p>", {
                    text: _reason
                });
                reason_lable.appendTo(result_div);
 
                if (status == 0) {
                    // 请求是成功的，编译运行过程没出问题，但是结果是否通过看测试用例的结果
                    var _stdout = data.stdout;
                    var _stderr = data.stderr;
 
                    var stdout_lable = $("<pre>", {
                        text: _stdout
                    });
 
                    var stderr_lable = $("<pre>", {
                        text: _stderr
                    })
 
                    stdout_lable.appendTo(result_div);
                    stderr_lable.appendTo(result_div);
                }
                else {
                    // 编译运行出错,do nothing
                }
            }
        }
    </script>
</body>
 
</html>
5、OJ_model_MySQL.hpp：数据交互
该头文件通常是和数据交互的模块，例如对题库进行增删查改，对外提供访问题目的接口。

首先，根据SQL命令建的表的每一列属性，创建题目结构体。然后设计GetAllQuestions和GetOneQuestion函数，分别获取数据库中全部的题目用来构建题库网页和获取单个题目，为题目练习主页面构建题干。

两个函数分别用 string 类型来代替SQL指令，交给本头文件的核心函数 QueryMysql 执行SQL指令，该函数调用了许多使用C++访问SQL数据库的接口，在代码中都有所体现。

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
6、OJ_view.hpp：渲染网页
仍然属于前端部分，不多做赘述，放出代码供参考：

// 通常是拿到数据之后，奥进行构建网页，渲染网络内容，展示给用户的（浏览器功能）
#pragma once

#include <iostream>
#include <string>
#include <ctemplate/template.h>

#include "OJ_model_MySQL.hpp"

namespace ns_view
{
using namespace std;
using namespace ns_model;

    const string template_path = "./template_html/";

    class View
    {
    public:
        View() {}
        ~View() {}

    public:
        void AllExpandHtml(const vector<Question> &questions, string *html)
        {
            // 题目的编号 题目的标题 题目的难度
            // 推荐使用表格显示
            // 1. 形成路径
            string src_html = template_path + "all_questions.html";
            // 2. 形成数据字典
            ctemplate::TemplateDictionary root("all_questions");
            for (const auto &q : questions)
            {
                ctemplate::TemplateDictionary *sub = root.AddSectionDictionary("question_list");
                sub->SetValue("number", q.number);
                sub->SetValue("title", q.title);
                sub->SetValue("star", q.star);
            }

            // 3. 获取被渲染的html
            ctemplate::Template *tpl = ctemplate::Template::GetTemplate(src_html, ctemplate::DO_NOT_STRIP);

            // 4. 开始完成渲染功能
            tpl->Expand(html, &root);
        }

        void OneExpandHtml(const Question &q, string *html)
        {
            // 1. 形成路径
            string src_html = template_path + "one_question.html";

            // 2. 形成数据字典（不需要循环了，只有一个题目）
            ctemplate::TemplateDictionary root("one_question");
            root.SetValue("number", q.number);
            root.SetValue("title", q.title);
            root.SetValue("star", q.star);
            root.SetValue("desc", q.desc);
            root.SetValue("pre_code", q.header);

            // 3. 获取被渲染的html
            ctemplate::Template *tpl = ctemplate::Template::GetTemplate(src_html, ctemplate::DO_NOT_STRIP);

            // 4. 开始完成渲染功能
            tpl->Expand(html, &root);
        }
    };

}
7、OJ_Control.hpp：核心控制器
该头文件是本模块的核心业务逻辑所在处，既整合了前面各个文件，同时又多出了负载均衡、主机上线离线等多种业务，由于代码过长，我们分类来写：

7.1 主机类 Machine
首先，创建一个Machine类，视为每一台提供服务的主机，里面包含有主机的IP地址、主机端口号、提供编译服务的负载、用于负载均衡时保护负载的锁四个变量，也可以通过这个类来获取每台主机的负载情况，同时提供了接口，可以进行增加减少主机负载。

// 提供服务的主机，每个主机可以负载多个服务
class Machine
{
public:
Machine() : ip(""), port(0), load(0), mtx(nullptr)
{
}

        // 提升主机负载
        void IncLoad()
        {
            if (mtx)
            {
                mtx->lock();
            }
            load++;
            if (mtx)
            {
                mtx->unlock();
            }
        }
        // 减少主机负载
        void DecLoad()
        {
            if (mtx)
            {
                mtx->lock();
            }
            load--;
            if (mtx)
            {
                mtx->unlock();
            }
        }
        // 重置主机负载
        void ResetLoad()
        {
            if (mtx)
            {
                mtx->lock();
            }
            load = 0;
            if (mtx)
            {
                mtx->unlock();
            }
        }
        // 获取主机负载，意义不大，仅为统一接口
        uint64_t GetLoad()
        {
            uint64_t _load = 0;
            if (mtx)
            {
                mtx->lock();
            }
            _load = load;
            if (mtx)
            {
                mtx->unlock();
            }
            return _load;
        }
        ~Machine() {}

        string ip;     // 主机IP地址
        int port;      // 主机端口号
        uint64_t load; // 编译服务的负载
        mutex *mtx;    // 锁，用于负载均衡时保护负载，mutex类型不可被拷贝，使用指针完成
    };

7.2 主机负载均衡类 LoadBlance
该类负责负载均衡地选择主机进行编译运行服务，也是本项目最核心的一个特点。

整个负载均衡模块，单独设计为一个类，内含：

vector<Machine> machines：可以给我们提供编译服务的所有主机的集合，每台主机都有其下标，以下标充当其主机id；

vector<int> online：所有在线的主机集合

vector<int> offline：所有离线的主机集合

mutex mtx：保证 LoadBlance 它的数据安全

7.2.1 加载 service_machine.conf 主机配置文件
设计函数LoadConf（）加载 service_machine.conf 主机配置文件，配置文件所在路径会提前设置好。

7.2.2 切割配置文件，分割负载主机的 IP 地址和端口号，配置负载主机
读取配置文件成功后，循环进行字符串分割，分割出各个主机的IP地址和端口号，以此为参数创建若干个 Machine 类。

同时把这些 Machine 类 push 给 machines，以下标作为主机的ID，视作成功配置了这么多的主机。（为了减少步骤，在主机成功配置后，同时也 push 给 online，视作主机上线）

切割配置文件，就需要最后一个公共类：字符串切割类 StringUtil

公共类3：字符串切割类 StringUtil

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

7.2.1 + 7.2.2 代码：
// 加载配置
bool LoadConf(const string &machine_list)
{
ifstream in(machine_list);
if (!in.is_open())
{
Log(FATAL) << "加载配置文件失败！" << endl;
return false;
}
string line;
while (getline(in, line))
{
vector<string> tokens;
string sep = ":";
StringUtil::SpiltStr(line, &tokens, sep);
if (tokens.size() != 2)
{
Log(WARNING) << " 切分 " << line << " 失败" << endl;
continue;
}
Machine m;
m.ip = tokens[0];
m.port = stoi(tokens[1].c_str());
m.load = 0;
m.mtx = new mutex();
online.push_back(machines.size()); // 用machines的数量代替下标给id，减少了一步
machines.push_back(m);
}
in.close();
return true;
}
7.2.3 主机的上线与下线
我们通过操纵 vector<int> online 和 vector<int> offline 来进行，值得注意的是，主机离线之时，需清空这个主机上的所有负载。

        // 主机上线
        void OnlineMachine()
        {
            // 我们统一上线，后面统一解决
            mtx.lock();
            // 插入上线主机，insert传入参数：插入位置，数据的开头，数据的结尾
            online.insert(online.end(), offline.begin(), offline.end());
            // 删除下线主机
            offline.erase(offline.begin(), offline.end());
            mtx.unlock();

            Log(INFO) << "所有的主机全部上线！" << endl;
        }

        // 主机离线
        void OfflineMachine(int OffId)
        {
            mtx.lock();
            for (auto iter = online.begin(); iter != online.end(); iter++)
            {
                if (*iter == OffId)
                {
                    machines[OffId].ResetLoad(); // 主机离线，清零其负载
                    // 要离线的主机已经找到
                    online.erase(iter);
                    offline.push_back(OffId);
                    break; // 因为break的存在，所有我们暂时不考虑迭代器失效的问题
                }
            }
            mtx.unlock();
        }

7.2.4 负载均衡地选择主机
能够负载均衡地选择主机，是本项目的精髓所在。

负载均衡算法有两种：随机数 + 哈希 / 轮询 + 哈希；这里我们选择通过 轮询+哈希 的负载均衡算法来选择主机，即：

函数参数 主机ID 与 主机类地址，初始默认是第一台主机，然后通过遍历 vector<machine> machines 数组的方式（注意：数组下标是 vector<int> online 数组中的元素，因为以下标代替ID，所以找到的都是在线的主机）找到其中负载最小的机器来替换 ID 与 地址。

（因为主机是多线程共享的，挑选期间不要忘记加锁）

        // 智能选择，id和m都是输出型参数
        bool SmartChoice(int *id, Machine **m)
        {
            // 1. 使用选择好的主机(更新该主机的负载)
            // 2. 我们需要可能离线该主机
            mtx.lock();
            // 负载均衡的算法—— 1. 随机数+hash   2. 轮询+hash（采用此法）
            int online_num = online.size();
            if (online_num == 0)
            {
                mtx.unlock();
                Log(FATAL) << " 所有的后端编译主机都已经离线！没有主机可供选择！" << endl;
                return false;
            }
            // 通过遍历的方式，找到负载最小的机器
            *id = online[0];
            *m = &machines[online[0]]; // 传回去的是地址
            uint64_t MinLoad = machines[online[0]].GetLoad();
            for (int i = 1; i < online_num; i++)
            {
                uint64_t CurLoad = machines[online[i]].GetLoad();
                if (MinLoad > CurLoad)
                {
                    MinLoad = CurLoad;
                    *id = online[i];
                    *m = &machines[online[i]];
                }
            }
            mtx.unlock();
            return true;
        }

7.3 核心业务逻辑的控制器 Control
7.3.1 构建题目网页和题目练习主页面
结合前面的 OJ_view.hpp 渲染网页头文件与 OJ_model_MySQL.hpp 题库头文件，构建出题库网页与 题目练习主页面。

// 根据题库数据构建网页 传入html输出型参数
bool AllQuestions(string *html)
{
vector<Question> all;
bool ret = true;
if (\_model.GetAllQuestions(&all))
{
// 给题目进行排序
sort(all.begin(), all.end(), [](const Question &q1, const Question &q2)
{ return stoi(q1.number.c_str()) < stoi(q2.number.c_str()); }); // 小于升序排序，大于降序排序
// 获取题库信息成功，把题库构建成网页
\_view.AllExpandHtml(all, html);
}
else
{
*html = "获取题库失败，无法形成题库网页！";
ret = false;
}
return ret;
}

        // 构建指定单个题目的网页
        bool OneQuestion(const string &number, string *html)
        {
            Question q;
            bool ret = true;
            if (_model.GetOneQuestion(number, &q))
            {
                // 获取单个题目信息成功，构建网页
                _view.OneExpandHtml(q, html);
            }
            else
            {
                *html = "获取单个题目失败，无法形成题目网页！";
                ret = false;
            }
            return ret;
        }

7.3.2 较为核心的题目判断部分
传入参数：题目编号，客户上传的代码，返回的结果。

1、根据题目编号，拿到对应的题目细节。

2、对客户上传的代码进行反序列化操作 。

3、把用户代码和测试用例代码进行拼接，然后再进行序列化。

4、找到选择负载最低的主机（规则：一直寻找，直到主机可用，否则，就是所有主机全部离线

5、建立一个 httplib 的客户端类，通过找到的这个负载均衡主机的 IP 地址和端口号，向对应的 compile_server发送http请求，请求编译和运行（客户端请求与服务端响应在post时采用相同的路径，就可以得到返回报文）

（PS：似乎没有判断测试用例？实际上是有的：

用户的OJ页面

测试用例页面

本项目的判断比较简单，就是把测试用例代码和用户代码拼接之后，直接运行，根据运行结果输出到标准输出上。）

8、OJ_Server.cpp：完成后端业务逻辑
在打包给用户的可执行程序中，就会有 OJ_Server.cpp 编译产生的 OJ_server可执行文件，用户通过“ ./oj_server ”命令，便可以在自己的云服务器后端启动客户端。

此时，在浏览器中输入“云服务器公网:8080”，便可以启动OJ系统，看见OJ系统的网页。

8.1 搭建首页
在代码中，我们并没有为 / 根目录设置路由处理函数，但是我们设置了svr.set_base_dir("./wwwroot")；

这就告诉了 httplib 服务器，使用指定的目录作为静态文件服务的根目录，在找不到任何对应路由请求的情况下，使用这个目录下的文件作为响应（即首页文件作为初始响应）

8.2 前端点击“点击我开始编程”发送请求后，用Get方法先获取前端题库1.前端发起请求：前端通过 HTTP 发起 GET 请求到 http://localhost:8080/all_questions

2.服务器处理请求：

服务器接收到请求，并根据请求的 URL 路径调用相应的路由处理函数。
svr.Get 方法根据 URL 路径调用对应的 lambda 函数。3.执行 lambda 函数：

当前端向服务器发起 GET 请求到 /all_questions 路由时，httplib 库会捕捉到这个请求并调用对应定义的匿名 lambda 函数，传入 req 和 resp 参数（具体流程：如果请求是 GET 方法，httplib 库就会把函数和路由注册在 httplib 的 handlers 表中，在 handlers 映射表中，如果请求方法和请求资源有相对应的映射函数，则会调用相对应的映射函数，即这里的 lambda 函数；通过lambda函数解析 http 请求形成Request类）
lambda 函数执行其内部逻辑，包括调用 ctrl.AllQuestions(&html); 来获取题目的列表，并将结果设置为响应的内容；设置响应的内容类型为 HTML，并将 html 设置为响应体。4.发送响应:

最终，服务器发送带有 HTML 内容的响应给前端。

8.3 点击题目标题，获取单个题目页面
执行逻辑与题库页面类似

8.4 点击提交代码，实现判题功能 1.前端发起 POST 请求（可以通过请求正文传递参数）:

客户端通过 HTTP 发起 POST 请求到 http://localhost:8080/judge/(\d+)。这里的 (\d+) 是一个正则表达式，表示 URL 中包含一个数字。2.服务器处理请求:

服务器接收到请求，并根据请求的 URL 路径调用相应的路由处理函数。
svr.Post 方法根据 URL 路径调用对应的 lambda 函数。3.执行 lambda 函数:

httplib 库调用 lambda 函数，传入 req 和 resp 参数。
req.matches[1] 获取 URL 中捕获的题号，并将其赋值给 number。（当定义一个使用正则表达式的路由时，httplib会捕获 URL 中与正则表达式匹配的部分，并将这些捕获的组存储在req.matches数组中。）
req.body 获取 POST 请求体中的数据，通常用于传递用户的代码或相关的判题信息。
调用 ctrl.Judge(number, req.body, &result_json); 方法来处理判题逻辑，其中 number 是题目编号，req.body 是提交的代码，result_json 是一个指向字符串的指针，用于接收判题的结果。4.设置响应内容:

resp.set_content(result_json, "application/json;charset=utf-8"); 设置响应的内容类型为 JSON，并将 result_json 设置为响应体的内容。5.发送响应:

最终，服务器发送带有 JSON 数据的响应给前端。
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include "../comm/httplib.h"
#include "OJ_control.hpp"

using namespace std;
using namespace httplib;
using namespace ns_control;

static Control \*ctrl_ptr = nullptr;

void Recovery(int signo)
{
ctrl_ptr->RecoveryMachine();
}

int main()
{
// 0、修改指定信号的默认行为，把ctrl + \产生的SIGQUIT信号重定向为一键上线所有主机（交由运维使用）
signal(SIGQUIT, Recovery);

        // 1、守护进程化
        if (daemon(1, 0) == -1)
        {
                perror("oj_server 守护进程化失败！\n");
                exit(EXIT_FAILURE);
        }

        // 2、用户请求的服务路由功能（用户不同的请求前往不同的区域）
        Server svr;   // httplib中的Server类
        Control ctrl; // 核心业务逻辑的控制器（初始化的同时，加载题库）
        ctrl_ptr = &ctrl;

        // 3、搭建首页，告诉httplib服务器使用指定的目录作为静态文件服务的根目录，在找不到任何对应路由请求的情况下，使用这个目录下的文件作为响应（即首页文件）
        svr.set_base_dir("./wwwroot");

        // 3.1 获取所有题目的列表
        svr.Get("/all_questions", [&ctrl](const Request &req, Response &resp)
                {
                string html;
                ctrl.AllQuestions(&html);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 3.2 用户提供题目编号来获取题目内容
        svr.Get(R"(/question/(\d+))", [&ctrl](const Request &req, Response &resp)
                {
                string number = req.matches[1];     //matches[1]中储存着正则匹配到的字符
                string html;
                ctrl.OneQuestion(number, &html);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 3.3 用户提交代码，使用我们的判题功能
        // 尽管POST请求的参数是在正文当中的，但是调用函数的时候还是要根据URL路径来调用（因为HTTP 服务器的核心功能之一是根据客户端请求的 URL 路径来决定如何处理请求。路由机制确保了不同类型的请求可以被正确地路由到相应的处理函数。）
        svr.Post(R"(/judge/(\d+))", [&ctrl](const Request &req, Response &resp)
                 {   string number = req.matches[1];
                 string result_json;
                 ctrl.Judge(number, req.body /* POST请求的参数，存放在正文当中 */, &result_json);
                 resp.set_content(result_json, "application/json;charset=utf-8"); });

        svr.listen("0.0.0.0", 8080);
        return 0;

}
六、 项目发布模块（output）
把客户端和服务端以makefile形式打包形成一个output文件夹，如果用户要使用，可直接把这个output文件夹交给用户，不需要把大量源代码给出。

下面给出Makefile源代码：

.PHONY: all
all:
@cd compile_server;\
 make;\
 cd -;\
 cd oj_server;\
 make;\
 cd -;

.PHONY:output
output:
@mkdir -p output/compile_server;\
 mkdir -p output/oj_server;\
 cp -rf compile_server/compile_server output/compile_server;\
 cp -rf compile_server/temp output/compile_server;\
 cp -rf oj_server/conf output/oj_server/;\
 cp -rf oj_server/lib output/oj_server/;\
 cp -rf oj_server/questions output/oj_server/;\
 cp -rf oj_server/template_html output/oj_server/;\
 cp -rf oj_server/wwwroot output/oj_server/;\
 cp -rf oj_server/oj_server output/oj_server/;

.PHONY:clean
clean:
@cd compile_server;\
 make clean;\
 cd -;\
 cd oj_server;\
 make clean;\
 cd -;\
 rm -rf output;
七、项目完整源码
https://gitee.com/dongfang-weim
