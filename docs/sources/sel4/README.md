# seL4 官方资料本地导航

本目录保存 2026-10-08 从官方站点取得的 HTML 快照。网页中的 CSS、图片和相对链接未全部镜像；需要完整导航时使用每项的在线入口。版本可变的文档仍以在线页面和仓库提交为准。

## 从教程开始

| 顺序 | 本地快照 | 在线入口 |
| --- | --- | --- |
| 1 | [Setting up](tutorial-setting-up.html) | <https://docs.sel4.systems/Tutorials/setting-up.html> |
| 2 | [Getting the tutorials](tutorial-get-the-tutorials.html) | <https://docs.sel4.systems/Tutorials/get-the-tutorials.html> |
| 3 | [Hello World](tutorial-hello-world.html) | <https://docs.sel4.systems/Tutorials/hello-world.html> |
| 4 | [Capabilities](tutorial-capabilities.html) | <https://docs.sel4.systems/Tutorials/capabilities.html> |
| 5 | [Untyped](tutorial-untyped.html) | <https://docs.sel4.systems/Tutorials/untyped.html> |
| 6 | [Mapping](tutorial-mapping.html) | <https://docs.sel4.systems/Tutorials/mapping.html> |
| 7 | [Threads](tutorial-threads.html) | <https://docs.sel4.systems/Tutorials/threads.html> |
| 8 | [IPC](tutorial-ipc.html) | <https://docs.sel4.systems/Tutorials/ipc.html> |
| 9 | [Notifications](tutorial-notifications.html) | <https://docs.sel4.systems/Tutorials/notifications.html> |
| 10 | [Interrupts](tutorial-interrupts.html) | <https://docs.sel4.systems/Tutorials/interrupts.html> |
| 11 | [Fault handlers](tutorial-fault-handlers.html) | <https://docs.sel4.systems/Tutorials/fault-handlers.html> |
| 12 | [MCS](tutorial-mcs.html) | <https://docs.sel4.systems/Tutorials/mcs.html> |

教程页面的官方顺序是推荐顺序。MCS 教程明确说明相关新特性仍在进行验证，适合放在基础 IPC、线程和通知之后。

## 内核、构造工具与验证

| 本地快照 | 在线入口 | 用途 |
| --- | --- | --- |
| [kernel overview](kernel-overview.html) | <https://docs.sel4.systems/projects/sel4/> | 区分 seL4 内核 API、用户态系统和框架 |
| [verified configurations](verified-configurations.html) | <https://docs.sel4.systems/projects/sel4/verified-configurations.html> | 查不同架构、平台和配置的证明属性 |
| [Microkit](microkit.html) | <https://docs.sel4.systems/projects/microkit/> | 静态结构化系统框架 |
| [Microkit setting up](microkit-setting-up.html) | <https://docs.sel4.systems/projects/microkit/setting-up.html> | SDK 与 macOS/Linux 环境说明 |
| [Microkit tutorial](microkit-tutorial.html) | <https://docs.sel4.systems/projects/microkit/tutorial/welcome.html> | 从组件系统入门 |
| [Microkit manual](microkit-manual.html) | <https://docs.sel4.systems/projects/microkit/manual/latest/> | SDK 参考 |
| [capDL](capdl.html) | <https://docs.sel4.systems/projects/capdl/> | capability 描述语言和工具 |
| [capDL loader](capdl-loader.html) | <https://docs.sel4.systems/projects/capdl/c-loader-app.html> | 启动时对象和 capability 初始化 |
| [CAmkES](camkes.html) | <https://docs.sel4.systems/projects/camkes/> | 组件化系统构造框架 |

## 论文和手册

- 论文原文在 [docs/papers](../../papers/)；中文注释在 [bibliography.md](../../guides/bibliography.md)。
- [seL4 Reference Manual 16.0.0](../../references/sel4-manual.pdf) 是本轮下载的版本；在线 `latest` 地址可能随时间变化。
- [官方出版物索引](../sel4-publications.html) 保留了更多论文入口。本地索引条目不等于本地已经保存全文。
- [来源清单](../../SOURCES.md) 和 [manifest](../../sources-manifest.json) 记录下载来源、大小和 SHA-256。

## 版本和运行提示

- 教程以 `repo` manifest 取得多仓库源码；请将实际 manifest revision 写入学习记录。
- 官方 Setting up 页面以 Linux/Docker 教程为主；Microkit SDK 页面列出 Linux 和 macOS、Intel 和 Arm 的 SDK。SDK、教程代码和内核配置需要成套使用。
- HTML 快照用于离线阅读正文；页面中的远程图片、CSS、GitHub 链接和相对导航可能仍需要网络。
