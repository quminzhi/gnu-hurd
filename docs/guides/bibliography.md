# 带注释的 Hurd 与微内核文献目录

[导读目录](README.md) · [资料库索引](../README.md) · [代码实验](../labs/README.md)

核对日期：2026-10-08。**必读**表示与理解 Hurd 直接相关；**进阶**表示适合在掌握整体结构后阅读；**比较**表示其他微内核路线。论文、技术文档、学位论文和演讲分别标明，避免把资料类型混在一起。

完整来源、固定版本与下载记录见 [SOURCES.md](../SOURCES.md) 和 [sources-manifest.json](../sources-manifest.json)。

## 一、理解 GNU 的设计目标

### 1. Towards a New Strategy of OS Design — Thomas Bushnell, BSG

**必读 · 设计文章 · 难度：低到中。** [本地原文](../sources/gnu-hurd/hurd-paper.html) · [GNU 在线版](https://www.gnu.org/software/hurd/hurd-paper.html)

最适合回答“GNU 希望借助微内核改变什么”。重点读 Part 1、Introduction to Translators、What the User Can Do，以及 auth、proc、exec 的职责。不要把文章中的未来时陈述当成当前实现清单；网页版权年份也不等于首次发表年份，因此这里不强行指定首发年。

**讨论问题：** 对普通用户而言，“能运行自己的服务”与“能把服务接入已有 POSIX 命名空间”有何区别？为什么后者对既有应用更有价值？

### 2. The Hurd — Marcus Brinkmann，2001

**必读 · 演讲文本 · 难度：中。** [本地原文](../sources/gnu-hurd/hurd-talk.html) · [GNU 在线版](https://www.gnu.org/software/hurd/hurd-talk.html)

重点看 How to Get a Port、Pathname Resolution Example、Mapping the POSIX Interface、Active vs Passive、Authentication。它能把“程序调用 `open()`”和“程序取得一个服务对象的 port right”联系起来。

**讨论问题：** 路径字符串在哪一步被解析为对象引用？一个打开的文件为什么可以理解为带状态的服务对象？

### 3. A Document About Translators — Marcus Brinkmann

**必读 · 技术说明 · 难度：低到中。** [本地原文](../sources/gnu-hurd/hurd/documentation/translators.mdwn) · [GNU 在线版](https://www.gnu.org/software/hurd/hurd/documentation/translators.html)

解释 translator、active/passive 的区别，以及文件系统、设备文件、符号链接如何统一到这种机制中。与 [Translator Primer](../sources/gnu-hurd/hurd/documentation/translator_primer.mdwn) 配合阅读。

**讨论问题：** passive translator 保存的是进程还是启动描述？它为什么可以跨重启保留，而 active translator 不行？

## 二、Hurd 的反思与后续研究

### 4. A Critique of the GNU Hurd Multi-Server Operating System — Neal H. Walfield、Marcus Brinkmann，2007

**必读 · 期刊论文 · 难度：中到高。** [本地 PDF，10 页](../papers/walfield-brinkmann-2007-hurd-critique.pdf) · [作者原文](http://walfield.org/papers/200707-walfield-critique-of-the-GNU-Hurd.pdf) · [DOI](https://doi.org/10.1145/1278901.1278907)

出处：*ACM SIGOPS Operating Systems Review*，41(4)，30–39，2007 年 7 月。

先读第 1、2 节的目标与架构，再读批评部分。重点是分布式命名、对象引用与身份权限的关系、资源管理，以及 Mach 对多服务器系统施加的约束。它比一般入门介绍更能说明设计的代价。

**讨论问题：** 一个系统使用不可伪造的对象引用，为什么仍不自动满足最小权限原则？把服务拆开后，资源消耗应如何归责？

**历史语境：** 作者参与过 Hurd/L4 工作，论文评价的是当时的设计。GNU 的 [Critique 页面](../sources/gnu-hurd/hurd/critique.mdwn) 还记录了后续讨论：部分问题可能在现有框架内解决。把事实观察与“需要怎样重构”的结论分别检验。

### 5. Improving Usability via Access Decomposition and Policy Refinement — Neal H. Walfield、Marcus Brinkmann，2007

**进阶 · 研究立场论文 · 难度：高。** [本地 PDF，5 页](../papers/walfield-brinkmann-2007-access-decomposition.pdf) · [作者原文](http://walfield.org/papers/20070104-walfield-access-decomposition-policy-refinement.pdf)

GNU 官方将它称为 position paper，用于讨论后续系统的架构。阅读 object-capability、access decomposition、policy refinement，以及应用如何参与资源管理。所下载版本为 2007 年 1 月的作者稿；不把它标为未经核实的会议论文。

**讨论问题：** 应用比内核更了解自己的缓存价值时，如何允许它提供策略信息，同时限制恶意应用占用资源？

### 6. Generalizing Mobility for the Hurd — Carl Fredrik Hammar

**进阶 · 学位论文 · 难度：中到高 · 仅链接。** [GNU 官方介绍](https://www.gnu.org/software/hurd/hurd/documentation.html) · [官方指向的原文地址](https://pdfs.semanticscholar.org/4cce/9abb177cc58c199e13b82d498f37010c2bfc.pdf)

GNU 的介绍说明，它研究 Hurd store 的 mobility 及其推广，背景章节适合新开发者。本次该 PDF 地址跳转后未返回有效论文文件，所以没有保存本地 PDF，也未推断年份或学位授予信息。

适合在理解 libstore 与 translator 后研究服务、存储抽象的可替换性和迁移能力。这里的说明依据官方介绍，未冒充全文评述。

## 三、Mach 的机制与经典论文

### 7. Unix as an Application Program — David Golub、Randall Dean、Alessandro Forin、Richard Rashid，1990

**必读 · USENIX 论文 · 难度：中。** [本地 PostScript](../papers/mach-mach3_intro.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/mach3_intro.html)

出处：1990 Summer USENIX Conference。它介绍在 Mach 3.0 上将 Unix 实现为多线程用户态程序的思路。读它，再比较 Hurd 的多服务器结构，可以看清“采用微内核”和“采用 Hurd 的服务分解方式”是两个层次的选择。

**讨论问题：** 微内核上的单 Unix server 与 Hurd 的多服务器系统分别获得了哪些边界？又有哪些职责仍然集中？

### 8. The Duality of Memory and Communication in the Implementation of a Multiprocessor Operating System — Michael Young 等，1987

**进阶 · SOSP 论文 · 难度：高。** [本地 PostScript](../papers/mach-dual-mem-com.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/dual-mem-com.html)

出处：第 11 届 SOSP，1987。重点是 memory object、external memory management，以及通过消息接口由用户程序管理内存对象。

**讨论问题：** 当缺页处理需要与用户态 pager 通信时，怎样理解内存机制与 IPC 的相互依赖？out-of-line 数据传递在哪些条件下可能减少复制？

### 9. Machine-Independent Virtual Memory Management for Paged Uniprocessor and Multiprocessor Architectures — Richard Rashid 等，1987

**进阶 · ASPLOS 论文 · 难度：高。** [本地 PostScript](../papers/mach-MIvmm.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/MIvmm.html)

出处：ASPLOS II，1987。研究虚拟内存中机器无关部分与机器相关支持的分离。先复习页表、地址空间与 copy-on-write，再读 VM object、映射及硬件相关模块。

**讨论问题：** 把虚拟内存政策、软件抽象、硬件页表支持区分开，分别改善了哪些可移植性问题？

### 10. A Revised IPC Interface — Richard P. Draves

**进阶 · Mach IPC 论文 · 难度：高。** [本地 PostScript](../papers/mach-ipc.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/ipc.html)

CMU 目录将其列为 1990 年 USENIX Mach Conference 论文；本地作者文件版权页标示 1991。保留这个版本差异。重点读 port rights、消息传递接口与接口设计约束，并对照 GNU Mach 手册查术语。

**讨论问题：** send right、receive right、send-once right 分别赋予什么能力？为什么只猜出一个 `mach_port_t` 整数不能凭空获得权限？

### 11. MIG — The MACH Interface Generator — Richard P. Draves、Michael B. Jones、Mary R. Thompson，1989

**进阶 · 技术文档 · 难度：中。** [本地 PostScript](../papers/mach-mig.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/mig.html)

本地版本标示 1989-11-02。读 subsystem、type、routine 定义，以及 client stub、server stub 的生成过程。将它与 Hurd 的 `.defs` 和 [RPC 教程](../sources/gnu-hurd/hurd/rpc.mdwn) 对照。

**讨论问题：** RPC 生成器解决哪些重复工作？它为什么不能替服务实现消除授权检查、线程同步和生命周期管理问题？

### 12. The Impact of Operating System Structure on Memory System Performance — J. Bradley Chen、Brian N. Bershad，1993

**比较 · SOSP 论文 · 难度：高。** [本地 PostScript](../papers/mach-os-memorysys.ps) · [CMU 摘要](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/abstracts/os-memorysys.html)

出处：SOSP 1993，120–133。比较 Ultrix 与 Mach 3.0 + Unix server 的内存系统行为，提醒我们关注指令数量、缓存与内存开销，而不仅是一次 IPC 延迟。

**讨论问题：** 某个微基准快，是否意味着真实工作负载快？论文的具体机器和服务结构，限制了结论能推广到哪些系统？

### 13. Mach: A New Kernel Foundation for UNIX Development — Mike Accetta 等，1986

**扩展 · USENIX 论文 · 难度：中 · 仅链接。** [GNU 的文献介绍](https://www.gnu.org/software/hurd/microkernel/mach.html) · [介绍所指的大学存档](http://www.cs.toronto.edu/~demke/469F.06/Handouts/mach_usenix86.pdf)

早期 Mach 总体设计论文。该存档本次返回 403，未保存本地文件。可先读已下载的 *Unix as an Application Program* 建立 Mach 3.0 概念。早期 Mach 的组成不能直接等同于后来的 GNU Mach + Hurd。

## 四、与 L4/seL4 比较

### 14. On Micro-Kernel Construction — Jochen Liedtke，1995

**比较 · SOSP 论文 · 难度：高 · 仅链接。** [DOI 与出版入口](https://doi.org/10.1145/224056.224075)

出处：SOSP 1995，237–250。经典微内核构造论文，适合用来讨论内核最小化和 IPC 路径设计。本次未取得可靠的本地全文；出版元数据已核对。可先读下面的 L4 回顾论文获取设计背景，再由学校图书馆或出版入口获取全文。

**讨论方向：** 某些机制是否一定要进入内核？减少内核功能时，会不会增加用户态协议和服务之间的复杂度？

### 15. L4 Microkernels: The Lessons from 20 Years of Research and Deployment — Gernot Heiser、Kevin Elphinstone，2016

**比较 · 期刊综述 · 难度：中到高。** [本地 PDF，30 个 PDF 页面](../papers/heiser-elphinstone-2016-l4-lessons.pdf) · [作者原文](https://trustworthy.systems/publications/nicta_full_text/8988.pdf) · [DOI](https://doi.org/10.1145/2893177)

出处：*ACM Transactions on Computer Systems*，34(1)，Article 1，2016。论文署名的正式篇幅为 29 页；作者 PDF 文件包含 30 个页面。

重点读 minimality、generality、IPC、资源管理与不同 L4 实现的演化。它与 2013 年 *From L3 to seL4: What Have We Learnt in 20 Years of L4 Microkernels?*（[DOI](https://doi.org/10.1145/2517349.2522720)）相关，但本地保存的是 **2016 年期刊版**。

**讨论问题：** 哪些设计选择是微内核路线共有的问题，哪些只属于某一代 Mach 或 L4？性能与权限模型为什么要一起讨论？

### 16. seL4: Formal Verification of an OS Kernel — Gerwin Klein 等，2009

**比较 · SOSP 论文 · 难度：高。** [本地 PDF，14 页](../papers/sel4-2009-formal-verification.pdf) · [seL4 项目原文](https://sel4.systems/Research/pdfs/sel4-formal-verification-os-kernel.pdf) · [DOI](https://doi.org/10.1145/1629575.1629596)

出处：SOSP 2009，207–220。先看摘要、编程模型和 What We Proved 一类关于证明范围的说明，再深入方法。

**讨论问题：** 内核 C 实现满足抽象规范，为什么仍不意味着所有用户态服务和整个操作系统都已验证？编译器、汇编、硬件等假设处于什么位置？

这是比较材料；Hurd 并非运行于 seL4 的系统。

### 17. Comprehensive Formal Verification of an OS Microkernel — Gerwin Klein 等，2014

**比较 · TOCS 论文 · 难度：高。** [本地 PDF，70 页](../papers/sel4-2014-comprehensive-verification.pdf) · [seL4 项目原文](https://sel4.systems/Research/pdfs/comprehensive-formal-verification-os-microkernel.pdf)

这是 2009 年工作之后的完整证明栈综述，适合在读完短版论文后使用。重点看抽象规范、可执行规范、C 实现、编译和证明假设之间的关系；它不能被解释为用户态操作系统整体证明。

### 18. seL4 Enforces Integrity — Thomas Sewell 等，2011

**比较 · ITP 论文 · 难度：高。** [本地 PDF，16 页](../papers/sel4-2011-integrity.pdf) · [官方原文](https://sel4.systems/Research/pdfs/sel4-enforces-integrity.pdf)

将功能正确性证明延伸到完整性（integrity）性质。阅读时区分“低权限主体不能影响高权限主体”与功能 API 正确性，并记录证明依赖的系统模型。

### 19. seL4: From General Purpose to a Proof of Information Flow Enforcement — Toby Murray 等，2013

**比较 · IEEE S&P 论文 · 难度：高。** [本地 PDF，15 页](../papers/sel4-2013-information-flow.pdf) · [官方原文](https://sel4.systems/Research/pdfs/sel4-from-general-purpose-to-proof-information-flow-enforcement.pdf)

讨论机密性和信息流非干扰性质，适合与 Hurd 的 auth、Mach port rights 和命名空间问题对照。证明结论依赖抽象系统和配置假设，不能直接推导出整个 Hurd 或任意 seL4 用户态系统的机密性。

### 20. Translation Validation for a Verified OS Kernel — Thomas Sewell 等，2013

**比较 · PLDI 论文 · 难度：高。** [本地 PDF，11 页](../papers/sel4-2013-translation-validation.pdf) · [官方原文](https://sel4.systems/Research/pdfs/translation-validation-verified-os-kernel.pdf)

研究如何把编译后二进制纳入验证链。它很适合回答“C 代码通过证明后，编译器生成的机器码怎么办”，也提醒读者硬件和启动环境仍是信任边界的一部分。

### 21. Scheduling-Context Capabilities — Anna Lyons 等，2018

**比较 · EuroSys 论文 · 难度：高。** [本地 PDF，16 页](../papers/sel4-2018-scheduling-contexts.pdf) · [官方原文](https://sel4.systems/Research/pdfs/scheduling-context-capabilities.pdf)

介绍 MCS 中用 capability 管理时间预算和调度上下文的机制。应与 IPC、阻塞和优先级反转一起阅读，而不是把它简化为“用户态调度器”。

### 22. Time Protection: The Missing OS Abstraction — Qian Ge 等，2019

**比较 · EuroSys 论文 · 难度：高。** [本地 PDF，17 页](../papers/sel4-2019-time-protection.pdf) · [官方原文](https://sel4.systems/Research/pdfs/time-protection-missing-os-abstraction.pdf)

从缓存和微架构造成的时间侧信道出发，说明调度时间预算并不自动等于时间隔离。适合补充现代微内核共同关注的性能、安全和可预测性问题。

### 23. Complete, High-Assurance Determination of Loop Bounds and Infeasible Paths for WCET Analysis — Thomas Sewell 等，2016

**比较 · RTAS 论文 · 难度：高。** [本地 PDF，11 页](../papers/sel4-2016-wcet.pdf) · [官方原文](https://sel4.systems/Research/pdfs/complete-high-assurance-determination.pdf)

关注实时系统所需的可执行代码 WCET 分析输入。它不是 seL4 API 教程，而是理解“可证明实时性还需要哪些工具链”的补充材料。

### 24. Formally Verified Software in the Real World — Gerwin Klein 等，2018

**比较 · CACM 论文 · 难度：中到高。** [本地 PDF，9 页](../papers/sel4-2018-real-world.pdf) · [官方原文](https://sel4.systems/Research/pdfs/formally-verified-software-real-world.pdf)

以工程和部署视角总结形式化验证的收益、成本和边界。建议在证明论文之后阅读，避免把数学性质、工程认证和整机安全目标混为一谈。

## 五、seL4 官方实践资料

| 资料 | 类型与建议 |
| --- | --- |
| [seL4 Tutorials](../sources/sel4/tutorials-index.html) | 官方教程入口；按 Hello World、Capabilities、Untyped、Mapping、Threads、IPC、Notifications、Interrupts、Fault handling、MCS 逐步实践 |
| [seL4 Reference Manual](../references/sel4-manual.pdf) | 当前下载的 Version 16.0.0，190 页；用于查 API、对象和启动协议，教程仍是第一阅读入口 |
| [seL4 kernel overview](../sources/sel4/kernel-overview.html) | 官方内核项目说明，帮助区分内核、用户态系统和构造工具 |
| [Verified configurations](../sources/sel4/verified-configurations.html) | 说明不同架构、配置和 fastpath 的验证覆盖，不要用笼统的“seL4 已验证”代替配置检查 |
| [Microkit](../sources/sel4/microkit.html) | 建立在 seL4 之上的静态系统框架，适合从 API 学习转向组件和 protection domain |
| [capDL](../sources/sel4/capdl.html) | 描述和加载 capability 分配，适合研究启动时资源和对象拓扑 |
| [CAmkES](../sources/sel4/camkes.html) | 组件化系统构造框架，适合对照 Hurd 的多服务器分解 |

## 六、接口手册、实践资料和近期状态

| 资料 | 类型、版本和建议 |
| --- | --- |
| [GNU Mach Reference Manual](../references/gnu-mach-reference-manual.pdf) | 官方手册；Edition 0.4，2008-11-13，GNU Mach 1.3.99，144 个 PDF 页面。重点查 IPC、task/thread、virtual memory；不要以旧安装章节判断当前架构支持。 |
| [Mach 3 Kernel Principles](../papers/mach-kernel-principles-1992.ps) | OSF/CMU 手册；Keith Loepere，1992-07-15。适合补充 Mach 3 的系统概念，不是 GNU Mach 当前版本规范。 |
| [Hurd Hacking Guide 全文](../references/hurd-hacking-guide.txt) | Wolfgang Jährling；Version 0.2_1，2002-03-25。含 translator 编写教程；旧示例对照当前 API。 |
| [The Authentication Server](../sources/gnu-hurd/hurd/documentation/auth.mdwn) | Wolfgang Jährling，2002 年演讲文本。重点是四组 IDs、客户端/服务器 reauthentication、password 与 fakeauth。 |
| [RPC](../sources/gnu-hurd/hurd/rpc.mdwn) 与 [I/O Path](../sources/gnu-hurd/hurd/io_path.mdwn) | 技术说明；追踪 `open`、`lseek`、`read` 经 glibc、MIG、服务器的路径。路径和缓冲区细节需要核对当前源码。 |
| [Mach IPC without MIG](../sources/mach-ipc-without-mig.txt) 与 [ipc-hello.c](../sources/ipc-hello.c) | Neal Walfield 的练习及代码原文。适合观察消息结构和端口权利；保存供阅读，未在本机编译运行。 |
| [Manually Bootstrapping a Translator](../sources/manual-bootstrap.txt) | Neal Walfield；学习 translator 如何与父文件系统建立联系。 |
| [GNU Hurd Reference Manual 入口](../sources/gnu-hurd/hurd/reference_manual.mdwn) | 官方说明其尚不完整；本地保存的是入口介绍，未把它冒充完整手册。 |
| [FOSDEM 2026 进展报告](../talks/thibault-2026-hurd-progress.pdf) | Samuel Thibault，2026-02-01，演讲幻灯片，29 个 PDF 页面。主题包括 rump、64 位、SMP、软件构建与语言支持。 |
| [FOSDEM 演讲与视频入口](https://fosdem.org/2026/schedule/event/7FZXHF-updates_on_gnuhurd_progress_rump_drivers_64bit_smp_software_bootstrapping/) | 可观看约 30 分钟的讲解。下载幻灯片采用活动页的当前附件地址；GNU Wiki 内旧附件号已失效。 |
| [Debian GNU/Hurd](../sources/debian/hurd.html)、[安装说明](../sources/debian/hurd-install.html)、[镜像入口](../sources/debian/hurd-cd.html) | 实践入口。旧 QEMU 页面混有历史命令，安装时核对当前发行版说明。 |
| [已有入门讲义](../talks/shakthi-kannan-gnu-mach-hurd-workshop.pdf) | Shakthi Kannan 的 GNU Hurd Workshop，20 页，PDF 元数据生成于 2006-10-08。提供快速概览，不属于研究论文；元数据日期不作为活动日期的证据。 |

## 检索入口

- [GNU Hurd 官方文档](https://www.gnu.org/software/hurd/hurd/documentation.html) 与 [本地快照](../sources/gnu-hurd/hurd/documentation.mdwn)。
- [CMU Mach 论文档案](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/publications.html) 与 [本地索引](../sources/mach-publications.html)。
- [seL4 论文入口](https://sel4.systems/Research/publications.html) 与 [本地索引](../sources/sel4-publications.html)。
- [GNU 官方源码入口](https://cgit.git.savannah.gnu.org/cgit/hurd/)：`hurd/hurd.git`、`hurd/gnumach.git`、`hurd/mig.git`。

进一步检索可以使用：`GNU Hurd translator naming capability`、`Mach external pager memory object`、`Hurd authentication reauthentication`、`Hurd L4 critique resource accounting`。Hurd NG/L4/Viengoos 相关内容应作为探索分支和研究历史阅读，与当前 GNU Mach 上的 Hurd 区分。
