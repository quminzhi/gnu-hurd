# 实验导航

当前实验位于 [posix/](posix/README.md)，用 C 和 POSIX API 观察跨进程请求/回复、对象授权及事件同步。它们与真实 seL4 API 的对应和差别均写在实验说明中。

| 顺序 | 源码 | 观察重点 |
| --- | --- | --- |
| 1 | [01_rpc_socket.c](posix/01_rpc_socket.c) | 请求、回复、独立进程和阻塞 |
| 2 | [02_capability_table.c](posix/02_capability_table.c) | 本地槽位、对象引用、权限和派生授权 |
| 3 | [03_notification_shared_memory.c](posix/03_notification_shared_memory.c) | 事件位与共享数据、通知合并 |

在项目根目录执行：

```sh
make run
```

或：

```sh
make -C docs/labs/posix run
```

生成的程序和调试文件集中在 `posix/build/`，由 `make clean` 清理。学习顺序见 [学习参考路线](../guides/study-plan.md)，真实 seL4 教程见 [官方资料导航](../sources/sel4/README.md)。
