# 学习路线、实验与讨论问题

[导读目录](README.md) · [资料库索引](../README.md) · [代码实验](../labs/README.md)

建议以问题驱动阅读，每轮掌握一条具体机制，再阅读论文中的取舍。下面的时间是参考，可按自己的课程安排调整。

## 第 1 阶段：系统结构与设计目标（2–3 小时）

阅读 [README](../README.md)、[Bushnell 原始设计](../sources/gnu-hurd/hurd-paper.html)、[系统启动概览](../sources/gnu-hurd/overview.mdwn) 和 [Marcus 的演讲](../sources/gnu-hurd/hurd-talk.html)。

先画出应用、glibc、Hurd 服务、GNU Mach 四层，并说明各自负责什么。重点识别：

| 部件 | 应建立的认识 |
| --- | --- |
| GNU Mach | 调度与线程、地址空间与虚拟内存、IPC，以及具体版本所提供的底层硬件机制 |
| Hurd | 在用户态实现文件系统、认证、进程元数据、程序加载等系统服务 |
| glibc | 将许多 POSIX 操作映射到 Hurd RPC，并维护部分进程状态 |
| MIG | 根据 `.defs` 生成客户端/服务器的消息编解码和分派代码 |
| translator | 可接入文件系统命名空间、实现相应 Hurd 接口的用户态服务程序 |

讨论：单个 Unix server 运行于 Mach 与 Hurd 的多服务器设计，分别在哪些地方划分边界？为什么普通应用可以继续用 `open/read/write`，而无需专门认识每一种 translator？

**阶段产出：** 一张结构图，及一段解释“GNU 为何采用这样的服务结构”的文字。

## 第 2 阶段：port、权利与 RPC（3–5 小时）

阅读 [Mach port](../sources/gnu-hurd/microkernel/mach/port.mdwn)、[GNU Mach 手册](../references/gnu-mach-reference-manual.pdf) 的 IPC 章节、[RPC 教程](../sources/gnu-hurd/hurd/rpc.mdwn)，然后读 [Unix as an Application Program](../papers/mach-mach3_intro.ps)。

要区分三件事：port 对象、内核维护的 port right、某个 task 私有命名空间中的整数 port name。两个进程内相同的整数不必代表同一对象；传递权利也不只是把那个整数复制给另一个进程。

思考一次请求的组成：目标 send right、消息 ID、参数、传入的权利、reply port，以及接收/等待回复的行为。Mach 消息传递机制可用于异步通信；常见 RPC 模式在此之上等待回复。

**阶段产出：** 给 `open("test.txt")` 和 `lseek(fd, 10, SEEK_CUR)` 各画一条请求与回复路径，并解释客户端最终保存什么状态。

## 第 3 阶段：translator 与用户可扩展性（3–5 小时）

阅读 [Translator Primer](../sources/gnu-hurd/hurd/documentation/translator_primer.mdwn)、[Translators](../sources/gnu-hurd/hurd/documentation/translators.mdwn)、[Hacking Guide](../references/hurd-hacking-guide.txt) 和 [libtrivfs](../sources/gnu-hurd/hurd/libtrivfs.mdwn)。

下面是学习时可操作的实验，**尚未代你运行**。这些命令在 GNU/Hurd 内运行；本机 macOS 或 Linux 通常没有 `/hurd/hello` 和 `settrans`。

### 实验 A：让普通文件由用户态服务提供内容

在 Hurd 中，以普通用户创建一个独立的练习目录：

```sh
mkdir hurd-study
cd hurd-study
touch hello
cat hello
settrans hello /hurd/hello
showtrans hello
cat hello
fsysopts hello
```

预期观察：初始 `cat` 没有内容；设置 translator 后，`cat` 读取 `/hurd/hello` 提供的文本。`showtrans` 用于观察 passive 启动描述，`fsysopts` 与运行中的服务通信。以实际输出为准，可能随版本改变。

可以按照 Primer 修改内容：

```sh
fsysopts hello --contents='Hello GNU!'
cat hello
settrans -g hello
cat hello
```

最后一步清除该练习节点的 translator 后，底层文件仍为空。实验围绕用户自己拥有的节点进行。

回答：`cat` 为什么不需要知道 `/hurd/hello` 的名字？返回的字节是否一定存储在底层文件里？绑定 translator 是否让它获得额外的硬件访问权限？

### 实验 B：观察 active 与 passive 的区别

先查看本机的 `settrans --help`、`showtrans --help`，再对练习节点分别使用普通 `settrans` 和 `settrans -a`。观察访问前后是否出现服务进程，以及停止服务后再次访问时是否按保存的描述启动。

解释时要把“保存启动命令”“启动进程”“取得服务引用”分开。passive 描述跨重启保留需要底层文件系统持久存储；它不是把进程的全部状态持久化。

**阶段产出：** 一页实验记录，注明 Hurd 版本、命令、实际输出、自己的解释。

## 第 4 阶段：跟踪 `read()` 与阅读小服务（4–8 小时）

阅读 [I/O Path](../sources/gnu-hurd/hurd/io_path.mdwn)、[MIG 文档](../papers/mach-mig.ps)、[Mach IPC without MIG](../sources/mach-ipc-without-mig.txt)。如果暂时没有 Hurd 环境，也可以先读文档与源码。

以 `/hurd/hello` 为例，文档描述的概念链路是：

```text
应用 read(fd, buffer, count)
  -> glibc __libc_read / _hurd_fd_read
  -> MIG 客户端 stub：__io_read
  -> Mach IPC
  -> 服务端 demux / RPC stub
  -> hello translator 的 trivfs_S_io_read
  -> 回复，经 IPC 返回 glibc 和应用
```

生成代码的名字与具体目录可能随版本改变。不要把一次本地 C 函数调用与跨进程 RPC 混为一谈。

源码阅读入口：

| 想追踪的内容 | 源码位置或入口 |
| --- | --- |
| `io_read`、`io_seek` 的协议 | [Hurd io.defs](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/hurd/io.defs) |
| `dir_lookup` 与文件元数据协议 | [Hurd fs.defs](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/hurd/fs.defs) |
| 最小文本服务的实现 | [trans/hello.c](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/trans/hello.c) |
| trivfs 的接口适配 | [libtrivfs/](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/libtrivfs) |
| port 对象及服务循环 | [libports/](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/libports) |
| auth 协议与实现 | [auth.defs](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/hurd/auth.defs)、[auth/](https://cgit.git.savannah.gnu.org/cgit/hurd/hurd.git/tree/auth) |
| glibc 的 Hurd 适配 | GNU glibc 源码中的 `sysdeps/mach/hurd/` 与 `hurd/`，对照 I/O Path 文档 |

追踪 `read` 时记录：文件偏移在哪维护；参数如何校验；内存由谁分配、释放；服务返回 inline/out-of-line 数据后，是否仍需复制到调用者给出的缓冲区。

**阶段产出：** 一张函数与协议调用图。重点解释每次边界穿越的目的，不必逐行阅读整个 glibc。

## 第 5 阶段：权限、信任与架构批评（4–6 小时）

先读 [Authentication Server](../sources/gnu-hurd/hurd/documentation/auth.mdwn)，再读 [Critique](../papers/walfield-brinkmann-2007-hurd-critique.pdf) 与 [position paper](../papers/walfield-brinkmann-2007-access-decomposition.pdf)。

Hurd 同时使用 Mach 提供的能力式对象引用和 Unix 风格的身份/权限语义。auth 对象、服务端授权策略以及“是否拥有某对象的引用”不能直接当成同一个概念。

挑一个具体场景，例如“图片查看器读取用户选择的一张图片”。列出它实际需要的资源，再对照程序默认获得的权限。继续考虑 translator 代理、资源消耗归责、撤销权限和命名空间变化。

**阶段产出：** 一个分析表：设计目标、提供的机制、仍然存在的问题、可能的改进。对论文中的批评注明年代，不直接认定当前实现没有修复。

## 第 6 阶段：VM、性能与其他微内核路线（按兴趣选读）

- 虚拟内存方向：[Duality of Memory and Communication](../papers/mach-dual-mem-com.ps) + [Machine-Independent VM](../papers/mach-MIvmm.ps)。
- 性能方向：[OS Structure and Memory Performance](../papers/mach-os-memorysys.ps) + [L4 20 年回顾](../papers/heiser-elphinstone-2016-l4-lessons.pdf)。
- 正确性与安全方向：[seL4 验证论文](../papers/sel4-2009-formal-verification.pdf)。
- 当前 Hurd 方向：[2026 进展演讲](../talks/thibault-2026-hurd-progress.pdf) + 当前源码与发行版资料。

比较时分别记录内核 API、服务划分、授权模型、资源管理方式、IPC 实现与评估工作负载。Mach 的一次实验结果，不能直接代表所有微内核；内核经过形式化验证，也不能直接代表整个用户态系统。

## 实践环境入口

推荐先使用发行版提供的 QEMU 镜像或安装镜像：[Debian GNU/Hurd](https://www.debian.org/ports/hurd/)、[GNU QEMU 页面](https://www.gnu.org/software/hurd/hurd/running/qemu.html)。本资料库只收集文献，没有安装或启动虚拟机。

旧教程中的 `-enable-kvm` 属于 Linux/KVM 环境。macOS 使用不同的加速方式；在 Apple Silicon 上运行 x86 Hurd 属于跨架构模拟，不能直接套用 x86/KVM 的性能预期。先确认宿主架构、镜像架构和当前驱动/内存要求，再安排实验。

本地 `.ps` 是论文作者/大学档案提供的 PostScript 原文。需要 PDF 时可使用已安装的 Ghostscript，例如在自己的临时目录运行 `ps2pdf mach-mach3_intro.ps mach-mach3_intro.pdf`。资料库保留原始文件，没有重新排版论文。

## 术语速查

| 术语 | 在本轮学习中的含义 |
| --- | --- |
| microkernel | 以少量内核机制支撑用户态系统服务；具体边界取决于实现 |
| multi-server | 系统服务由多个相对独立的服务器实现 |
| task / thread | Mach 的资源容器与执行单元；task 不等于完整 POSIX process 语义 |
| port / port right / port name | 内核通信对象、对象权利、某个 task 内的整数名称 |
| capability | 同时指定对象并提供某种授权的受保护引用；本处主要研究 port rights |
| IPC / RPC | 跨进程通信机制 / 建立在通信之上的请求与回复调用模式 |
| MIG / `.defs` | Mach 接口生成器 / RPC 接口定义文件 |
| translator | 将某类对象与访问语义接入 Hurd 命名空间的用户态服务 |
| active / passive | 当前运行并连接的服务 / 保存的启动描述 |
| namespace | 名字、起点对象与解析关系构成的对象访问结构 |
| auth / password | 身份对象和认证协作 / 凭密码获取相应身份授权的服务 |
| pager / memory object | 与内核协作供页的服务 / 可被映射并通过协议管理的内存对象 |
| libtrivfs / libnetfs / libdiskfs | 面向简单文件服务、虚拟/网络文件系统、磁盘文件系统的 Hurd 辅助库 |
| mechanism / policy | 提供何种基本操作 / 如何选择、限制与组合这些操作 |
| reliance set / TCB | 某操作正确性所依赖的组件集合 / 为保证特定安全性质必须信任的组件 |

## 可以作为下一轮讨论的起点

“如果我运行 `cat hello`，文件描述符、port right 和 translator 三者有什么关系？”

讨论时可以带上自己画的调用路径，我们逐步检查 `open`、`read` 和关闭文件时各个组件掌握的状态。随后再追问：为什么这条路径允许用户扩展系统，新的自由又带来了哪些信任和资源管理问题？

记录模板：资料与章节；它试图解决的问题；提供的机制；依赖的假设；仍不理解的地方；可以用实验或源码检验的结论。
