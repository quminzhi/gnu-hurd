# Hurd 中的 Translator 与命名空间

## 1. Translator 是什么

在 GNU Hurd 中，translator 是一个运行在用户态的 server。它通常附着在文件系统中的某个 node 上，并实现 Hurd 的 `fs`、`io` 等接口。它接收对该 node 的操作请求，再把请求转换成对后端资源或其他服务的操作。

例如：

- `ext2fs` 把磁盘上的 ext2 数据结构转换成目录和文件；
- `ftpfs` 把 FTP 服务器转换成可以通过路径访问的文件系统；
- `hello`、`random` 等 translator 可以动态生成文件内容；
- 设备文件、符号链接、网络文件系统也可以用 translator 实现。

“translator”这个名称强调的是转换过程：它可以把一种对象表示转换成另一种表示，也可以把文件操作转换成网络请求、设备访问或动态计算。

从 Mach 内核的角度看，translator 只是一个普通用户进程。它通过 Mach IPC 接收消息，并通过 RPC 提供服务。在 Hurd 语境中，通常把“附着到文件系统节点的 server”称作 translator；如果强调 RPC 服务本身，也可以直接称为 server。

## 2. Translator 如何参与文件访问

以 `open` 和 `read` 为例：

```text
open("/mnt/a.txt")
  -> glibc 进行路径解析
  -> 对文件系统 server 发起 dir_lookup RPC
  -> 获得表示文件的 Mach port
  -> glibc 用这个 port 建立文件描述符

read(fd, buf, n)
  -> glibc 对该 port 发起 io_read RPC
  -> translator 处理请求并返回数据
```

在 Hurd 中，文件描述符背后通常对应一个 Mach port。Mach port 是访问某个对象或服务端点的能力；`io_read`、`io_write`、`io_stat` 等 RPC 则规定了可以对这个端点做什么。

Translator 可以附着在普通文件或目录上，因此一个用户态程序可以像文件系统一样接入路径解析。用户程序无需知道后端是磁盘、FTP、ISO 镜像、设备还是动态生成器。

## 3. 为什么 Hurd 需要 Translator

Translator 不是 Mach IPC 的硬性要求；它是 Hurd 把用户态服务接入 POSIX 文件系统接口的主要机制。它有几个作用：

1. **保持微内核小而稳定**：文件系统、网络、认证等策略放在用户态，新增服务通常不需要修改 Mach 内核。
2. **保持 POSIX 兼容性**：服务可以表现成普通文件或目录，因此现有程序可以继续使用 `open`、`read`、`write`、`stat`、`ls` 等接口。
3. **支持服务的组合和替换**：一个 translator 可以把请求转交给另一个 translator。例如，FTP translator 提供远程文件，ISO translator 再解析其中的 ISO9660 内容。
4. **允许普通用户扩展系统**：用户通常只需拥有目标 inode 和后端资源的访问权，就能在自己的目录中安装 translator，不必获得修改内核的权限。
5. **支持按需启动**：passive translator 可以把启动命令保存在 inode 中，第一次访问时才启动对应的 active translator；配置还可以跨重启保留。

因此，translator 的关键价值是：

> 把用户态服务变成文件系统命名空间中的可组合对象，并让它获得路径解析、权限检查和 POSIX 访问方式。

## 4. 不使用 Translator 的其他服务访问方式

用户态程序访问服务不一定要通过 translator。常见方式包括：

| 方式 | 访问形式 | 适合场景 |
| --- | --- | --- |
| 直接 Mach IPC / MIG RPC | 客户端拿到 server 的 Mach port，调用自定义 RPC | 明确的控制接口和系统服务 |
| Unix socket、TCP、管道、FIFO | 通过字节流协议通信 | 网络服务和流式数据 |
| 共享内存加同步机制 | 共享数据，IPC 负责控制 | 大量数据传输 |
| 动态库或嵌入式组件 | 服务代码直接链接进客户端 | 不需要独立进程和隔离的场景 |
| 内核 syscall 或设备接口 | 由内核直接实现服务 | 必须在内核中执行的操作 |

例如，客户端可以先通过 `/servers/password` 找到认证服务的 port，再直接调用认证 RPC：

```c
mach_port_t p = hurd_file_name_lookup("/servers/password");
password_check_user(p, ...);
```

这里使用的是服务端口和自定义 RPC，而不是把认证服务模拟成普通文件。

需要注意：不使用 translator 并不意味着不需要命名和权限。客户端仍然要知道如何找到服务端点，并且需要获得相应的 Mach port right。Translator 只是提供了一种特别方便的方式：把服务挂入文件系统路径。

## 5. 什么是命名空间

命名空间可以理解为：

> 一组名字，以及把名字解析成对象的规则、范围和权限。

因此，命名空间不只是名字列表，也不只是目录结构。它通常包括：

- 名字，例如 `/mnt/a.txt` 或 `/servers/password`；
- 解析规则，例如从哪个根开始、如何逐级查找；
- 解析结果，例如文件、目录、服务端点或 Mach port；
- 访问范围和权限，例如谁能看到、谁能访问；
- 别名和生命周期，例如符号链接、挂载、服务重启和按需启动。

## 6. 文件系统命名空间

普通 Unix 中，文件系统命名空间通常表现为一棵目录树：

```text
/
├── home/
├── dev/
├── etc/
└── mnt/
```

路径名被解析成文件、目录或设备等对象。

在 Hurd 中，路径解析最终通常得到一个 Mach port。这个 port 代表一个对象，或者代表继续解析路径的 server。例如：

```text
/mnt
  -> ext2fs translator 的 port

/mnt/a.txt
  -> 先把 /mnt 解析到 ext2fs
  -> 再由 ext2fs 解析 a.txt
  -> 得到表示该文件的 port
```

所以 Hurd 的文件系统命名空间不是只由内核中的单一 VFS 实现的目录树，而更像一棵由多个用户态 server 共同维护的树。路径解析过程中，控制权可以从一个 server 转移到另一个 server。

## 7. 服务命名空间

服务命名空间是把名字映射到服务端点的命名空间。例如：

```text
/servers/password  -> 认证服务的 Mach port
/servers/foo       -> foo 服务的 Mach port
```

客户端先通过名字找到 port，再使用该服务定义的 RPC。这里的路径名类似电话簿中的服务名称，Mach port 提供访问能力，RPC 接口决定可以执行哪些操作。

在原始 Mach 中，服务发现可以由独立的 nameserver 完成。Hurd 通常复用文件系统作为服务命名空间：通过 `hurd_file_name_lookup` 做路径解析，从文件系统树中取得服务 port。因此在 Hurd 中，服务命名空间往往是文件系统命名空间的一种用途，而不是完全独立的注册表。

## 8. 两种命名空间的关系

| 概念 | 名字指向 | 典型操作 |
| --- | --- | --- |
| 文件系统命名空间 | 文件、目录、设备、translator 节点 | `open`、`read`、`write`、`stat` |
| 服务命名空间 | server 或 Mach port | 自定义 RPC |
| Hurd 的统一做法 | 路径先解析为 port，再按 port 对应的接口通信 | POSIX RPC 或自定义 RPC |

可以把整个过程拆成三个层次：

1. **名字**：例如 `/mnt` 或 `/servers/password`；
2. **对象或端点**：例如某个文件对象对应的 port，或认证 server 的 port；
3. **协议**：例如 `io_read` 或 `password_check_user`。

命名空间负责“通过名字找到谁”；Mach port 表示“获得了怎样的访问能力”；RPC 接口决定“找到之后能做什么”。

因此，“服务命名空间”和“文件系统命名空间”的差别主要在于名字所代表的对象和后续协议不同。在 Hurd 中，两者被有意放进同一个路径体系，使得用户态服务既可以通过直接 RPC 使用，也可以通过 translator 伪装成文件系统对象使用。

## 9. 相关资料

- [Translator 定义](../docs/sources/gnu-hurd/hurd/translator.mdwn)
- [Translator 机制与 active/passive translator](../docs/sources/gnu-hurd/hurd/documentation/translators.mdwn)
- [Hurd RPC 调用流程](../docs/sources/gnu-hurd/hurd/rpc.mdwn)
- [Hurd 中的服务发现和路径解析](../docs/sources/gnu-hurd/hurd-talk.html)
