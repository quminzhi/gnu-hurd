# Mach、GNU Hurd、L4 与 seL4：历史选择和共同问题

[导读目录](README.md) · [资料库索引](../README.md) · [代码实验](../labs/README.md)

比较这些系统时，先把时间、目标和层次分开。Mach 是 1980 年代开始的研究系统；GNU Mach 是 GNU Hurd 使用的 Mach 分支；Hurd 是建立在微内核之上的用户态服务器集合；L4 是后来重新强调最小内核和 IPC 性能的一条研究与产业路线；seL4 是 L4 家族中以 capability 和形式化验证著称的一个内核。它们不是同一个操作系统，也不能因为都使用“微内核”一词就互换结论。

**最有用的区分是：问题长期存在，解决它的具体 API 和内核分工可以改变。** IPC、分页、授权和资源管理不会因离开 Mach 而消失；port 消息队列、memory-object 协议、MIG 和 Hurd translator 则是具体方案。这里的“历史选择”不等于“已经过时”。资料核对日期：2026-10-08。

## 一张总览表

| 维度 | Mach / GNU Mach + Hurd | seL4（不能泛指所有 L4） |
| --- | --- | --- |
| 主要历史目标 | 在多处理器、分布式研究和 Unix 兼容之间寻找通用基础；Hurd 进一步追求可替换、可组合的 Unix 服务 | 缩小内核机制、降低 IPC 和切换开销，并把安全边界与系统组合表达得更明确 |
| 基本 IPC 抽象 | port、send/receive/send-once right、消息；MIG 生成 RPC 桩代码 | endpoint、同步 IPC、notification、reply capability；接口生成由上层工具或手写协议完成 |
| 对象命名 | 每个 task 的 port-name 空间，整数只是本地名称 | CSpace 中的 CNode 槽位和 capability；槽位组织成层次化命名空间 |
| 虚拟内存 | task 地址空间、memory object、external pager、copy-on-write | VSpace、frame、page table 对象和用户态 pager；物理内存通常由 untyped retype 产生 |
| 服务分解 | Mach 上可放 Unix server；Hurd 将文件系统、auth、proc 等进一步拆成多个服务器和 translator | 内核之外的系统服务由平台或应用选择；Microkit/CAmkES 便于静态组件组装 |
| Unix 命名空间 | translator 把用户态服务接入路径名；POSIX UID/GID 与 Mach port 权利并存 | 没有内置 Hurd translator 语义；Unix 兼容层是用户态系统的设计问题 |
| 资源表达 | 早期多服务器设计需要额外处理服务间资源归责、缓存和撤销 | capability、untyped、scheduling context 使对象和部分资源边界更显式，但政策仍需用户态设计 |
| 调度 | Mach 内核提供线程调度和优先级等机制 | seL4 内核仍调度；MCS 增加 scheduling context、预算和时间隔离机制 |
| 证明重点 | 经典 Mach 论文主要讨论机制、可移植性和性能，不提供 seL4 式完整内核 refinement 证明 | Isabelle/HOL 证明链覆盖特定内核配置和性质；用户态系统需另行证明或分析 |

L4 是一个演化中的家族：早期 L4 的线程标识、映射模型和 IPC 与现代 seL4 不同，也不能把所有 L4 实现都归为同一种 capability 内核。具体演变参见 [L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf) 第 2、3 节。

## 快速判断：什么是历史方案，什么是共同问题

| 长期问题 | Mach 的具体方案 | Hurd 的上层选择 | seL4 的具体方案 |
| --- | --- | --- | --- |
| 受保护的跨域通信 | port rights、有缓冲消息队列；RPC 可等待 reply port | 用 MIG 与 `.defs` 实现文件系统、认证等 RPC 协议 | 同步 endpoint IPC、notification、reply 权利；大数据通常用共享内存 |
| 内存与分页策略放在哪里 | 内核 VM object、映射、COW，与 external pager 的 memory-object 协议协作 | 文件服务器经 libpager 提供内存对象内容 | 显式 frame/VSpace 操作与 fault IPC；untyped/retype 管对象内存 |
| 谁能访问哪个服务 | task 私有 port-name 空间及 port rights | 对象引用与 auth、UID/GID、POSIX 权限一起组成授权协议 | CSpace、带权利的 capability、派生/撤销；身份语义仍由服务定义 |
| 服务如何发现和组合 | 通用 IPC，不规定 Hurd 路径语义 | translator 与 active/passive 绑定接入文件系统命名空间 | 内核不规定文件系统；Microkit/capDL 等配置组件连接 |
| 谁支付资源消耗 | 内核和服务器分担管理，资源归责需另行设计 | 多服务委托和缓存使归责成为架构难点 | 显式内存授权；MCS 以 scheduling context 表达 CPU 预算和捐赠 |
| 内核与驱动边界 | 经典 Mach 3/GNU Mach 包含内核设备驱动 | 用户态服务不意味着驱动全部在用户态；近期另有 rump 驱动工作 | 常将驱动放在用户态，以设备内存和 IRQ 授权；DMA/IOMMU 仍需分析 |

因此，**user-level pager 是跨时代的设计议题，Mach memory object 是一种具体实现；细粒度授权是共同议题，seL4 CSpace/untyped 则是它的具体做法。** Translator 属于 Hurd 的系统服务设计，不能归给 Mach 内核。

## Mach 时代的历史选择

下面的选择解释了 Mach/Hurd 的形状，但不应自动当成现代微内核的最佳答案。

1. **以 task/thread/port/message 和有缓冲的 port IPC 作为核心。** 接收者未立即等待时，Mach 可以把消息放进有界端口队列；队列满时发送也可能等待。seL4 的典型阻塞 endpoint IPC 采用会合，端点排队的是等待线程，不是 Mach 式独立消息邮箱。二者都可以做 RPC，不能简单等同为“Mach 异步、seL4 同步”，更不能把某种 IPC 语义视为微内核的必然定义。
2. **用 MIG 生成 RPC 接口。** 把接口描述、消息编解码和 demux 桩代码交给生成器，适合大量系统服务共享 Mach 消息约定。MIG 减少样板代码，却不会自动解决授权检查、并发、回收和服务策略。
3. **内核 VM object 与 external pager 的分工。** Mach 将一部分存储/供页策略交给用户态，内核仍维护丰富的 VM 机制，例如映射、COW 和内存对象状态。memory-object 消息协议、out-of-line 数据与 VM 的结合属于这条路线。把分页策略放到用户态的思路仍适用于现代系统；seL4 通过 frame、页表操作和 fault IPC 提供另一种边界，并不实现同一套 Mach pager 协议。
4. **Mach 的 Unix server 路线，以及 Hurd 的多服务器选择。** Mach 3 可将 Unix 实现为用户态服务；Hurd 进一步拆分文件系统、认证、进程元数据和程序加载等职责。这是两个层次的决定。边界变细可提高隔离和替换粒度，也增加跨服务协议、状态一致性、资源归责和恢复工作，实际效果取决于分解方式。
5. **Hurd 的 translator 路径名空间。** Hurd 让用户态服务成为节点的 active/passive translator，使 `open`、`read` 等普通 POSIX 程序可以访问新对象。这体现 GNU 对用户可扩展命名空间的重视；seL4 不要求、也不禁止在它上面构造这种服务模型。
6. **Hurd 的能力式引用与 Unix 身份组合。** Hurd 依赖 Mach 对象引用，并保留 UID/GID、auth server 和 POSIX 权限。持有一个 port right、通过 auth 获得某个身份、以及服务允许某项操作，是三个需要分别检查的事实。seL4 的对象能力模型也不会自动消除服务层的用户身份和审计需求。
7. **关注分布式 IPC 和跨机器透明性。** 早期 Mach 文献讨论 NORMA 等方向，反映当时网络工作站和分布式系统的研究问题。现代系统仍关心 IPC，但通常把本机低延迟、硬件隔离、实时性和可信执行边界放在更突出的位置。
8. **在当时硬件条件下取舍通用性和性能。** Mach 论文中的内存拷贝、缓存和上下文切换数据来自特定硬件、编译器和服务结构。它们适合说明“结构会影响性能”，不适合直接当作今日 x86、ARM 或 seL4 的绝对基准。
9. **经典 Mach 的内核驱动边界。** 设备驱动是否进入内核，是具体实现选择，不能从“微内核”三个字推出答案。GNU Mach 的历史内核驱动与 Hurd 近期 rump 用户态驱动工作也应分别看待。

这些选择有其历史合理性。评价 Hurd 时，应区分“为了可扩展 Unix 命名空间而付出的复杂度”和“某个实现尚未完成或性能不足”这两类问题；2007 年 Hurd critique 是有年代背景的架构分析，不应直接读成 2026 年实现状态报告。

## 现代微内核普遍关注的问题

这些议题跨越不同微内核与其上层系统；具体机制可能完全不同，也并非每个项目都同样重视每一项。Genode 是可运行在不同内核之上的系统框架，Pistachio 是 L4 实现，不能都当作同层的内核比较。

| 共同问题 | 要问的技术问题 | seL4 中的对应例子 |
| --- | --- | --- |
| IPC 成本 | 一次请求需要几次切换、拷贝和缓存失效？常见路径能否优化？ | endpoint、message registers、IPC buffer、fastpath |
| 最小可信计算基 | 哪些代码必须由内核保护和信任？驱动、文件系统和协议栈能否隔离？ | capability 检查、地址空间和线程机制；用户态服务不自动纳入内核证明 |
| 细粒度授权 | 权限如何传递、限制、撤销和审计？对象引用能否避免全局身份检查？ | CSpace、CNode 派生、badge、reply capability |
| 地址空间与 pager | 页表、缺页、共享内存和缓存策略由谁决定？故障能否局部处理？ | VSpace/frame 对象和用户态 fault/pager 协议 |
| 资源分配与归责 | 内存、端点、线程、CPU 时间耗尽时，能否知道责任归属并阻止 DoS？ | untyped/retype、显式 capability、MCS scheduling context |
| 调度和实时性 | 优先级反转、预算耗尽、时间隔离和 WCET 如何表达？ | priority、scheduling context、MCS、时间保护研究 |
| 多核与硬件 | SMP、缓存一致性、IOMMU、设备中断和启动代码如何进入证明或信任边界？ | verified configurations 按架构和配置区分覆盖范围 |
| 故障隔离 | 服务崩溃后能否重启而不破坏其他域？状态和能力如何恢复？ | protection domain 的用户态架构；恢复策略由系统组件决定 |
| 系统组合 | 如何描述组件、连接、资源和初始 capability，避免手工启动错误？ | capDL、CAmkES、Microkit |
| 可验证性 | 抽象规范、C 实现、编译器、二进制和硬件之间的证明链到哪里结束？ | Isabelle/HOL refinement、translation validation、verified configurations |
| 真实性能 | IPC 微基准是否代表整机工作负载？隔离、安全检查和缓存影响多少？ | 官方性能数据与具体平台配置一起解读 |

“现代共同关注”不等于“所有系统都已经解决”。例如 capability 能表达授权，却不能自动完成资源预算、撤销策略、用户身份映射或跨服务审计；用户态 pager 能提高可替换性，却可能使故障路径、内存压力和实时性分析更复杂。

还要区分两种“时间隔离”：MCS 预算主要限制 CPU 执行资源的使用与归责；[Time protection](../papers/sel4-2019-time-protection.pdf) 研究缓存等共享微架构状态造成的时间信息泄漏。预算控制不能自动排除时间侧信道，功能正确性和信息流证明也必须按它们的模型解读。完整的机器检查证明是 seL4 的突出贡献，并非所有现代微内核都具备的属性。

## seL4 的具体设计和研究贡献

下列内容是 seL4/L4 路线的具体机制或研究成果，不能泛化成微内核的唯一形式：

- CSpace/CNode 与 VSpace 的具体对象模型。Capability 本身早于 Mach 和 seL4；现代内核也不全采用 seL4 的派生/撤销规则。
- untyped memory 和 `retype`：由拥有相应内存 capability 的组件创建 TCB、CNode、frame 等对象。
- endpoint、按位合并的 notification、badge 和 reply capability 的具体 IPC API；MCS 使用显式 reply object，不能混用基础内核的调用形式。
- MCS 配置中的 scheduling context、时间预算和混合关键级支持。
- 以 Isabelle/HOL 为中心的抽象规范、可执行规范、C 实现和（部分配置的）二进制验证链。
- `capDL`、CAmkES 和 Microkit 这类系统组装工具与框架。
- 官方列出的 verified configurations：验证性质随架构、内核配置和 fastpath 状态变化，不能用一个“seL4 已验证”的标签替代配置检查。

## 宏内核背景下的概念映射

| 宏内核直觉 | 微内核中需要改写的问题 |
| --- | --- |
| 系统调用进入一个共享内核，内核直接找到文件系统对象 | 客户端先持有服务 endpoint capability，通过 IPC 调用用户态文件服务；名字解析和服务状态由用户态决定 |
| 文件描述符是进程表中的整数 | capability 是受保护对象引用；整数槽位只有在当前 CSpace 中才有意义 |
| 页表和缺页处理通常由内核统一完成 | 内核提供映射和故障机制，pager/内存管理器可以是用户态组件 |
| UID/GID 是主要访问控制线索 | capability 给出对象和操作授权；身份、策略、审计仍可由服务层实现 |
| 内核线程调度是一个全局策略 | seL4 也有内核调度算法；用户态通过被授权的优先级、预算和拓扑配置行为，不能据此说内核完全没有策略 |
| 驱动通常运行在内核地址空间 | 驱动可以作为隔离的用户态域；硬件访问通过显式 frame、IRQ、I/O 或 DMA 能力授权 |
| 内核升级就是系统升级 | 微内核、启动组件、驱动、服务和系统描述分别有版本和证明/测试边界 |

## 讨论题

1. 如果把 Hurd translator 改写成 seL4 protection domain，需要哪些 endpoint、frame、线程和启动 capability？路径名解析服务放在哪里？
2. Mach external pager 和 seL4 user-level pager 都把策略放到用户态；它们在对象命名、内存所有权、故障协议和证明边界上分别有什么不同？
3. Hurd 的 auth server 发行 UID/GID 与 Mach port rights 组合，seL4 capability 传递能替代其中哪些步骤？哪些身份和审计需求仍需用户态服务？
4. 一个 IPC fastpath 的性能改进可能增加哪些验证条件、缓存侧信道或调度假设？
5. 当文件服务或驱动耗尽内存时，哪个组件可以证明“责任属于它”？如果资源只通过全局分配器发放，最小权限原则是否足够？

建议把每道题写成“机制、策略、假设、可测量结果”四栏。这样可以避免把 Mach 的历史选择、seL4 的具体 API 和现代微内核的共同问题混成同一个结论。

## 这些判断对应哪些原始资料

| 判断 | 建议核对的资料 |
| --- | --- |
| Mach port 队列、权利和消息 | [A Revised IPC Interface](../papers/mach-ipc.ps)、[GNU Mach 手册](../references/gnu-mach-reference-manual.pdf) IPC 章节 |
| Mach VM 与用户态供页的分工 | [Duality of Memory and Communication](../papers/mach-dual-mem-com.ps)、[Machine-Independent VM](../papers/mach-MIvmm.ps) |
| Unix server 与多服务器分解不同 | [Unix as an Application Program](../papers/mach-mach3_intro.ps)、[Hurd 原始设计](../sources/gnu-hurd/hurd-paper.html) |
| Hurd 命名、auth 和资源难点 | [Hurd 2007 Critique](../papers/walfield-brinkmann-2007-hurd-critique.pdf)、[auth 说明](../sources/gnu-hurd/hurd/documentation/auth.mdwn) |
| L4 的性能、调度和最小化如何演变 | [L4 20 年综述](../papers/heiser-elphinstone-2016-l4-lessons.pdf) 第 3 节，尤其 3.1–3.3 |
| seL4 API 与 MCS 的具体语义 | [seL4 16.0.0 手册](../references/sel4-manual.pdf) 第 2–8 章、[调度上下文论文](../papers/sel4-2018-scheduling-contexts.pdf) |
| 证明边界与当前配置 | [2014 证明栈论文](../papers/sel4-2014-comprehensive-verification.pdf)、[官方配置页快照](../sources/sel4/verified-configurations.html) |

比较方法和学习顺序是本资料库的综合建议；具体机制与历史事实应以上述原文及对应版本源码为准。继续实践见 [seL4 学习路线](sel4-learning.md)。
