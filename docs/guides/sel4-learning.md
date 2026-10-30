# seL4 学习资料与路线

[导读目录](README.md) · [资料库索引](../README.md) · [代码实验](../labs/README.md)

这份路线面向已经熟悉宏内核基本概念、正在通过 GNU Hurd/Mach 学习微内核的读者。目标不是把 seL4 API 背下来，而是回答三个问题：内核究竟保留哪些机制？用户态如何用能力和 IPC 组装系统？形式化证明到底覆盖了哪一层？

**建议把 seL4 当作理解现代微内核的第二个参照系，与 Hurd 并行学习。** 不必先掌握 Isabelle 或读完全部证明论文，再继续研究 Hurd。前 6 周的对象、通信、内存、时间和组件边界最值得优先补齐；证明细节和另一套组件框架可以后置。

先运行 [可运行代码实验](../labs/posix/README.md)，再进入官方教程。实验使用 POSIX API 模拟机制，帮助理解控制流和权限边界；它们不等于真实 Mach IPC 或 seL4 内核。

资料核对日期：2026-10-08。本轮新增 8 篇研究原文、seL4 Reference Manual 16.0.0，以及 22 份官方 HTML 快照。全部入口见 [本地 seL4 资料导航](../sources/sel4/README.md)；论文注释见 [文献目录](bibliography.md)。本轮收集和检查了资料，尚未构建或运行教程。

## 先建立一个准确的模型

seL4 是 L4 家族中的一种能力（capability）微内核。内核管理少量受保护对象、地址空间、线程、IPC、调度和硬件入口；文件系统、网络协议栈、驱动、进程模型和 Unix 兼容层由用户态系统决定。seL4 的“最小”是相对于内核机制和可信计算基而言的，不等于一个安装后即可使用的完整操作系统。

内核仍然执行调度、上下文切换、地址空间切换和能力检查。用户态可以通过优先级、调度上下文、时间预算、端点协议和组件拓扑表达系统策略；“机制与策略分离”不表示调度被完全移出内核。

这里有两个容易混淆的缩写：对象表中的 **TCB = Thread Control Block（线程控制块）**；安全讨论中的 **TCB = Trusted Computing Base（可信计算基）**。后者是为某项性质必须信任的组件集合，也可能包含用户态服务。例如隔离不可信文件服务与相信它返回正确文件内容，是不同的安全目标。

| seL4 概念 | 作用 | 适合用来对照的 Mach/Hurd 概念 |
| --- | --- | --- |
| TCB / thread | 保存线程寄存器、优先级和调度状态 | Mach thread；不要把 TCB 当成完整 Unix 进程 |
| CNode / CSpace | 以槽位组织 capability，线程通过配置的 CSpace 查找能力；多个线程可以共享它 | task 的 port-name 空间；不是一一对应的进程模型 |
| VSpace | 地址空间的映射结构 | Mach task 的虚拟地址空间 |
| untyped memory | 可由授权者 retype 成内核对象或页的物理内存 | Mach 的内存对象/页框机制；资源归属表达更显式 |
| endpoint | 同步 IPC 的发送/接收对象 | Mach port 的消息端点；seL4 的 API 和权利模型不同 |
| notification | 按位 OR 合并的事件通知；不保存每次事件的独立消息，也不自动计数 | 没有一个一一对应的 Hurd 抽象 |
| reply capability | 一次调用的受保护回复权利 | Mach reply port 的相近用途，但生命周期和传递规则不同 |
| badge | 与能力关联、由内核交付的标签；可以标识授权通道，不能直接推出 PID 或 UID | 对照服务端的对象/客户端上下文；不是全局身份 |
| scheduling context | MCS 配置中的时间预算和调度资源 | Mach 的优先级/调度概念；不是 Hurd 的 auth ID |
| IRQ capability | 授权中断处理及与 notification 的绑定 | GNU Mach 的设备/中断路径；不强行寻找同名对象 |

用户传入的整数是 capability 的本地查找地址（CPtr），并非授权本身；猜中另一个 CSpace 的数字不会获得其权利。Capability 是受内核保护的对象引用和权限。文件描述符也可承担能力式授权，适合辅助理解“句柄指向对象”，但 seL4 将这一模型推广到线程、内存和 IPC 等内核对象。CNode 槽位、复制/移动/删除、派生和撤销是核心；删除一个槽位并不等于撤销所有其他引用。

endpoint IPC 通常采用同步会合：没有接收者时，阻塞发送会等待，不是先把任意消息放进内核消息邮箱。`Call` 发送请求并等待受保护的回复；`Send`/`Recv` 把发送与接收分开。notification 解决事件同步，大块数据通常使用共享内存和协议。基础内核与 MCS 的 reply API 不同，学习时要记录配置。

## 推荐的 10 周路线（另有可选预习）

每周约 4–6 小时，约 40–60 小时。每一阶段都留下一个产出。官方教程代码、构建参数和 SDK 会变化，运行时以对应版本文档为准。第 7 周的 CAmkES 以及第 8 周的安全证明细节可选，避免同时学习三套工具链。

本机是 macOS x86_64。低层教程的官方 [环境说明](../sources/sel4/tutorial-setting-up.html) 以 Ubuntu/Docker 为主，可在 Linux 虚拟机或官方构建容器中运行；[Microkit SDK 说明](../sources/sel4/microkit-setting-up.html) 提供 macOS x86_64 和 Arm 包。本轮快照列出的 SDK 是 2.3.1，下载时仍应核对当前 release。先用 QEMU 验证功能；模拟器中的延迟不适合作为论文的原生硬件性能数据。

每次实验记录内核/教程提交、`repo manifest -r` 导出的版本清单、目标架构、基础或 MCS 配置、SDK 版本、命令和输出。教程初始代码含待补全项，出现预设断言并不一定表示环境失败；完成练习后再与官方 solution 对照。

### 第 0 周：从宏内核切换到微内核问题

阅读：本资料库 [Hurd 学习路线](learning-guide.md) 第 1、2 阶段、[L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf) 的引言与设计教训。

练习：把 Linux 的 `open`、`read`、页故障、线程切换各拆成“内核机制”和“用户态策略”；再画 Mach/Hurd 的 `open → translator → RPC` 路径。产出是一页“宏内核 syscall 与微内核 IPC”的对照表。

### 第 1 周：官方教程与启动对象

阅读：[Setting up](../sources/sel4/tutorial-setting-up.html)、[Getting the tutorials](../sources/sel4/tutorial-get-the-tutorials.html)、[Hello World](../sources/sel4/tutorial-hello-world.html)。在线对应地址见 [资料导航](../sources/sel4/README.md)。

目标：知道 root task 如何获得初始 capability、内核如何通过启动信息把资源交给用户态，以及应用二进制、内核和库的边界。部分教程使用 capDL Loader 作为 root task，应用看到的是分配后的能力，不是全部启动资源。产出：成功构建和模拟 Hello World，并标出哪些启动工作由框架代办。

### 第 2 周：Capability 与对象模型

阅读：[Capabilities](../sources/sel4/tutorial-capabilities.html)、[Untyped](../sources/sel4/tutorial-untyped.html)、[Mapping](../sources/sel4/tutorial-mapping.html)；查 [参考手册](../references/sel4-manual.pdf) 第 2、3、7 章。

练习：画出 CSpace 的 CNode/槽位结构，以及另一张 capability 派生关系图，不要把两棵树混同。标出 endpoint 的权利、谁能创建派生能力；比较 `Delete` 与 `Revoke` 后哪些引用仍存在；用 untyped 创建对象、映射一页 frame。产出：解释“有对象引用”“可授权他人”“拥有它占用的内存”三个问题。

### 第 3 周：IPC、回复与事件

先读并练习 [Threads](../sources/sel4/tutorial-threads.html)，再读 [IPC](../sources/sel4/tutorial-ipc.html)、[Notifications](../sources/sel4/tutorial-notifications.html)。重点观察 endpoint、IPC buffer、message registers、capability transfer、badge、reply capability 和 fastpath 条件。

练习：完成官方 echo；记录 `Call → endpoint → handler → reply` 中每个线程持有的 capability 和阻塞位置。比较 badge=1 与 badge=2 的两个授权通道。再运行“共享内存 + notification”例子，解释连续两次相同 badge 的 Signal 为什么可能合并，精确计数应保存在哪里。产出：对象图、调用时序和实际输出。

### 第 4 周：地址空间、内存和 pager

阅读：[Interrupts](../sources/sel4/tutorial-interrupts.html)、[Fault handling](../sources/sel4/tutorial-fault-handlers.html)，回看 Mapping 和手册第 6–8 章。随后重读 Mach 的 [Duality of Memory and Communication](../papers/mach-dual-mem-com.ps) 和 Hurd 的 [libpager 说明](../sources/gnu-hurd/hurd/libpager.mdwn)。

练习：先完成 fault handler，再设计一个按需映射单页的扩展：“VM fault → fault endpoint → handler 获取 frame → 安装映射 → 合法回复/恢复”。这是在教程之上的扩展，不是现成磁盘分页系统。列出 handler 需要的 frame、页表和地址空间权限，预测没有权限或未修复映射便恢复时的结果。产出：故障时序图，以及内存资源归属表。

### 第 5 周：线程、调度和时间隔离

阅读：[MCS 教程](../sources/sel4/tutorial-mcs.html)、[Scheduling-context capabilities](../papers/sel4-2018-scheduling-contexts.pdf) 的模型和示例，以及 [Verified configurations](../sources/sel4/verified-configurations.html)。[Time protection](../papers/sel4-2019-time-protection.pdf) 可留到第 9 周。

目标：区分 priority、budget、period、阻塞、优先级反转，以及 passive server 的 scheduling-context donation；优先级、实时 deadline 与故障严重性也不是同一维度。实验中改变预算，观察耗尽、补充和绑定/解绑后的行为。预算限定使用上界，不自动保证 deadline；还需可调度性与工作负载分析。产出：一个“客户端请求由谁的 CPU 预算支付”的例子。

基础 IPC 教程中的 `SaveCaller`、隐式 reply 状态，与 MCS 的显式 reply object 不可混用。截至本轮官方快照，ARM/AArch64/RISC-V 的相关 MCS 证明仍有进行中的工作；能跑 MCS 实验不意味着该配置的全部证明已经完成。

### 第 6 周：故障、设备和系统组装

阅读：[Microkit 入门](../sources/sel4/microkit-tutorial.html)、[SDK 环境](../sources/sel4/microkit-setting-up.html)、[Microkit 手册](../sources/sel4/microkit-manual.html)。

练习：构造两个 protection domain（PD），通过通知或 protected procedure call 通信；列出私有内存、共享内存、连接、IRQ 和时间配置。故意让一个 PD 非法访问内存，观察故障是否局限于它、另一个 PD 是否仍可执行。Microkit monitor 报告故障不等于自动恢复服务；重启与状态恢复需要额外设计。产出：一个可重建的小系统及故障记录。

### 第 7 周：capDL 与可组合系统（CAmkES 可选）

阅读：[capDL 项目](../sources/sel4/capdl.html)、[capDL Loader](../sources/sel4/capdl-loader.html)、Microkit 的系统描述。只在需要研究另一套组件框架时选读 [CAmkES](../sources/sel4/camkes.html)。

目标：理解内核 API、初始 capability 分配和应用组件协议三层。Microkit/CAmkES 是两种系统构造入口，无须串行精通；capDL 描述底层对象和能力布局。生成或检查连接不等于业务授权、所有生成代码和用户态应用都已证明。产出：把第 6 周系统的描述文件、对象图和安全策略对齐。

### 第 8 周：形式化证明链

必读：[2009 SOSP 论文](../papers/sel4-2009-formal-verification.pdf) 的模型、结果和假设，以及 [2014 全面证明论文](../papers/sel4-2014-comprehensive-verification.pdf) 的 proof stack 总览；70 页正文用作参考，无须一周逐页读完。按兴趣选读 [integrity](../papers/sel4-2011-integrity.pdf)、[information flow](../papers/sel4-2013-information-flow.pdf)、[translation validation](../papers/sel4-2013-translation-validation.pdf)。

读论文时画出“抽象规范 → 可执行规范 → C 实现 → 编译后二进制”的 refinement 链，并在每一步写下假设。证明的是特定内核配置和属性；用户态文件系统、驱动、协议栈、应用和硬件环境仍需单独分析。`verified configurations` 页面列出的配置、架构和 fastpath 状态，应优先于论文中的历史配置描述。

| 性质 | 能回答的问题 | 不能直接推出的结论 |
| --- | --- | --- |
| Functional correctness | 实现是否精化了给定抽象规范？ | 规范满足所有业务安全目标 |
| Integrity / authority confinement | 状态修改、权限传播是否被给定政策限制？ | 任何随意授权的组件图都安全 |
| Confidentiality / information flow | 在模型和配置假设内，是否不存在被禁止的信息流？ | 所有微架构时间侧信道都被排除 |
| Binary correctness | 特定编译后二进制是否保持已证明的 C 语义？ | 任意编译器、平台和编译选项都被覆盖 |
| WCET | 特定路径/硬件模型下最坏执行时间的上界如何建立？ | 整个用户态系统都能满足 deadline |

本轮配置页明确列出：AArch64 的相应配置有 C 级功能正确性、访问控制和信息流证明，包含 fastpath；X64 相应配置为 C 级功能正确性且不含 fastpath。启动、IOMMU 设备地址转换及调试/分析接口等不在该页所列已验证功能中。产出：针对一个确切配置写“已证明、假设、尚未覆盖”三栏，不笼统写“全部验证”。

### 第 9–10 周：性能与回到 Hurd/Mach

阅读：[L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf) 第 3 节、[Formally verified software in the real world](../papers/sel4-2018-real-world.pdf)，再回看本库的 [Mach/Hurd 对照](microkernel-comparison.md)。时间方向选读 [Time protection](../papers/sel4-2019-time-protection.pdf)、[WCET](../papers/sel4-2016-wcet.pdf)。

最终小项目：选择一个 Hurd 机制（Mach port、external pager、translator 或 auth），用 seL4 的 endpoint、capability、pager 或 protection domain 重画它；写出至少三处相似点、三处不能直接类比的地方，以及一个可测试的性能或安全假设。

建议首选“只向文本服务授予一页共享内存和一个通信端点”：画权限图，加入一个不可信客户端，再检查它能否越权访问另一个服务。比较协议设计即可，不必为了完成主线移植整个 Hurd。

## 资料分层

| 层次 | 资料 | 用法 |
| --- | --- | --- |
| 实践入口 | [seL4 Tutorials](https://docs.sel4.systems/Tutorials/)、[seL4 Manual](https://docs.sel4.systems/Info/Docs/seL4-manual-latest.pdf) | 先跑小实验，再查 API 和启动协议 |
| 系统构造 | [Microkit](https://docs.sel4.systems/projects/microkit/)、[CAmkES](https://docs.sel4.systems/projects/camkes/)、[capDL](https://docs.sel4.systems/projects/capdl/) | 学习组件、资源边界和静态拓扑 |
| 设计背景 | [L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf)、[On Micro-Kernel Construction](https://doi.org/10.1145/224056.224075) | 了解 L4 对 Mach 性能和最小化争论的回应 |
| 证明基础 | [2009 SOSP](../papers/sel4-2009-formal-verification.pdf)、[2014 proof stack](https://sel4.systems/Research/pdfs/comprehensive-formal-verification-os-microkernel.pdf) | 理解 refinement、Isabelle/HOL 和证明边界 |
| 安全性质 | [integrity](https://sel4.systems/Research/pdfs/sel4-enforces-integrity.pdf)、[confidentiality](https://sel4.systems/Research/pdfs/sel4-from-general-purpose-to-proof-information-flow-enforcement.pdf) | 区分完整性、机密性和功能正确性 |
| 时间性质 | [MCS](https://sel4.systems/Research/pdfs/scheduling-context-capabilities.pdf)、[time protection](https://sel4.systems/Research/pdfs/time-protection-missing-os-abstraction.pdf)、[WCET](https://sel4.systems/Research/pdfs/complete-high-assurance-determination.pdf) | 研究实时性、隔离和可预测性 |
| 当前索引 | [官方 publications](https://sel4.systems/Research/publications.html)、本地 [HTML 快照](../sources/sel4-publications.html) | 按主题继续扩展，不把索引条目误当成已下载全文 |

## 每篇论文的阅读记录模板

```text
资料：
它试图解决的问题：
系统边界：哪些在内核，哪些在用户态：
核心机制：
安全/性能/验证性质：
明确写出的假设：
与 Mach/Hurd 的相似处：
与 Mach/Hurd 不能直接类比之处：
可以运行或检查的实验：
仍然不理解的问题：
```

## 容易产生的误解

- “seL4 已经验证，所以基于 seL4 的操作系统全部验证”是错误的。内核证明、特定配置证明、用户态组件证明和整机安全论证是不同层次。
- “capability 就是文件描述符”只在“都是进程持有的句柄”这一层面相似。seL4 capability 由内核保护并参与对象授权和派生；文件描述符通常只是 Unix 服务中的一种命名约定。
- “所有策略都在用户态”也不准确。调度、地址空间切换、能力检查等关键机制仍由内核实施；用户态配置策略并组合服务。
- “Mach port 等于 seL4 endpoint”过于简单。两者都支持受保护的 IPC 对象，但 Mach 的 port rights、消息语义和 task port-name 空间，与 seL4 CSpace、reply capability 和 endpoint API 不是同一个模型。
