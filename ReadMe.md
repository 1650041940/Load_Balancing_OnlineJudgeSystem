## 项目简介

这是一个 C++ 实现的负载均衡 Online Judge（OJ）系统：

- `oj_server`：Web 前端静态页 + 题库渲染 + 评测调度（负载均衡）+ 异步并发评测队列 + 推荐算法
- `compile_server`：编译运行服务节点（可部署多个端口模拟多节点）
- `comm`：公共工具（文件、时间戳、字符串切分等）

本仓库已增强：

- **并发评测**：`oj_server` 内置“任务队列 + 工作线程池”，支持异步提交、实时状态查询（PENDING/RUNNING/DONE）。
- **推荐算法**：基于历史提交记录（Accepted）做物品协同过滤，提供推荐页面 `/recommend` 与 API `/api/recommend`。

## 环境依赖（Ubuntu/Debian）

需要的系统库（编译时）：

```bash
sudo apt-get update -y
sudo apt-get install -y g++ make libboost-all-dev libjsoncpp-dev libctemplate-dev
```

## 编译

在项目根目录：

```bash
make clean
make
```

生成：

- `compile_server/compile_server`
- `oj_server/oj_server`

## 运行（本机演示）

### 1）启动多个评测节点（3 个端口示例）

开 3 个终端分别执行：

```bash
cd compile_server
./compile_server 8081
```

```bash
cd compile_server
./compile_server 8082
```

```bash
cd compile_server
./compile_server 8083
```

### 2）配置评测节点列表

编辑 `oj_server/conf/service_machine.conf`：

```text
127.0.0.1:8081
127.0.0.1:8082
127.0.0.1:8083
```

### 3）启动 Web 服务（开发模式建议前台运行）

```bash
cd oj_server
./oj_server --no-daemon --port 8080
```

浏览器打开：

- 首页：`http://localhost:8080/`
- 题库：`http://localhost:8080/all_questions`
- 推荐：`http://localhost:8080/recommend`

提交代码后会显示任务 ID，并实时轮询展示评测状态与结果。

## 关键接口

- `POST /judge_async/{number}`：提交评测任务（返回 `task_id`）
- `GET /task/{task_id}`：查询任务状态与结果
- `POST /judge/{number}`：同步评测（保留兼容）
- `GET /recommend`：推荐页面
- `GET /api/recommend`：推荐数据 JSON

## 数据与日志

- 历史提交记录：`oj_server/data/submissions.jsonl`（用于推荐算法）
- 日志：`oj_server/oj_server.log`、`compile_server/compile_server.log`

## 打包发布

```bash
make output
```

输出目录：`output/`（包含网页资源、题库、二进制等）。