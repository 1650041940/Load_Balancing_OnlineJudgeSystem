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
#include <vector>
#include <sstream>
#include "../comm/httplib.h"
#include "OJ_control.hpp"

using namespace std;
using namespace httplib;
using namespace ns_control;

static Control *ctrl_ptr = nullptr;

void Recovery(int signo)
{
        ctrl_ptr->RecoveryMachine();
}

static std::string GetCookieValue(const httplib::Request &req, const std::string &key)
{
        auto cookie = req.get_header_value("Cookie");
        if (cookie.empty())
                return "";

        // 简单解析: a=b; c=d
        std::istringstream iss(cookie);
        std::string token;
        while (std::getline(iss, token, ';'))
        {
                // trim leading space
                while (!token.empty() && token[0] == ' ')
                        token.erase(token.begin());
                auto pos = token.find('=');
                if (pos == std::string::npos)
                        continue;
                auto k = token.substr(0, pos);
                auto v = token.substr(pos + 1);
                if (k == key)
                        return v;
        }
        return "";
}

static std::string EnsureUserId(const httplib::Request &req, httplib::Response &resp)
{
        std::string uid = GetCookieValue(req, "uid");
        if (!uid.empty())
                return uid;
        uid = ns_Util::FileUtil::UniqFileName();
        // 30 天有效
        resp.set_header("Set-Cookie", "uid=" + uid + "; Path=/; Max-Age=2592000");
        return uid;
}

int main(int argc, char **argv)
{
        // 0、修改指定信号的默认行为，把ctrl + \产生的SIGQUIT信号重定向为一键上线所有主机（交由运维使用）
        signal(SIGQUIT, Recovery);

        int port = 8080;
        bool no_daemon = false;
        for (int i = 1; i < argc; i++)
        {
                std::string arg = argv[i];
                if (arg == "--no-daemon")
                        no_daemon = true;
                else if (arg == "--port" && i + 1 < argc)
                        port = atoi(argv[++i]);
        }

        // 1、守护进程化（默认开启，便于部署；开发调试可加 --no-daemon）
        if (!no_daemon)
        {
                if (daemon(1, 0) == -1)
                {
                        perror("oj_server 守护进程化失败！\n");
                        exit(EXIT_FAILURE);
                }
        }

        // 2、用户请求的服务路由功能（用户不同的请求前往不同的区域）
        Server svr;   // httplib中的Server类
        Control ctrl; // 核心业务逻辑的控制器（初始化的同时，加载题库）
        ctrl_ptr = &ctrl;

        // 3、搭建首页，告诉httplib服务器使用指定的目录作为静态文件服务的根目录，在找不到任何对应路由请求的情况下，使用这个目录下的文件作为响应（即首页文件）
        svr.set_base_dir("./wwwroot");

        // 注册页面（静态 HTML）
        svr.Get("/register", [](const Request &req, Response &resp)
                {
                EnsureUserId(req, resp);
                std::string html;
                ns_Util::FileUtil::ReadFile("./wwwroot/register.html", &html, true);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 登录页面（静态 HTML）
        svr.Get("/login", [](const Request &req, Response &resp)
                {
                EnsureUserId(req, resp);
                std::string html;
                ns_Util::FileUtil::ReadFile("./wwwroot/login.html", &html, true);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 注册接口：只需要用户名（演示用），成功后写入 uid cookie
        svr.Post("/api/register", [&ctrl](const Request &req, Response &resp)
                 {
                 Json::Value in;
                 Json::Reader r;
                 r.parse(req.body, in);
                 const std::string username = in.get("username", "").asString();
                 std::string uid;
                 std::string err;
                 Json::Value out;
                 if (!ctrl.RegisterUser(username, &uid, &err))
                 {
                        out["ok"] = false;
                        out["error"] = err;
                        Json::StyledWriter w;
                        resp.status = 400;
                        resp.set_content(w.write(out), "application/json;charset=utf-8");
                        return;
                 }
                 // 30 天有效
                 resp.set_header("Set-Cookie", "uid=" + uid + "; Path=/; Max-Age=2592000");
                 out["ok"] = true;
                 out["uid"] = uid;
                 out["username"] = username;
                 Json::StyledWriter w;
                 resp.set_content(w.write(out), "application/json;charset=utf-8"); });

        // 登录接口：输入用户名，找到对应 uid 并写 cookie
        svr.Post("/api/login", [&ctrl](const Request &req, Response &resp)
                 {
                 Json::Value in;
                 Json::Reader r;
                 r.parse(req.body, in);
                 const std::string username = in.get("username", "").asString();
                 std::string uid;
                 std::string err;
                 Json::Value out;
                 if (!ctrl.LoginUser(username, &uid, &err))
                 {
                        out["ok"] = false;
                        out["error"] = err;
                        Json::StyledWriter w;
                        resp.status = 400;
                        resp.set_content(w.write(out), "application/json;charset=utf-8");
                        return;
                 }
                 resp.set_header("Set-Cookie", "uid=" + uid + "; Path=/; Max-Age=2592000");
                 out["ok"] = true;
                 out["uid"] = uid;
                 out["username"] = username;
                 Json::StyledWriter w;
                 resp.set_content(w.write(out), "application/json;charset=utf-8"); });

        // 退出：清除 cookie 并跳回首页
        svr.Get("/logout", [](const Request &req, Response &resp)
                {
                (void)req;
                resp.set_header("Set-Cookie", "uid=; Path=/; Max-Age=0");
                resp.status = 302;
                resp.set_header("Location", "/"); });

        // 3.1 获取所有题目的列表
        svr.Get("/all_questions", [&ctrl](const Request &req, Response &resp)
                { 
                std::string uid = EnsureUserId(req, resp);
                string html;
                ctrl.AllQuestions(uid, &html);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 3.2 用户提供题目编号来获取题目内容
        svr.Get(R"(/question/(\d+))", [&ctrl](const Request &req, Response &resp)
                {
                std::string uid = EnsureUserId(req, resp);
                string number = req.matches[1];     //matches[1]中储存着正则匹配到的字符
                string html;
                ctrl.OneQuestion(uid, number, &html);
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 3.3 用户提交代码，使用我们的判题功能
        svr.Post(R"(/judge/(\d+))", [&ctrl](const Request &req, Response &resp)
                 {   string number = req.matches[1];
                 string uid = EnsureUserId(req, resp);
                 string result_json;
                 ctrl.Judge(uid, number, req.body, &result_json);
                 resp.set_content(result_json, "application/json;charset=utf-8"); });

        // 3.4 异步提交评测任务
        svr.Post(R"(/judge_async/(\d+))", [&ctrl](const Request &req, Response &resp)
                 {
                string number = req.matches[1];
                string uid = EnsureUserId(req, resp);
                string result_json;
                ctrl.JudgeAsync(uid, number, req.body, &result_json);
                resp.set_content(result_json, "application/json;charset=utf-8"); });

        // 3.5 查询任务状态
        svr.Get(R"(/task/([0-9\-]+))", [&ctrl](const Request &req, Response &resp)
                {
                EnsureUserId(req, resp);
                string task_id = req.matches[1];
                string out;
                if (!ctrl.GetTask(task_id, &out))
                {
                        resp.status = 404;
                        resp.set_content("{\"error\":\"task not found\"}", "application/json;charset=utf-8");
                        return;
                }
                resp.set_content(out, "application/json;charset=utf-8"); });

        // 3.6 推荐题目页面
        svr.Get("/recommend", [&ctrl](const Request &req, Response &resp)
                {
                string uid = EnsureUserId(req, resp);
                string html;
                if (!ctrl.Recommend(uid, &html))
                {
                        resp.set_content("推荐生成失败", "text/plain; charset=utf-8");
                        return;
                }
                resp.set_content(html, "text/html; charset=utf-8"); });

        // 3.7 推荐题目接口
        svr.Get("/api/recommend", [&ctrl](const Request &req, Response &resp)
                {
                string uid = EnsureUserId(req, resp);
                string out;
                if (!ctrl.RecommendJson(uid, &out))
                {
                        resp.status = 500;
                        resp.set_content("{\"error\":\"recommend failed\"}", "application/json;charset=utf-8");
                        return;
                }
                resp.set_content(out, "application/json;charset=utf-8"); });

        svr.listen("0.0.0.0", port);
        return 0;
}