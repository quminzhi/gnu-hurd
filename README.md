# GNU Hurd / Mach / seL4 学习资料与实验

面向有计算机本科基础、希望从宏内核转向理解微内核的学习者。项目包含中文导读、论文与官方文档快照，以及三个可在 macOS/Linux 编译运行的 C 实验。

## 从这里开始

1. 阅读 [学习参考路线](docs/guides/study-plan.md)，按步骤安排阅读与实验。
2. 打开 [代码实验说明](docs/labs/posix/README.md)，理解请求/回复、能力式授权、通知与共享数据。
3. 在项目根目录运行：

   ```sh
   make run
   ```

4. 带着实验中的问题阅读 [Hurd/Mach 导读](docs/guides/learning-guide.md) 和 [seL4 学习路线](docs/guides/sel4-learning.md)。
5. 回到 [微内核对照](docs/guides/microkernel-comparison.md)，区分历史选择、共同问题和具体实现。

C 实验使用 POSIX API 展示机制，真实 Hurd/Mach 与 seL4 API 的对应关系见实验说明。实际 seL4 教程的构建环境与版本以所用官方教程为准。

## 目录结构

```text
hurd/
├── README.md                    项目入口与运行方法
├── Makefile                     统一构建、运行、清理入口
└── docs/
    ├── README.md                资料库总索引
    ├── guides/                  中文学习路线、对照和文献注释
    │   ├── README.md
    │   ├── study-plan.md        逐步阅读与实验顺序
    │   ├── learning-guide.md    Hurd/Mach 学习主线
    │   ├── sel4-learning.md     seL4 学习主线
    │   ├── microkernel-comparison.md
    │   └── bibliography.md
    ├── labs/                    实验导航
    │   ├── README.md
    │   └── posix/               三个可运行 C 实验与 Makefile
    │       └── build/           make 自动生成的程序与调试文件
    ├── papers/                  Hurd、Mach、L4、seL4 论文原文
    ├── references/              参考手册与 Hurd Hacking Guide
    ├── sources/                 官方网页快照、历史示例和来源元数据
    ├── talks/                   进展报告与入门讲义
    ├── assets/images/           Markdown 使用的 PNG 与图表源码
    ├── SOURCES.md               本地原文与在线来源索引
    ├── sources-manifest.json    版本、获取状态、大小与 SHA-256
    └── references.bib           BibTeX 文献条目
```

查论文前先看 [带注释的文献目录](docs/guides/bibliography.md)。原始资料的来源和版本见 [来源索引](docs/SOURCES.md)，历史文献与近期实现的区别见 [资料库说明](docs/README.md)。

## 运行代码

需要 C 编译器、make 和 POSIX 环境。以下命令均在项目根目录执行：

```sh
make          # 编译三个实验
make run      # 编译并依次运行三个实验
make clean    # 清理生成的程序和调试文件
```

也可以直接调用实验目录的 Makefile：

```sh
make -C docs/labs/posix run
```

程序生成在 `docs/labs/posix/build/`。例如单独运行 RPC 实验：

```sh
make
./docs/labs/posix/build/01_rpc_socket
```

中文导读采用 Markdown；图表使用本地 PNG，可在 VS Code Markdown 预览中阅读。官方 HTML 和 `.mdwn` 快照的导航、语法与样式说明见 [资料库说明](docs/README.md)。
