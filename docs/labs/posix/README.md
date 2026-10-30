# 可运行代码实验

这些实验用 macOS 或 Linux 自带的 POSIX API，把 Hurd/Mach/seL4 中的几个机制做成最小模型。它们帮助你先理解控制流和权限边界，**不等于真正的 Mach IPC 或 seL4 内核实现**。

## 运行

在本目录执行（源码留在当前目录，程序和调试文件生成在 `build/` 中）：

```sh
make
make run
```

也可以单独运行：

```sh
./build/01_rpc_socket
./build/02_capability_table
./build/03_notification_shared_memory
```

结束后清理二进制：

```sh
make clean
```

## 实验对应关系

| 程序 | 你要观察的东西 | 对应资料 |
| --- | --- | --- |
| `01_rpc_socket.c` | client/server、请求/回复、进程边界、阻塞读写 | [seL4 IPC 教程](../../sources/sel4/tutorial-ipc.html)、[Mach IPC 论文](../../papers/mach-ipc.ps) |
| `02_capability_table.c` | 本地 slot、对象引用、权限、复制和撤销的直觉 | [seL4 Capabilities](../../sources/sel4/tutorial-capabilities.html)、[seL4 手册](../../references/sel4-manual.pdf) |
| `03_notification_shared_memory.c` | notification 只表达事件；数据需要共享内存或其他协议 | [seL4 Notifications](../../sources/sel4/tutorial-notifications.html)、[Microkit](../../sources/sel4/microkit.html) |

## 每个程序怎么读

先不要只看输出。对每个实验回答：

1. 哪个对象跨越了保护边界？
2. 谁拥有访问权？
3. 哪一步会阻塞？
4. 数据在哪里保存？
5. 如果服务崩溃或拒绝请求，另一方会看到什么？
6. 这个 POSIX 机制与真实 Mach/seL4 哪些地方不能直接类比？

## 从模拟到真实系统

### `01_rpc_socket.c` → seL4 IPC

先看 `socketpair`、`fork`、`write_all` 和 `read_line`，画出请求/回复时序；然后进入 seL4 Tutorial 的 `IPC`，把 Unix socket 替换成 endpoint capability，把请求缓冲区替换成 message registers/IPC buffer。seL4 的 endpoint 通常是同步会合，不是这个示例的字节流 socket。

### `01_rpc_socket.c` → Hurd/Mach

把 `request` 想成一个由 MIG 定义的 RPC 请求，把 `channel` 想成 Mach port 路径。随后阅读 `docs/sources/mach-ipc-without-mig.txt` 和 [Hurd RPC 文档](../../sources/gnu-hurd/hurd/rpc.mdwn)，标出消息头、port right、reply port 与这个简单 socket 的区别。

### `02_capability_table.c` → seL4 CSpace

先运行它，再阅读 CSpace/CNode、capability transfer、Delete/Revoke。示例里的 `struct capability` 是用户态模型；真实 capability 由内核保护，不能由程序直接修改结构体来改变权利。

### `03_notification_shared_memory.c` → seL4 notification

观察两个 bit 被一次唤醒合并。然后阅读 notification 教程，理解 badge 和 notification word。若每个事件都必须被处理，用户态需要在共享内存中维护计数器或队列；notification 本身不是可靠消息队列。

## 第二轮练习

- 修改 RPC 服务，让它拒绝不是 `read` 开头的请求。
- 给 capability 表增加 `CAP_EXEC`，说明“能读”不等于“能执行”。
- 把共享数据从一个整数改成环形队列，并记录生产者/消费者索引。
- 为每个实验画一张“内核机制 / 用户态策略 / 依赖假设”表。
- 完成模拟实验后，再运行官方 seL4 Tutorial；不要把模拟实验的输出当成 seL4 的性能数据。
