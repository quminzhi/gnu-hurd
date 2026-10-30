# Hurd / Mach / seL4 学习参考路线

[导读目录](README.md) · [资料库索引](../README.md) · [代码实验](../labs/README.md)

这份文件是当前资料库的操作指南。建议按顺序阅读，不要从 `papers/` 目录随机开始。每一阶段先读概念，再运行一个小实验，最后回到论文或源码核对。

## 资料库地图

| 位置 | 用途 |
| --- | --- |
| [README.md](../README.md) | 总入口、资料范围和历史语境 |
| [learning-guide.md](learning-guide.md) | Hurd/Mach 主线：服务分解、Mach port、MIG、translator、权限、VM |
| [sel4-learning.md](sel4-learning.md) | seL4 主线：capability、IPC、内存、调度、Microkit、证明 |
| [microkernel-comparison.md](microkernel-comparison.md) | Mach、Hurd、L4、seL4 横向比较 |
| [labs/posix/](../labs/posix/) | 可在 macOS/Linux 直接编译运行的 POSIX 小实验 |
| [bibliography.md](bibliography.md) | 论文和手册的中文注释、难度和讨论问题 |
| [papers/](../papers/) | 论文原文 |
| [references/](../references/) | GNU Mach、seL4 参考手册和 Hurd Hacking Guide |
| [sources/gnu-hurd/](../sources/gnu-hurd/) | GNU Hurd 官方文档快照 |
| [sources/sel4/README.md](../sources/sel4/README.md) | seL4 官方教程、Microkit、capDL、CAmkES 快照导航 |
| [SOURCES.md](../SOURCES.md) | 来源和本地文件索引 |

## 推荐顺序

### 第 0 步：建立地图，约 30 分钟

阅读：

1. [README 的“从这里开始”](../README.md#从这里开始)。
2. [seL4 学习路线的概念模型](sel4-learning.md#先建立一个准确的模型)。
3. [Mach/Hurd 与 seL4 的快速对照](microkernel-comparison.md#快速判断什么是历史方案什么是共同问题)。

回答三个问题：Hurd 服务和 GNU Mach 分别负责什么？seL4 内核保留什么？Mach port 与 seL4 capability/endpoint 有哪些相似和不同？

产出一张四层图：应用、用户态服务、微内核、硬件。

### 第 1 步：建立 Hurd/Mach 背景，约 2–3 小时

阅读 [learning-guide.md](learning-guide.md) 第 1 阶段，然后看：

1. [Towards a New Strategy of OS Design](../sources/gnu-hurd/hurd-paper.html)。
2. [The Hurd 演讲文本](../sources/gnu-hurd/hurd-talk.html)。
3. [Translator Primer](../sources/gnu-hurd/hurd/documentation/translator_primer.mdwn)。

目标是解释：普通程序调用 `open/read/write` 时，用户态服务器如何提供对象，translator 为什么能够接入路径名空间。暂时不读完整 Hurd 源码。

### 第 2 步：先用可运行代码建立 RPC 直觉，约 1 小时

进入 [实验说明](../labs/posix/README.md)，在项目根目录运行：

```sh
make run
```

先读 `01_rpc_socket.c`，观察 client/server、请求/回复和阻塞读写；再回看 seL4 [IPC 教程](../sources/sel4/tutorial-ipc.html) 和 Mach [IPC 论文](../papers/mach-ipc.ps)。

记住：Unix socket 是模拟工具，不是 Mach IPC 或 seL4 endpoint。代码的价值是让请求/回复边界先变得可见。

### 第 3 步：学习 seL4 对象、启动和 capability，约 3–5 小时

按顺序看：

1. [Setting up](../sources/sel4/tutorial-setting-up.html)。
2. [Hello World](../sources/sel4/tutorial-hello-world.html)。
3. [Capabilities](../sources/sel4/tutorial-capabilities.html)。
4. [Untyped](../sources/sel4/tutorial-untyped.html)。
5. [Mapping](../sources/sel4/tutorial-mapping.html)。

同时运行 `02_capability_table.c`。画出 CSpace、CNode、TCB、VSpace、endpoint、frame 和 untyped 的关系图。要区分：capability 是受内核保护的对象引用，slot 是当前 CSpace 中的本地查找地址，untyped 是可 retype 的资源。

### 第 4 步：学习 IPC、回复和通知，约 3–5 小时

按顺序看：

1. [Threads](../sources/sel4/tutorial-threads.html)。
2. [IPC](../sources/sel4/tutorial-ipc.html)。
3. [Notifications](../sources/sel4/tutorial-notifications.html)。
4. Hurd 的 [Mach port 文档](../sources/gnu-hurd/microkernel/mach/port.mdwn)。
5. [A Revised IPC Interface](../papers/mach-ipc.ps)。

用这条时序追踪实验：

```text
client → Call(endpoint) → server Recv → handler → Reply → client
```

记录每一步谁拥有 capability、谁阻塞、badge 如何区分客户端。然后运行 `03_notification_shared_memory.c`，观察 notification 表达事件而共享内存保存数据。

### 第 5 步：学习地址空间、故障和 pager，约 3–5 小时

阅读 [Interrupts](../sources/sel4/tutorial-interrupts.html)、[Fault handlers](../sources/sel4/tutorial-fault-handlers.html)、Mach [memory object 论文](../papers/mach-dual-mem-com.ps) 和 Hurd [libpager 文档](../sources/gnu-hurd/hurd/libpager.mdwn)。

画出：

```text
VM fault → fault endpoint → 用户态 pager → frame → mapping → resume
```

产出一张“内核检查 / pager 策略 / 所需 capability”表。这个练习是用户态按需映射示意，不等于完整磁盘分页系统。

### 第 6 步：学习调度和时间隔离，约 2–4 小时

阅读 [MCS 教程](../sources/sel4/tutorial-mcs.html)、[Scheduling-Context Capabilities](../papers/sel4-2018-scheduling-contexts.pdf) 和 [Verified Configurations](../sources/sel4/verified-configurations.html)。

区分 priority、budget、period、scheduling context、passive server 和 priority inversion。预算给出 CPU 使用上界，不自动保证 deadline；基础 IPC 的隐式 reply 状态与 MCS 的显式 reply object 也不能混用。

### 第 7 步：学习系统组装，约 3–5 小时

阅读 [Microkit 入门](../sources/sel4/microkit-tutorial.html)、[Microkit 手册](../sources/sel4/microkit-manual.html)、[capDL](../sources/sel4/capdl.html) 和 [capDL Loader](../sources/sel4/capdl-loader.html)。需要时再看 [CAmkES](../sources/sel4/camkes.html)。

构造两个 protection domain，通过 notification 或共享内存通信；列出各自需要的内存、endpoint、IRQ 和时间配置。让一个组件发生非法访问，观察故障边界。

要区分三层：

```text
内核 API → 初始 capability 分配 → protection domain → 用户态组件协议
```

构造工具不会自动证明所有用户态服务和业务策略。

### 第 8 步：学习形式化验证边界，约 4–8 小时

按顺序阅读：

1. [seL4 2009 SOSP 论文](../papers/sel4-2009-formal-verification.pdf)。
2. [Comprehensive Formal Verification，2014](../papers/sel4-2014-comprehensive-verification.pdf)。
3. [Integrity，2011](../papers/sel4-2011-integrity.pdf)。
4. [Information Flow，2013](../papers/sel4-2013-information-flow.pdf)。
5. [Translation Validation，2013](../papers/sel4-2013-translation-validation.pdf)。

先读摘要、系统模型、证明范围和假设，不要一开始通读 70 页长文。画出：

```text
抽象规范 → 可执行规范 → C 实现 → 编译后二进制
```

每一层都写“证明了什么、依赖什么、没有证明什么、适用于哪个配置”。内核证明不等于整个用户态操作系统证明。

### 第 9 步：回到 Hurd/Mach 完成对照，约 3–5 小时

阅读 [L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf) 第 3 节，再回看 [Mach/Hurd/seL4 对照](microkernel-comparison.md)。选择 Mach port、external pager、translator 或 auth 之一，用 seL4 endpoint、capability、pager 或 protection domain 重画。

最后写出三处相似点、三处不能直接类比的地方，以及一个可测试的性能或安全假设。建议从“文本服务 + 一页共享内存 + 一个 endpoint”开始，不要一开始移植整个 Hurd。

## 每次阅读记录

```text
资料：
它试图解决的问题：
内核机制：
用户态策略：
权限/资源边界：
依赖的假设：
与 Mach/Hurd 的相似处：
不能直接类比之处：
可以运行或检查的实验：
我仍然不理解的问题：
```

## 今天从哪里开始

1. 阅读本文件的“资料库地图”和“第 0 步”。
2. 在项目根目录运行 `make run`。
3. 阅读 [Capabilities 教程](../sources/sel4/tutorial-capabilities.html)。
4. 画出 CSpace、TCB、endpoint、frame、VSpace 的关系图。
5. 回答：seL4 capability 与 Mach port right 都能授权访问对象，为什么它们不是同一个概念？

完成后再进入 IPC。这样可以避免同时被论文、MIG、Microkit 和 Isabelle/HOL 分散注意力。
