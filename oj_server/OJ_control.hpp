// 控制器（逻辑控制）模块，即核心业务逻辑
#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <cassert>
#include <jsoncpp/json/json.h>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <sys/stat.h>
#include <signal.h>

#include "../comm/Util.hpp"
#include "../comm/Log.hpp"
#include "../comm/httplib.h"
#include "OJ_view.hpp"
#include "OJ_model.hpp"

namespace ns_control
{
    using namespace std;
    using namespace ns_Util;
    using namespace ns_Log;
    using namespace ns_model;
    using namespace ns_view;
    using namespace httplib;

    inline void EnsureDataDir(const std::string &dir)
    {
        struct stat st;
        if (stat(dir.c_str(), &st) == 0)
        {
            return;
        }
        mkdir(dir.c_str(), 0755);
    }

    inline std::string TaskStateToString(int state)
    {
        switch (state)
        {
        case 0:
            return "PENDING";
        case 1:
            return "RUNNING";
        case 2:
            return "DONE";
        case 3:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    class SubmissionStore
    {
    public:
        SubmissionStore(const std::string &path) : _path(path)
        {
        }

        void Append(const Json::Value &record)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            std::ofstream out(_path, std::ios::out | std::ios::app);
            if (!out.is_open())
                return;
            Json::FastWriter writer;
            out << writer.write(record) << "\n";
        }

        void LoadAccepted(std::unordered_map<std::string, std::unordered_set<std::string>> *userSolved,
                          std::unordered_map<std::string, uint64_t> *acceptCount) const
        {
            std::ifstream in(_path);
            if (!in.is_open())
                return;
            std::string line;
            Json::Reader reader;
            while (std::getline(in, line))
            {
                if (line.empty())
                    continue;
                Json::Value v;
                if (!reader.parse(line, v))
                    continue;
                const std::string verdict = v.get("verdict", "").asString();
                if (verdict != "Accepted")
                    continue;
                const std::string uid = v.get("user_id", "").asString();
                const std::string qid = v.get("question_number", "").asString();
                if (uid.empty() || qid.empty())
                    continue;
                (*userSolved)[uid].insert(qid);
                (*acceptCount)[qid] += 1;
            }
        }

    private:
        std::string _path;
        mutable std::mutex _mtx;
    };

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

    const string service_machine = "./conf/service_machine.conf";
    // 负载均衡模块
    class LoadBlance
    {
    public:
        LoadBlance()
        {
            assert(LoadConf(service_machine));
            Log(INFO) << "加载主机配置文件" << service_machine << "成功！" << "\n";
        }
        // 加载配置
        bool LoadConf(const string &machine_list)
        {
            ifstream in(machine_list);
            if (!in.is_open())
            {
                Log(FATAL) << "加载配置文件失败！" << "\n";
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
                    Log(WARNING) << " 切分 " << line << " 失败" << "\n";
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
                Log(FATAL) << " 所有的后端编译主机已经离线，请运维的同事尽快查看！" << "\n";
                return false;
            }
            // 通过遍历的方式，找到所有负载最小的机器
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

            Log(INFO) << "所有的主机全部上线！" << "\n";
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
        // 展现主机列表
        void ShowMachines()
        {
            mtx.lock();
            cout << "当前在线主机列表：";
            for (auto &id : online)
            {
                cout << id << " ";
            }
            cout << endl;
            cout << "当前离线主机列表：";
            for (auto &id : online)
            {
                cout << id << " ";
            }
            cout << endl;
            mtx.unlock();
        }

        ~LoadBlance()
        {
        }

    private:
        vector<Machine> machines; // 可以给我们提供编译服务的所有主机，每台主机都有其下标，充当其主机id
        vector<int> online;       // 所有在线的主机id
        vector<int> offline;      // 所有离线的主机id
        mutex mtx;                // 保证LoadBlance它的数据安全
    };

    // 这是我们核心业务逻辑的控制器
    class Control
    {
    public:
        Control() : _store("./data/submissions.jsonl")
        {
            EnsureDataDir("./data");
            StartWorkers(4);
        }

        ~Control()
        {
            StopWorkers();
        }

        // 恢复主机
        void RecoveryMachine()
        {
            _loadblance.OnlineMachine();
        }
        // 根据题库数据构建网页 传入html输出型参数
        bool AllQuestions(string *html)
        {
            vector<Question> all;
            bool ret = true;
            if (_model.GetAllQuestions(&all))
            {
                // 给题目进行排序
                sort(all.begin(), all.end(), [](const Question &q1, const Question &q2)
                     { return stoi(q1.number.c_str()) < stoi(q2.number.c_str()); }); // 小于升序排序，大于降序排序
                // 获取题库信息成功，把题库构建成网页
                _view.AllExpandHtml(all, html);
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

        // 判断题目
        // 传入参数：题目编号，客户上传的代码，返回的结果
        void Judge(const string &user_id, const string &number, const string in_json, string *out_json)
        {
            // 1、根据题目编号，拿到对应的题目细节
            Question q;
            _model.GetOneQuestion(number, &q);

            // 2、对in_json进行反序列化操作
            Json::Value in_value;
            Json::Reader reader;
            reader.parse(in_json, in_value);

            // 3、把用户代码和测试用例代码进行拼接，并进行序列化
            Json::Value compile_value;
            string code = in_value["code"].asString();
            compile_value["code"] = code + "\n" + q.tail; // 最重要的一步，拼接代码
            compile_value["input"] = in_value["input"].asString();
            compile_value["cpu_limit"] = q.cpu_limit;
            compile_value["mem_limit"] = q.mem_limit;
            Json::FastWriter writer;
            string compile_str = writer.write(compile_value);

            // 4、选择负载最低的主机
            // 规则：一直寻找，直到主机可用，否则，就是全部挂掉
            while (true)
            {
                int id = 0;
                Machine *m = nullptr;
                if (!_loadblance.SmartChoice(&id, &m))
                {
                    Json::Value fail;
                    fail["status"] = -2;
                    fail["reason"] = "所有评测节点离线";
                    fail["verdict"] = "SystemError";
                    Json::StyledWriter w;
                    *out_json = w.write(fail);
                    AppendSubmission(user_id, number, q.star, fail);
                    break;
                }

                // 5、向远端发送http请求，得到结果
                Client cli(m->ip, m->port);
                m->IncLoad(); // 负载增加
                Log(INFO) << "第" << id << "号主机选择成功，IP地址：" << m->ip << " 端口号：" << m->port << "当前主机负载：" << m->load << "\n";
                if (auto res = cli.Post("/compile_and_run", compile_str, "application/json;charset=utf-8"))
                {
                    // 5. 将结果赋值给out_json
                    if (res->status == 200) // 状态码是这个才代表成功
                    {
                        // 给编译运行结果补充 verdict 等信息
                        Json::Value compile_res;
                        Json::Reader r;
                        r.parse(res->body, compile_res);
                        compile_res["question_number"] = number;
                        compile_res["machine"] = m->ip + ":" + std::to_string(m->port);
                        compile_res["verdict"] = MakeVerdict(compile_res);
                        Json::StyledWriter w;
                        *out_json = w.write(compile_res);

                        AppendSubmission(user_id, number, q.star, compile_res);
                        m->DecLoad();
                        Log(INFO) << "请求编译和运行服务成功..." << "\n";
                        break;
                    }
                    m->DecLoad();
                }
                else
                {
                    // 请求失败，离线主机
                    Log(ERROR) << " 当前请求的主机id: " << id << " 详情: " << m->ip << ":" << m->port << " 可能已经离线" << "\n";
                    _loadblance.OfflineMachine(id);
                    _loadblance.ShowMachines(); // 调试用
                }
            }
        }

        // 异步提交评测任务：快速返回 task_id
        void JudgeAsync(const std::string &user_id, const std::string &number, const std::string &in_json, std::string *out_json)
        {
            const std::string task_id = FileUtil::UniqFileName();
            Task task;
            task.id = task_id;
            task.user_id = user_id;
            task.number = number;
            task.in_json = in_json;
            task.state = 0;
            task.created_ms = std::stoull(TimeUtil::GetTimeMs());

            {
                std::lock_guard<std::mutex> lock(_task_mtx);
                _tasks[task_id] = task;
                _pending.push(task_id);
            }
            _task_cv.notify_one();

            Json::Value resp;
            resp["task_id"] = task_id;
            resp["state"] = TaskStateToString(0);
            Json::StyledWriter w;
            *out_json = w.write(resp);
        }

        bool GetTask(const std::string &task_id, std::string *out_json)
        {
            Task task;
            {
                std::lock_guard<std::mutex> lock(_task_mtx);
                auto it = _tasks.find(task_id);
                if (it == _tasks.end())
                    return false;
                task = it->second;
            }

            Json::Value resp;
            resp["task_id"] = task.id;
            resp["state"] = TaskStateToString(task.state);
            resp["created_ms"] = Json::UInt64(task.created_ms);
            if (task.started_ms)
                resp["started_ms"] = Json::UInt64(task.started_ms);
            if (task.finished_ms)
                resp["finished_ms"] = Json::UInt64(task.finished_ms);
            if (task.state == 2)
            {
                resp["result"] = task.result;
            }
            if (task.state == 3)
            {
                resp["error"] = task.error;
            }
            Json::StyledWriter w;
            *out_json = w.write(resp);
            return true;
        }

        // 推荐题目（HTML）
        bool Recommend(const std::string &user_id, std::string *html)
        {
            std::vector<Question> all;
            if (!_model.GetAllQuestions(&all))
                return false;
            std::unordered_map<std::string, Question> qmap;
            for (const auto &q : all)
                qmap[q.number] = q;

            std::unordered_map<std::string, std::unordered_set<std::string>> userSolved;
            std::unordered_map<std::string, uint64_t> acceptCount;
            _store.LoadAccepted(&userSolved, &acceptCount);

            std::vector<std::pair<Question, double>> recs;
            BuildRecommend(user_id, qmap, userSolved, acceptCount, &recs);
            _view.RecommendExpandHtml(recs, html);
            return true;
        }

        // 推荐题目（JSON）
        bool RecommendJson(const std::string &user_id, std::string *out_json)
        {
            std::vector<Question> all;
            if (!_model.GetAllQuestions(&all))
                return false;
            std::unordered_map<std::string, Question> qmap;
            for (const auto &q : all)
                qmap[q.number] = q;

            std::unordered_map<std::string, std::unordered_set<std::string>> userSolved;
            std::unordered_map<std::string, uint64_t> acceptCount;
            _store.LoadAccepted(&userSolved, &acceptCount);

            std::vector<std::pair<Question, double>> recs;
            BuildRecommend(user_id, qmap, userSolved, acceptCount, &recs);

            Json::Value resp;
            for (const auto &item : recs)
            {
                Json::Value row;
                row["number"] = item.first.number;
                row["title"] = item.first.title;
                row["star"] = item.first.star;
                row["score"] = item.second;
                resp["items"].append(row);
            }
            Json::StyledWriter w;
            *out_json = w.write(resp);
            return true;
        }

    private:
        struct Task
        {
            std::string id;
            std::string user_id;
            std::string number;
            std::string in_json;
            int state = 0; // 0 pending,1 running,2 done,3 error
            uint64_t created_ms = 0;
            uint64_t started_ms = 0;
            uint64_t finished_ms = 0;
            Json::Value result;
            std::string error;
        };

        static std::string MakeVerdict(const Json::Value &compile_res)
        {
            const int status = compile_res.get("status", -2).asInt();
            if (status == 0)
            {
                const std::string out = compile_res.get("stdout", "").asString();
                if (out.find("没有通过") != std::string::npos)
                    return "WrongAnswer";
                return "Accepted";
            }
            if (status == -3)
                return "CompileError";
            if (status == SIGXCPU)
                return "TimeLimitExceeded";
            if (status == SIGABRT)
                return "MemoryLimitExceeded";
            if (status > 0)
                return "RuntimeError";
            return "SystemError";
        }

        void AppendSubmission(const std::string &user_id, const std::string &number, const std::string &star, const Json::Value &result)
        {
            if (user_id.empty())
                return;
            Json::Value rec;
            rec["ts_ms"] = Json::UInt64(std::stoull(TimeUtil::GetTimeMs()));
            rec["user_id"] = user_id;
            rec["question_number"] = number;
            rec["star"] = star;
            rec["verdict"] = result.get("verdict", "");
            rec["status"] = result.get("status", -2);
            _store.Append(rec);
        }

        void StartWorkers(size_t worker_num)
        {
            _stop.store(false);
            for (size_t i = 0; i < worker_num; i++)
            {
                _workers.emplace_back([this]()
                                      { this->WorkerLoop(); });
            }
        }

        void StopWorkers()
        {
            _stop.store(true);
            _task_cv.notify_all();
            for (auto &t : _workers)
            {
                if (t.joinable())
                    t.join();
            }
            _workers.clear();
        }

        void WorkerLoop()
        {
            while (!_stop.load())
            {
                std::string task_id;
                {
                    std::unique_lock<std::mutex> lock(_task_mtx);
                    _task_cv.wait(lock, [this]()
                                  { return _stop.load() || !_pending.empty(); });
                    if (_stop.load())
                        return;
                    task_id = _pending.front();
                    _pending.pop();

                    auto it = _tasks.find(task_id);
                    if (it == _tasks.end())
                        continue;
                    it->second.state = 1;
                    it->second.started_ms = std::stoull(TimeUtil::GetTimeMs());
                }

                Task snapshot;
                {
                    std::lock_guard<std::mutex> lock(_task_mtx);
                    snapshot = _tasks[task_id];
                }

                std::string out;
                try
                {
                    Judge(snapshot.user_id, snapshot.number, snapshot.in_json, &out);
                    Json::Value v;
                    Json::Reader r;
                    r.parse(out, v);
                    std::lock_guard<std::mutex> lock(_task_mtx);
                    auto &t = _tasks[task_id];
                    t.state = 2;
                    t.finished_ms = std::stoull(TimeUtil::GetTimeMs());
                    t.result = v;
                }
                catch (...)
                {
                    std::lock_guard<std::mutex> lock(_task_mtx);
                    auto &t = _tasks[task_id];
                    t.state = 3;
                    t.finished_ms = std::stoull(TimeUtil::GetTimeMs());
                    t.error = "评测执行异常";
                }
            }
        }

        static void BuildRecommend(const std::string &user_id,
                                   const std::unordered_map<std::string, Question> &qmap,
                                   const std::unordered_map<std::string, std::unordered_set<std::string>> &userSolved,
                                   const std::unordered_map<std::string, uint64_t> &acceptCount,
                                   std::vector<std::pair<Question, double>> *out)
        {
            out->clear();
            // 1) 计算题目两两共现（基于 Accepted）
            std::unordered_map<std::string, std::unordered_map<std::string, double>> co;
            for (const auto &kv : userSolved)
            {
                const auto &items = kv.second;
                for (const auto &i : items)
                {
                    for (const auto &j : items)
                    {
                        if (i == j)
                            continue;
                        co[i][j] += 1.0;
                    }
                }
            }

            std::unordered_set<std::string> solved;
            auto itSolved = userSolved.find(user_id);
            if (itSolved != userSolved.end())
                solved = itSolved->second;

            // 2) 计算候选得分（物品协同过滤：已解决题目 -> 相似题目）
            std::unordered_map<std::string, double> score;
            for (const auto &i : solved)
            {
                auto it = co.find(i);
                if (it == co.end())
                    continue;
                for (const auto &kv : it->second)
                {
                    const auto &j = kv.first;
                    if (solved.count(j))
                        continue;
                    const double cij = kv.second;
                    const double ni = (double)std::max<uint64_t>(1, acceptCount.count(i) ? acceptCount.at(i) : 1);
                    const double nj = (double)std::max<uint64_t>(1, acceptCount.count(j) ? acceptCount.at(j) : 1);
                    const double sim = cij / std::sqrt(ni * nj);
                    score[j] += sim;
                }
            }

            // 3) 若该用户没有足够历史，则按全局通过数回退
            if (score.empty())
            {
                std::vector<std::pair<std::string, uint64_t>> pop;
                for (const auto &q : qmap)
                {
                    if (solved.count(q.first))
                        continue;
                    pop.push_back({q.first, acceptCount.count(q.first) ? acceptCount.at(q.first) : 0});
                }
                std::sort(pop.begin(), pop.end(), [](const std::pair<std::string, uint64_t> &a, const std::pair<std::string, uint64_t> &b)
                          { return a.second > b.second; });
                size_t topn = std::min<size_t>(10, pop.size());
                for (size_t i = 0; i < topn; i++)
                {
                    auto itq = qmap.find(pop[i].first);
                    if (itq != qmap.end())
                        out->push_back({itq->second, (double)pop[i].second});
                }
                return;
            }

            std::vector<std::pair<std::string, double>> items(score.begin(), score.end());
            std::sort(items.begin(), items.end(), [](const std::pair<std::string, double> &a, const std::pair<std::string, double> &b)
                      { return a.second > b.second; });
            size_t topn = std::min<size_t>(10, items.size());
            for (size_t i = 0; i < topn; i++)
            {
                auto itq = qmap.find(items[i].first);
                if (itq != qmap.end())
                    out->push_back({itq->second, items[i].second});
            }
        }

        Model _model;           // 提供后台数据
        View _view;             // 提供网页渲染服务
        LoadBlance _loadblance; // 核心负载均衡控制器

        SubmissionStore _store;

        std::atomic<bool> _stop{false};
        std::mutex _task_mtx;
        std::condition_variable _task_cv;
        std::queue<std::string> _pending;
        std::unordered_map<std::string, Task> _tasks;
        std::vector<std::thread> _workers;
    };
};
