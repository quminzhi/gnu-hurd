# GNU/Hurd（Mach）虚拟内存设计教程

> 面向具有操作系统、计算机组成原理和 C 语言基础的读者。本文的目标不是记住一组 API，而是建立一个可以用来阅读微内核源码的模型：**Mach 内核管理地址空间、页和消息，并决定缺页与回收的时机；Hurd 用户态服务器决定“这些页的内容是什么、从哪里来，以及如何持久化”**。

本文结合本项目中的 GNU Mach Reference Manual、Mach VM 论文、Hurd 文档和调用路径整理而成。GNU Mach Reference Manual 的本地版本对应 GNU Mach 1.3.99（2008 年手册），而 Hurd 当前开发仍在变化；因此，结构和协议关系是可靠的，具体函数签名、默认 pager 和架构相关细节应以正在使用的源码版本为准。

---

## 1. 先看全局：一次文件映射到底经过谁

对一个普通 Hurd 程序而言，`mmap` 看起来像一个 C 库函数，但它跨越了多个对象和协议：

```text
应用程序
  │  mmap(fd, ...)
  ▼
glibc（Hurd 端）
  │  对文件 port 发 io_map RPC
  ▼
文件 server / translator（例如 ext2fs）
  │  返回代表文件内容的 Mach memory-object port（概念上；具体接口可能区分读/写 port）
  ▼
glibc 调用 Mach vm_map
  │  把 memory object 映射进应用 task 的地址空间
  ▼
Mach 内核：task address map + resident page cache
  │
  │  应用第一次访问某个尚未驻留的虚拟页
  ▼
缺页处理
  │  memory_object_data_request RPC
  ▼
Hurd pager（通常由 libpager 帮助实现）
  │  pager_read_page / libstore
  ▼
磁盘、设备、文件、网络、压缩对象或其他后端
  │
  └─ memory_object_data_supply 返回页给 Mach
```

对于可写的共享映射，应用修改映射中的页后，Mach 可能在内存回收、显式同步或 pager 请求清理时，将脏页作为 `memory_object_data_return`（旧接口中也可能叫 `memory_object_data_write`）交给 pager；pager 再按自己的策略写回文件系统或其他 backing store。这个过程不是每次用户态写入都会立即发生；`MAP_PRIVATE` 产生的私有 COW 页面也不会因此写回原文件。

这里有三个必须分开的概念：

1. **虚拟地址**：某个 task 中的地址，例如 `0x400000`。
2. **memory object offset**：该地址对应的对象内偏移，例如文件的第 8192 字节。
3. **物理页**：当前是否存在 RAM 中的页。它是 Mach 的缓存，不是永久数据的唯一副本。

Mach 的设计把第 1 项和第 2 项的映射交给内核，把第 2 项的内容交给 memory manager/pager；第 3 项由内核的 resident page table、页队列和 paging daemon 管理。

---

## 2. Mach 的基本对象：从“进程”换成“任务、线程、端口、内存对象”

### 2.1 Task 是虚拟地址空间的拥有者

Mach 的 `task` 是资源容器。它包含：

- 一个 paged virtual address space；
- task 中运行的线程；
- 该 task 的 Mach port name space；
- 访问其他内核对象的 port rights。

Mach 传统意义上的 Unix process，在这里通常是“一个 task 加上一个或多个 thread，以及由 Hurd server 管理的进程元数据”。`task_t` 同时也是访问该 task 的 port 类型；Mach 把 task 的虚拟地址映射表作为 task 对象的一部分管理。

### 2.2 Thread 是执行上下文

线程共享所属 task 的地址空间。缺页异常发生在线程执行一次加载、存储或取指令时，但决定如何补页的是该地址所在 task 的映射和 memory object。

### 2.3 Port 是能力和 RPC 端点

Mach port 是内核保护的消息队列。持有 send right 才能向对象发送消息；持有 receive right 才能接收消息。port right 不能靠伪造整数生成，因而可以作为 capability。

这对 VM 很关键：

- `task_t` port 允许调用者操作一个 task 的地址空间；
- `memory_object_t` port 标识一个可映射的数据对象；
- `memory_object_control_t` port 允许 pager 要求内核清理、刷新或改变缓存页；
- `memory_object_name_t` 用于询问映射信息，例如 `vm_region` 返回的 object name。

端口是权限边界，地址空间中的页保护位是另一层边界。拿到一个 memory object 的 send right 不等于获得任意读写权，最终访问仍受 `vm_map` 的 current/max protection 和 pager 返回的 lock value 约束。

### 2.4 Memory object 是“可被映射的数据源”

Mach 论文把 memory object 定义为：由某个 server 提供和管理、可以映射到 task 地址空间的数据集合。它很像一个按字节编号的文件，但不要求实际存在文件：

- 磁盘文件可以是 memory object；
- 匿名零填充内存可以是 memory object；
- 网络对象、压缩对象、设备缓冲区也可以实现成 memory object；
- copy-on-write 会产生没有直接 backing store 的 shadow object。

这个抽象是 Mach VM 和 Hurd 文件系统之间的桥梁。

---

## 3. Mach 内核对上暴露了哪些 VM 能力

Mach 的 API 大多通过对 task/host port 发消息实现。下面按“控制地址空间”“共享和传输”“外部分页”分类。

这里的“接口”通常是 Mach MIG 生成的 RPC，而不是传统 Unix 意义上只能从当前进程调用的私有函数。调用者拿到目标 task、host 或 memory object 的 port right 后，就可以向相应端口发送请求；内核负责检查 port right、参数和权限，再修改目标对象。这个模型解释了为什么 Mach 可以把“操作另一个 task 的地址空间”表达成普通的带 capability 的消息调用。

### 3.1 地址空间控制接口

| 接口 | 语义 | 学习重点 |
| --- | --- | --- |
| `vm_allocate` | 在目标 task 中分配一段页对齐的虚拟地址；初始内容为零，物理页通常按需分配 | reservation 与 demand allocation 的区别 |
| `vm_deallocate` | 删除目标 task 对某段地址的映射 | 只影响指定 task，不自动销毁其他 task 的引用 |
| `vm_map` | 把一个 memory object 的某段 offset 映射到 task | 文件映射、共享内存和用户 pager 的入口 |
| `vm_protect` | 设置 current protection，或降低 maximum protection | current/max 两级保护；读、写、执行权限 |
| `vm_inherit` | 设置新 task 如何继承一段映射 | `SHARE`、`COPY`、`NONE`；fork 的基础 |
| `vm_region` | 查询某个地址所在映射的范围、保护、继承、object name 和 offset | 可以扫描一个 task 的地址空间 |
| `vm_statistics` | 读取页大小、free/active/inactive/wired、fault、pagein/pageout、COW 等统计 | 观察 VM 行为的第一组指标 |
| `vm_wire` | 特权调用，使某段页不可分页或重新可分页 | 启动链、内核关键缓冲区和实时性；需要 host privileged port |
| `vm_machine_attribute` | 设置架构相关的 cache、迁移、复制属性 | 不是所有架构都支持，属于扩展接口 |

`vm_allocate` 分配的是虚拟地址范围，不是立即拿到一段连续物理内存。第一次访问时可以产生 zero-fill fault；如果系统压力较大，物理页也可能后来被回收。

`vm_map` 的关键参数包括：目标 task、虚拟地址/大小、对齐 mask、memory object、对象内 offset、是否 `copy`、current/max protection 和 inheritance。`memory_object == MEMORY_OBJECT_NULL` 时，Mach 可以建立零填充对象；使用真实 memory object 时，如果该对象尚未被这个内核使用，内核会向其 manager 发起 `memory_object_init`。同一对象已有活动映射时，后续 `vm_map` 不一定再次执行初始化握手。`vm_map` 返回“映射已建立”并不表示 Pager 已经准备好；Pager 必须先发送 `memory_object_ready`，否则后续数据/解锁请求不会正常推进。

### 3.2 跨 task 的数据复制接口

| 接口 | 作用 |
| --- | --- |
| `vm_read` | 从目标 task 读取页，结果通常由 Mach 在调用者中分配一块新区域返回 |
| `vm_write` | 把数据写入目标 task |
| `vm_copy` | 在同一目标 task 中复制一段区域；语义上类似 `vm_read` 加 `vm_write` |

Mach 消息本身也可以携带 out-of-line memory。内核不必逐字节复制整个文件或地址空间，而可以通过重映射、COW 和 port rights 传递大块数据。Hurd `io_read` 的实现会利用这种机制：服务器可以返回 MIG 管理的 out-of-line buffer，客户端再把它复制或接管到自己的地址空间。

### 3.3 Task 创建与 VM 继承

`task_create(parent, inherit_memory, &child)` 创建新 task。若 `inherit_memory` 为真，子 task 根据父 task 每个映射的 `vm_inherit` 属性获得地址空间：

- `VM_INHERIT_SHARE`：父子共享同一映射，可读写共享；GNU Mach 手册把它描述为 Mach task 间共享物理内存的路径；
- `VM_INHERIT_COPY`：逻辑上得到一份副本，通常以 copy-on-write 实现；
- `VM_INHERIT_NONE`：子 task 中没有这段映射。

Unix `fork` 通常把各个映射的继承属性设为 `VM_INHERIT_COPY`，因此父子一开始共享物理页并只读；任一方写入时触发 COW fault，Mach 才分配私有页。这里的 `VM_INHERIT_COPY` 是 task 创建时的继承策略，不是一个独立的“COPY 地址空间”对象。

### 3.4 默认 memory manager

`vm_set_default_memory_manager(host, &manager)` 设置 host 的默认 memory manager 接口。内核创建临时 memory object 时，会通过 `memory_object_create` 等消息把对象交给这个 manager；Mach 提供的是交接协议和 VM 机制，具体的默认 pager 通常是用户态服务。在 GNU/Hurd 系统中常见的实现是 `mach-defpager`，但具体启动方式和 swap 后端应以系统版本为准。

从教学角度，可以把默认 manager 看成“没有文件系统语义的临时对象后端”：它知道如何保存匿名页或交换页，却不知道 `/home/a` 这样的路径、inode 或目录权限。文件 pager 则相反，知道文件偏移和文件大小，并把同一对象的页映射到具体文件后端。两者都遵守 memory object 协议，但策略不同。

不要把“默认 pager”和“文件 pager”混为一谈：

- 默认 pager 面向内核创建的临时/匿名对象；
- 文件 pager 由文件系统或 translator 管理，负责把文件 offset 转成持久后端的读写。

---

## 4. Mach 的内部 VM 模型：为什么可以把分页放到用户态

Mach VM 的实现可以用四组数据结构理解。它们来自本项目保存的 *Machine-Independent Virtual Memory Management* 论文。

### 4.1 Resident page table

它记录当前驻留物理页的机器无关信息，例如：

- 页属于哪个 memory object；
- 对象内 offset；
- 是否被修改、是否被引用；
- 位于 free、active、inactive 等页队列中的哪一个；
- 是否存在多个 task 的映射。

物理页首先被看成 memory object 内容的缓存。页被换出后，永久数据由 pager 保存；页重新出现时，Mach 根据 object+offset 找到它。

### 4.2 Address map

每个 task 有一张 address map。每个 map entry 描述：

```text
虚拟地址区间 [va, va + length)
    ↔ memory object 的 [offset, offset + length)
    + current/max protection
    + inheritance
    + 是否 copy/shared 等属性
```

这张表用于：

- 缺页时从虚拟地址找到 object 和 offset；
- 按范围修改保护或继承属性；
- 分配和释放稀疏地址空间；
- fork/COW 时复制或拆分映射。

历史 Mach 采用按虚拟地址排序的链表，并用 hint 加速常见的缺页查找。现代实现可能使用不同的数据结构，但抽象仍是“task map entries 指向 memory object”。

### 4.3 Memory object 与对象树

一个对象有自己的页缓存和引用计数。为了实现 COW，Mach 使用 shadow object：

```text
child mapping -> shadow C -> parent object B -> file object A
```

如果 child 读取某页，Mach 沿 shadow 链找到 B 或 A 的只读页；如果 child 写入，Mach 在 C 中分配一个新页并把修改放进去。这样，未修改页无需复制。

shadow object 可能形成长链，因此 Mach 需要对象树管理和垃圾回收。当某个 shadow 完全覆盖或不再需要时，可以合并或回收中间对象。

共享映射还会引入一种容易漏掉的内部对象：**sharing map**。它本身类似一张 address map，多个 task 的 map entry 可以共同指向它，再由 sharing map 指向 memory object。对共享范围执行保护、拆分、合并等操作时，Mach 可以在 sharing map 层统一修改，而不必逐个改写每个 task 的地址映射。

因此要区分两种关系：shadow object 表示“写时复制后，修改页存放在哪里”；sharing map 表示“哪些地址空间共同引用同一组映射元数据”。它们都可能出现在一次 fork 或共享内存操作中，但解决的是不同问题。

sharing map 是 Mach VM 论文描述的内部实现模型，不是应用可以直接取得的公共 port 或稳定 ABI。GNU Mach 的具体数据结构可能不同；阅读源码时应把它当作解释共享映射的概念工具。

### 4.4 Pmap

`pmap` 是机器相关的硬件地址转换结构，例如页表或其他 MMU 表。Mach 把大部分策略保留在机器无关层，`pmap` 只负责：

- 建立/撤销虚拟到物理的硬件映射；
- 改变页保护；
- 清除 TLB 或同步处理器；
- 处理硬件引用位、修改位等。

论文强调，pmap 可以把硬件映射当作 cache：缺失时根据机器无关的 address map 和 resident page table 重新构造。这是 Mach 跨 VAX、Sun、RT PC 和多处理器架构移植的关键。

### 4.5 从 MIG 消息到 Pager 回调

`memory_object_*` 不是 Pager 通常手写的裸 `mach_msg` 解析器。MIG 会生成 `memory_object_server`（以及带序列号的 `seqnos_memory_object_server`）分派器：它检查消息 ID、解码端口和 out-of-line memory，再调用对应的 `memory_object_*` 处理函数。`libpager` 在这层之上把处理函数转成 `pager_read_page`、`pager_write_page` 等后端回调。

所以源码调用链常常是：

```text
Mach message
  -> seqnos_memory_object_server
  -> libpager 的 data-request 处理器
  -> 文件系统 Pager callback
  -> libstore / device / file
```

这也解释了为什么排查协议问题时，不能只在文件系统的 `pager_read_page` 里找错误；消息解码、序列号、port right 和缓冲区所有权都发生在更上游。

---

## 5. External pager：Mach VM 的核心用户态协议

### 5.1 为什么需要 external pager

如果所有文件系统和交换策略都写进内核，内核就会重新变成一个大内核。Mach 的选择是：

1. 内核维护地址空间、页表、物理页缓存和缺页控制；
2. 用户态 pager 维护 memory object 的内容来源和持久化策略；
3. 二者通过 Mach IPC 协作。

`vm_map` 时，客户端把 memory object port 交给内核。发生缺页时，内核向 pager 发消息；pager 通过另一个由内核提供的 control/request port，把页交回内核。

同一 memory object 相关的三个 port 可以这样记：

| 端口 | 谁主要持有/使用 | 用途 |
| --- | --- | --- |
| paging object / memory object port | pager 作为服务端；内核在映射时使用 | 内核对 pager 发数据请求、终止、copy 等消息 |
| memory control / request port | 内核在 `memory_object_init` 中交给 pager | pager 请求内核清理、刷新、解锁、改变缓存属性 |
| memory object name port | 内核生成 | 供 `vm_region` 等查询对象身份，通常不承担数据请求 |

不同 Mach 版本对类型名和旧接口的命名略有区别，但这个三端口关系是理解协议的关键。

### 5.2 内核发给 pager 的消息

| RPC/protocol | 含义 |
| --- | --- |
| `memory_object_init(memory_object, memory_control, memory_object_name, page_size)` | 某个对象第一次被映射到该 kernel；交付 control/name port 和使用页大小 |
| `memory_object_data_request(object, control, offset, length, desired_access)` | 缺页或预取请求：请提供对象中某个范围的数据，并至少允许指定访问 |
| `memory_object_data_unlock(...)` | 某些缓存页当前被锁住，要求 pager 允许某种访问 |
| `memory_object_lock_completed(...)` | 对 pager 发出的清理/刷新/加锁请求已完成 |
| `memory_object_change_completed(...)` | 对 pager 发出的属性变更已经完成 |
| `memory_object_copy(old_object, old_control, offset, length, new_object)` | 在 `COPY_CALL` 策略下，内核创建了对象副本，要求 pager 接管新对象 |
| `memory_object_data_return(object, control, offset, data, count, dirty, kernel_copy)` | 脏页或 precious 页写回 pager；旧版本常见 `memory_object_data_write` |
| `memory_object_terminate(object, control, name)` | 内核不再使用对象，交还相关 port rights |
| 默认 manager 专用 `memory_object_create` | 内核创建临时对象，请默认 manager 接管 |
| 默认 manager 专用 `memory_object_data_initialize` | 给新建临时对象提供初始数据 |

`memory_object_data_request` 不是普通同步函数调用，而是缺页路径上的请求。pager 必须在另一个消息中使用 `memory_object_data_supply`、`memory_object_data_error` 或 `memory_object_data_unavailable` 完成它。

### 5.3 Pager 发给内核的消息

| RPC/protocol | 含义 |
| --- | --- |
| `memory_object_ready(control, may_cache, copy_strategy)` | pager 已准备好接收/响应数据请求 |
| `memory_object_data_supply(control, offset, data, count, lock_value, precious, reply)` | 把页数据交给内核；可指定返回页的访问锁和是否 precious |
| `memory_object_data_provided(...)` | 较旧版本的数据提供形式；与 `data_supply` 的精确语义和可用性要按目标 MIG 定义核对 |
| `memory_object_data_error(control, offset, size, reason)` | 后端无法提供数据，例如 I/O 错误 |
| `memory_object_data_unavailable(control, offset, size, reason)` | 告知内核该范围不由 pager 直接提供；普通对象通常由内核补零，内核创建对象或 COW 副本则按对象规则处理 |
| `memory_object_lock_request(control, offset, size, should_clean, should_flush, lock_value, reply)` | 要求内核写回脏页、刷新缓存或收紧访问权限 |
| `memory_object_get_attributes(control, ...)` | 查询对象是否 ready、是否允许缓存以及 copy strategy |
| `memory_object_destroy(control, reason)` | 要求内核停止对该对象分页 |
| `memory_object_change_attributes(control, may_cache, copy_strategy, reply)` | 改变是否缓存对象和 COW 策略 |

在某些版本中，Mach 还会用 `memory_object_supply_completed` 通知 pager 一次 supply 操作已经完成。它是完成通知，不是让 pager 再次提供页面的请求；是否存在以及参数形式同样以目标版本为准。

`lock_value` 表示禁止的访问类型，而不是允许的访问类型；这是阅读 MIG 定义时很容易弄反的地方。`desired_access` 则表示缺页请求方希望获得的读、写、执行权限。

`may_cache_object` 和 `copy_strategy` 也会改变整个对象的生命周期：前者允许内核在最后一个映射消失后继续保留缓存；后者通常取以下三种策略之一：

| 策略 | 含义 |
| --- | --- |
| `MEMORY_OBJECT_COPY_NONE` | 对象被复制时不要求额外的 Pager 操作 |
| `MEMORY_OBJECT_COPY_CALL` | 复制前通知 Pager，由 `memory_object_copy` 接管新对象 |
| `MEMORY_OBJECT_COPY_DELAY` | Pager 承诺不在内核之外修改对象，内核可用自己的 COW 机制；历史实现中常见 |

因此，看到 `memory_object_copy` 并不能推断所有 COW 都会经过该 RPC；很多普通的 COW 路径使用 `COPY_DELAY`，由 Mach 内部建立 shadow object。

### 5.4 对象生命周期

一个典型的 external pager 生命周期是：

```text
客户端得到 memory object port
  -> vm_map
  -> kernel: memory_object_init
  -> pager: memory_object_ready
  -> 多次 data_request / data_supply
  -> 脏页 data_return，或 pager 主动 lock_request
  -> 最后一处映射释放
  -> kernel: memory_object_terminate
```

pager 可以选择让 kernel 在无映射后继续缓存对象（`may_cache_object`），这样再次映射时可以避免重新读后端。文件系统 pager 通常知道自己的缓存一致性和后端生命周期，因而会决定是否缓存、何时请求清理或刷新；页面真正何时被回收仍由 Mach 的 VM 策略决定。

如果 Pager 主动调用 `memory_object_destroy`，内核可以停止对该对象提供分页，并把相关权利在 `memory_object_terminate` 中归还。这个路径可能早于“最后一个映射自然释放”，因此对象销毁和映射解除不能画成唯一的线性顺序。

若 Pager 还需要拿回内核缓存中的脏数据，不能直接 destroy 后再期待数据出现；应先用 `memory_object_lock_request` 请求 clean/flush，并等待 `memory_object_lock_completed`，再销毁对象。这个顺序是对象终止时避免数据丢失的关键。

### 5.5 如何读懂协议参数：页粒度、访问权和缓冲区所有权

External Pager 的消息看起来像普通 RPC，但它们携带的是页缓存状态，而不是任意长度的字节流。阅读某个 MIG 定义时，可以按下面的顺序检查：

1. `offset` 和 `length/count` 是否按 `memory_object_init` 给出的页大小对齐。分页消息通常以页为单位；文件的字节偏移需要先转换成对象页偏移。
2. 请求中的 `desired_access` 表示缺页方希望得到的访问类型（读、写、执行）。它不是 pager 可以无条件授予的权限，最终还要受映射的 current/max protection 限制。
3. `memory_object_data_supply` 中的 `lock_value` 表示返回页面禁止的访问类型，和 `desired_access` 的含义方向相反。把二者混淆会造成“页面已经交付却一写就 fault”的错误。
4. 数据参数经常是 MIG 的 out-of-line memory。服务端必须确认缓冲区是借用、复制还是移动给内核，并在协议要求时释放自己的引用；不能把它当作普通 `malloc` 缓冲区随意复用。
5. `memory_object_data_error` 和 `memory_object_data_unavailable` 表达不同状态。前者是后端无法提供数据，后者表示该范围没有可直接交付的数据，内核能否零填充或沿 shadow 链查找取决于对象类型和当前协议。

`memory_object_data_return` 也不能理解成一次普通的 `write`。它可能带有脏页、precious 页和 `kernel_copy` 等信息，说明内核是否仍保留副本以及 pager 是否必须保存数据。真实实现应同时阅读 MIG 声明、调用方释放缓冲区的代码和 pager 回调；仅凭函数名推断所有权是不安全的。不同 Mach 版本还可能使用 `memory_object_data_write` 等旧名称，因此本文的名称以概念角色为准。

`memory_object_data_supply` 接受的是完整的 memory-object 页；`offset`、`data_count` 和页内地址必须满足该次 `memory_object_init` 通知的页大小约束，部分页不能当作普通字节写入。Pager 可以预先 supply 数据，但通常是在收到 `memory_object_data_request` 后交付，并通过 `reply` 端口了解 supply 是否完成。

---

## 6. 缺页、写回和 COW 的逐步推导

### 6.1 文件映射的读缺页

假设 `ext2fs` 将文件的第 0 页映射给应用。

1. 应用访问虚拟地址 `V`。
2. 硬件页表没有有效映射，CPU 产生 page fault。
3. Mach 根据 task 的 address map 找到：
   - 该地址属于文件 memory object `O`；
   - 对象内偏移是 `off = 0`；
   - 允许的访问是读。
4. 如果 resident page table 已有 `(O, 0)`，Mach 直接建立 pmap 映射并恢复线程。
5. 否则 Mach 向 pager 发送 `memory_object_data_request(O, control, 0, page_size, READ)`。
6. Hurd pager 将 `offset=0` 转换为后端访问。对 ext2fs 来说，`pager_read_page` 找到磁盘块，再通过 `libstore` 读取设备/文件。
7. pager 将对齐的页 buffer 通过 `memory_object_data_supply` 交给 Mach。
8. Mach 将页放入 resident page table，设置 pmap，恢复 faulting thread。

注意：第 5 步之后，缺页线程可能处于等待状态。pager 不能把请求路径设计成依赖同一个被缺页阻塞的线程，否则会死锁。因此 Hurd pager 通常使用独立的消息处理线程，并对启动阶段的关键 server 使用 wired memory。

### 6.2 脏页写回

1. 映射允许写，应用修改页。
2. Mach 在 resident page entry 和硬件映射中记录 dirty 状态。
3. 内存压力、pager 主动清理，或映射解除时，Mach 需要回收页。
4. Mach 在确实需要回收或清理该脏页时发送 `memory_object_data_return`，并把数据 buffer、dirty/precious 状态和 `kernel_copy` 提示交给 pager；单纯解除一个映射并不保证一定产生这条消息。
5. pager 将对象 offset 转成 backing store 的位置，写入 ext2、文件、设备或其他后端。
6. 后端确认后，pager 可以释放 MIG 传来的 out-of-line buffer；Mach 也可以继续保留 clean cache copy。

`memory_object_lock_request(should_clean=true, should_flush=true, ...)` 是概念写法；GNU Mach 接口中的 `should_clean` 实际是 `memory_object_return_t`，可表达不返回、返回脏页或返回全部相关页等策略。这个请求可以把“写回”和“从内核缓存中淘汰”分成两个维度。它只影响调用时已经缓存的范围；之后新调入的页仍由对象的普通保护和缓存策略决定。文件系统卸载、`msync` 类操作或一致性控制会依赖这种能力。

### 6.3 Copy-on-write

对于普通的私有可写映射，父 task 的页由 memory object `A` 支撑。`fork` 后子 task 的 map entry 初始仍指向 `A`，但两边的页被设为不可写。

```text
parent ─┐
        ├─ read-only -> A[offset]
child  ─┘
```

子 task 写入时：

1. 触发 protection/COW fault；
2. Mach 创建或使用 shadow object `S`；
3. 如果 `S[offset]` 尚无内容，Mach 从 `A[offset]` 复制一页；
4. 将子 task 的映射改为 `S[offset]`，恢复写权限；
5. 父 task 仍读取 `A[offset]`。

如果多次 fork，可能出现 `S3 -> S2 -> S1 -> A` 的链。Mach 会在安全时合并或回收 shadow object，防止链条无限增长。

下面三个名字经常被混为一谈，但位于不同层次：

- `VM_INHERIT_COPY`：`task_create` 创建子 task 时，映射如何继承；
- `vm_map(copy=TRUE)`：建立一段映射时，请求一个与原映射隔离的逻辑副本；
- `MAP_PRIVATE`：POSIX/Hurd 文件映射语义，通常借助前两类 Mach COW 机制实现。

它们都可能导致 shadow object，却不是同一个标志，也不保证产生同一种对象生命周期。

### 6.4 读写共享

`VM_INHERIT_SHARE` 或将同一 memory object 以非 COW 方式映射到多个 task，可以让它们指向同一组可写数据；是否立即共享同一驻留物理页，还取决于对象类型、缓存状态和具体映射策略。它和 COW 的区别是：一个 task 的写入按共享对象语义成为其他 task 可见的数据，不会自动建立私有 shadow。GNU Mach 手册特别把 `VM_INHERIT_SHARE` 配合 task 创建列为 task 间共享物理内存的直接路径。共享内存仍需要应用自己设计锁、环形队列或通知协议。

---

## 7. Hurd 如何建立在 Mach VM 之上

Hurd 没有一个等价于 Linux `mm/` 的单一“内存服务”。VM 相关职责分散在：

- glibc 的 Mach/Hurd 适配层：把 `mmap`、`read`、`fork`、`exec` 变成 Mach 和 Hurd RPC；
- 文件 server/translator：提供 `io`、`fs` 等对象接口；
- `libpager`：把一个用户态对象包装成 Mach external pager；
- `libdiskfs`、`libnetfs`、`libtrivfs`：分别支持磁盘文件系统、网络/虚拟文件系统和简单单节点服务；
- `libstore`：把设备、文件、task、zero store 以及组合 store 统一成 block-oriented backing store；
- `exec` server：创建新 task 后装入可执行映像并建立其地址空间；
- GNU Mach 默认 pager：负责匿名/临时 memory object 的默认分页策略。

这就是 Hurd 设计中“机制和策略分离”的具体表现：Mach 提供可以映射、保护、缺页和写回的机制；哪个文件对应哪个块、是否从网络取、如何缓存、是否允许用户替换服务，由 Hurd server 决定。

### 7.1 `libpager`：把用户态对象变成 memory object

`libpager` 是 Hurd 对 Mach external pager 接口的封装。一个基于它的 server 通常需要提供与后端有关的回调，例如：

- 根据 object offset 读取一个页；
- 把脏页写回；
- 判断某个范围是否存在数据；
- 处理对象终止、清理和缓存策略；
- 在后端错误时报告错误。

在 ext2fs 中，文档记录了典型路径：

```text
memory_object_data_request
  -> libpager 的 _pager_seqnos_memory_object_data_request
  -> ext2fs 的 pager_read_page
  -> libstore 的 store_read
  -> device_read 或文件/设备后端
```

`libpager` 并不替 ext2fs 决定文件系统语义。它负责 Mach 协议、port、序列号和页粒度的通用工作；ext2fs 负责把文件逻辑偏移转换成 ext2 块，libstore 再负责把块请求转换成具体存储。

### 7.2 `libstore`：把 backing store 从文件系统中抽出来

本项目的 Hurd Workshop 资料把 store 描述为“可读、可能可写的固定大小块集合”。典型 primitive store 包括：

- `device:hd2`、`device:hd0s1`：Mach device；
- `file:/tmp/disk_image`：普通文件；
- `task:PID`：另一个 task 提供的对象；
- `zero:4m`：零填充存储。

store 还可以组合：`copy`、`concat`、`gunzip`/`bunzip2`、`ileave`、`remap` 等。这样 `libdiskfs` 不需要为每一种设备和包装格式重新实现 pager；它只需要针对统一的 block store 读写。

### 7.3 `libdiskfs`、`libnetfs`、`libtrivfs`

| 库 | 主要对象 | VM 相关作用 |
| --- | --- | --- |
| `libdiskfs` | ext2fs、UFS、tmpfs 等拥有 backing store 的文件系统 | 将 inode/file offset 对应到 store，并通常为文件建立 pager |
| `libnetfs` | ftpfs、nfs 等网络或虚拟层次文件系统 | 通过网络或上层 server 取得文件内容；是否支持高效 `io_map` 取决于实现 |
| `libtrivfs` | 单文件、设备文件、`/servers/password` 等简单 translator | 通常用 `io_read`/`io_write` 提供数据；也可以实现映射，但不是强制要求 |

不是所有 Hurd server 都必须提供 memory mapping。一个纯 RPC 服务可以只实现控制接口；`io_map` 是一种性能和语义优化，而不是 server 存在的必要条件。

### 7.4 `libpager` 回调和对象生命周期

`libpager` 的价值不只是生成几个 RPC stub。它把“Mach 消息状态机”和“后端如何读写一页”分开。一个典型 Pager 的概念创建流程是：

```text
后端对象
  -> pager_create
  -> 建立 memory-object port、request/control port 和服务线程
  -> pager_demuxer 分派 memory_object_* 消息
  -> pager_read_page / pager_write_page 访问后端
```

常见回调及其职责如下（精确签名必须以目标版本的 `<hurd/pager.h>` 为准）：

| 回调 | 作用 |
| --- | --- |
| `pager_read_page` | 根据对象页偏移取得一页内容，必要时生成零页或报告错误 |
| `pager_write_page` | 将归还的脏页写入文件、设备、交换区或其他 store |
| `pager_unlock_page` | 在内核要求放宽页面锁定/保护时更新后端状态 |
| `pager_report_extent` | 报告对象有效范围，帮助处理文件大小和越界请求 |
| `pager_clear_user_data` | 在 pager 对象终止时释放后端关联数据 |
| `pager_dropweak` | 处理弱引用消失，避免把最后一个映射引用和后端引用混为一谈 |

`pager_sync`、`pager_sync_some`、`pager_flush`、`pager_flush_some` 等高层操作用于把脏页推进到后端或请求缓存刷新；它们与应用的 `msync` 有联系，但不是同一个公共 RPC。`pager_shutdown` 也不等于立即杀死服务线程：仍在处理的缺页、写回和终止通知必须先完成或被安全取消。

因此至少要分别记录四种生命周期：文件描述符、客户端映射、Mach memory object/Pager 以及 backing store。关闭一个文件描述符并不会使仍被映射的对象失效；相反，Pager 退出时，已有映射能否继续工作取决于内核缓存、后端副本和 translator 的恢复策略。

---

## 8. Hurd 对外提供哪些 RPC protocol

这里要区分三层协议：

### 8.1 Mach 内核与 pager 之间的 VM protocol

这是上一节的 `memory_object_*` 协议。它面对的是 memory object，不是文件名。典型参与者是：

```text
Mach kernel  <---- memory_object_* messages ---->  Hurd pager
```

它们关注页、offset、protection、dirty、cache 和生命周期。

### 8.2 Hurd 客户端与文件/服务 port 之间的 protocol

Hurd 用 MIG 从 `hurd/*.defs` 生成 RPC stub。和 VM/文件访问最相关的接口类别有：

| 接口族 | 典型操作 | 作用 |
| --- | --- | --- |
| `io` | `io_read`、`io_write`、`io_seek`、`io_stat`、`io_map` 等 | 对已打开对象进行 I/O、属性查询和可能的 memory mapping |
| `fs` | `dir_lookup`、目录读取、文件创建/删除等 | 路径解析和目录服务，返回对象 port |
| `fsys` | `fsys_getroot`、`fsys_startup`、`fsys_init`、`fsysopts` 等 | translator/filesystem 的控制、启动、root 和配置 |
| `exec` | `exec_exec`、`exec_init` 等 | 创建/装载新程序，传递 task、端口和启动状态 |
| `auth`、`proc`、`startup` | 身份、进程登记、启动协作 | 不直接补页，但决定 server 是否有资格访问资源 |

其中 `io_map` 的语义是：客户端向一个打开的文件/对象请求可以映射的数据对象；Hurd 的常见接口会返回读用和写用的 memory object port（只读对象可能没有可写 port），客户端随后按映射权限选择端口并调用 Mach `vm_map`。文件长度和可映射范围通常由 `io_stat` 等接口取得，具体参数和返回值会随 Hurd 版本变化，应以对应版本的 `hurd/io.defs` 为准。

`MAP_SHARED` 和 `MAP_PRIVATE` 通常会影响客户端随后调用 `vm_map` 时的 `copy`、保护和继承参数：前者希望多个映射共享同一对象的修改，后者要求写入时形成私有 COW 状态。这个映射关系由 Hurd 的 `mmap` 实现组合出来，不能把 POSIX 标志直接当成一个 Mach RPC 参数。

`io_read` 是另一条路径。它直接请求数据，server 可以把数据放入 MIG out-of-line buffer 返回。缓冲区可能由 Mach 从 server 地址空间移动到 client；glibc 随后通常仍要把返回区域复制到应用传入的 `read` buffer，因此 OOL 传输不自动等于对应用 buffer 的零拷贝。`io_read` 通常比 page fault 显式、容易实现，但可能产生额外的复制；`io_map` 则允许内核按页缓存、延迟取数和直接共享。

### 8.3 Hurd server 内部的 pager callback

`libpager` 的 `pager_read_page`、`pager_write_page` 一类函数是库内部回调，不是普通应用可以直接调用的公共 RPC。它们把 Mach 的 `memory_object_data_request` 转译成文件系统或 store 的操作。

要记住：

```text
应用 -> io_map/io_read       是 Hurd 对象协议
Mach -> memory_object_*      是 VM/pager 协议
pager -> store_read/write    是 server 内部策略/库调用
```

把三者混为一个“read RPC”会导致难以理解的死锁、缓存和权限问题。

### 8.4 ABI 对照：从 MIG `.defs` 到 `libpager` 回调

前文的“接口”一词包含了四个不同层次。它们的名字可能相似，但调用者、目标端口、数据单位和错误返回方式都不同。学习源码时，先把一个函数归入下面四层，再去读它的参数：

| 层次 | 代表接口 | 调用者 → 接收者 | 目标 | 数据单位 | 是否是线上 RPC |
| --- | --- | --- | --- | --- | --- |
| Mach VM 控制 | `vm_map`、`vm_protect`、`vm_region` | 应用/服务 → Mach 内核 | `task_t`、`host_t` 等端口 | 地址区间和对象 offset | 是，由 MIG/`mach_msg` 实现 |
| Mach external pager | `memory_object_data_request`、`memory_object_data_supply` | 内核 ↔ Pager | memory object port 或 control port | 页和对象内 offset | 是，由 `memory_object.defs` 等定义 |
| Hurd 对象协议 | `io_map`、`io_read`、`io_write` | 客户端 → translator | 文件/服务的 `io_t` port | 文件字节、文件 offset | 是，由 `hurd/*.defs` 定义 |
| Pager 后端回调 | `pager_read_page`、`pager_write_page` | `libpager` → 文件系统/store | C 结构体和回调表 | 一页或后端块 | 否，是进程内函数调用 |

因此，`io_map` 不是 `vm_map` 的别名，`pager_read_page` 也不是一个应用可以发送的 RPC。典型的边界是：

```text
应用/应用库 --io_map--> 文件 translator
应用库      --vm_map--> Mach 内核
Mach 内核    --data_request--> libpager 分派器
libpager     --pager_read_page--> 文件系统或 libstore
```

#### 8.4.1 先读懂 `vm_map` 的 ABI

GNU Mach Reference Manual 1.3.99（[VM 接口第 48 页及 external memory management 第 52--61 页](../docs/references/gnu-mach-reference-manual.pdf)）给出的核心原型（类型名在不同头文件中可能使用 typedef 别名）是：

```c
kern_return_t vm_map(
    task_t               target_task,
    vm_address_t        *address,
    vm_size_t             size,
    vm_address_t          mask,
    boolean_t          anywhere,
    memory_object_t       memory_object,
    vm_offset_t           offset,
    boolean_t             copy,
    vm_prot_t              cur_protection,
    vm_prot_t              max_protection,
    vm_inherit_t           inheritance);
```

把它按“参数属于哪一层”标注，比死记顺序更有用：

| 参数 | 属于什么问题 | 关键语义 |
| --- | --- | --- |
| `target_task` | 能力/地址空间 | 要修改哪个 task；调用者必须有相应 task port right |
| `address`、`size`、`mask`、`anywhere` | 虚拟地址分配 | `address` 是入参/出参；`mask` 约束对齐；`size` 最终按页取整 |
| `memory_object`、`offset` | 对象内容 | 从哪个 memory object 的哪个字节偏移开始映射；`offset` 必须按页对齐 |
| `copy` | 建立映射时的复制语义 | `TRUE` 请求逻辑副本；不要把它等同于 `VM_INHERIT_COPY` 或 `MAP_PRIVATE` |
| `cur_protection`、`max_protection` | 当前和上限权限 | 当前权限不能超过最大权限；它们还受 Pager 返回的 `lock_value` 影响 |
| `inheritance` | 新 task 继承 | 控制 `task_create`/fork 类操作中的 `SHARE`、`COPY`、`NONE` |

两个容易被忽视的 ABI 事实是：

1. `memory_object == MEMORY_OBJECT_NULL` 表示零填充对象；传入任意普通 port 并不会自动使它成为可分页对象。GNU Mach 手册明确要求该 port 必须由真正的 memory manager 实现，否则访问映射可能永久阻塞或产生内存异常。
2. `vm_map` 返回成功只表示地址映射已建立。若这是该内核第一次使用该对象，后续会异步发送 `memory_object_init`；内核是否继续处理缺页请求取决于 Pager 随后发送 `memory_object_ready`。

#### 8.4.2 `memory_object_*`：端口、方向和生命周期

一个 Pager 至少要区分三个端口。`memory_object` 是 `vm_map` 传给内核、内核用来向 Pager 发数据请求的对象 port；`memory_control` 是 `memory_object_init` 交给 Pager、Pager 用来请求内核执行缓存管理的 control port；`memory_object_name` 只用于对象命名和 `vm_region` 查询。它不是数据请求端口。

下表按照 GNU Mach Reference Manual 第 6 章的接口关系整理。`reply`/`reply_to` 是可选的完成通知端口；没有 reply 的消息仍然可能在发送时返回 Mach IPC 错误，但不会得到一个同步的业务结果。

| 消息 | 方向 | 发送到哪个端口 | 主要参数/结果 | 处在生命周期的哪里 |
| --- | --- | --- | --- | --- |
| `memory_object_init(object, control, name, page_size)` | Mach → Pager | `memory_object` | 交付 control/name rights 和本内核采用的页大小 | `vm_map` 首次使用对象后 |
| `memory_object_ready(control, may_cache, copy_strategy)` | Pager → Mach | `memory_control` | 宣布可处理数据和解锁请求；设置缓存和复制策略 | `init` 之后，首次缺页之前 |
| `memory_object_data_request(object, control, offset, length, desired_access)` | Mach → Pager | `memory_object` | 请求至少具备指定访问权限的一段完整页 | 缺页或预取 |
| `memory_object_data_supply(control, offset, data, count, lock_value, precious, reply)` | Pager → Mach | `memory_control` | 交付页；`lock_value` 指禁止的访问；可标记 precious | 响应 `data_request`，也可预先 supply |
| `memory_object_supply_completed(...)` | Mach → Pager | `data_supply` 指定的 reply port | 报告 supply 的完成状态和可能的错误 offset | 需要异步确认时 |
| `memory_object_data_return(object, control, offset, data, count, dirty, kernel_copy)` | Mach → Pager | `memory_object` | 归还 dirty/precious 页；`kernel_copy` 说明内核是否保留副本 | 回收、清理或显式刷新时 |
| `memory_object_data_write(...)` | Mach → Pager | `memory_object` | `data_return` 的旧版本形式，缺少 dirty/precious 等信息 | 兼容旧 ABI；不要作为新协议首选 |
| `memory_object_data_error(control, offset, size, reason)` | Pager → Mach | `memory_control` | 后端无法提供数据，例如 I/O 错误 | 完成 `data_request` 的错误分支 |
| `memory_object_data_unavailable(control, offset, size, reason)` | Pager → Mach | `memory_control` | 该范围没有可直接交付的数据；内核按对象类型补零或沿复制关系查找 | 完成 `data_request` 的“无数据”分支 |
| `memory_object_lock_request(control, offset, size, should_clean, should_flush, lock_value, reply_to)` | Pager → Mach | `memory_control` | 清理、刷新缓存并收紧访问权限 | `msync`/卸载/一致性控制 |
| `memory_object_lock_completed(...)` | Mach → Pager | `reply_to` | 报告 `lock_request` 的动作已完成 | `lock_request` 完成后 |
| `memory_object_data_unlock(object, control, offset, length, desired_access)` | Mach → Pager | `memory_object` | 请求 Pager 允许缓存页恢复某种访问 | 先前被 lock 后再次访问 |
| `memory_object_copy(old_object, old_control, offset, length, new_object)` | Mach → Pager | 旧对象的 `memory_object` | 通知 `COPY_CALL` 策略下产生了新对象；新对象随后会收到 `init` | 显式复制且 copy strategy 为 `CALL` |
| `memory_object_get_attributes(control, ...)` | Pager → Mach | `memory_control` | 读取 `object_ready`、`may_cache_object` 和 `copy_strategy` | 对象运行期间查询属性 |
| `memory_object_change_attributes(control, may_cache, copy_strategy, reply_to)` | Pager → Mach | `memory_control` | 修改缓存/复制策略 | 对象运行期间 |
| `memory_object_change_completed(...)` | Mach → Pager | `reply_to` | 报告属性变更已完成 | `change_attributes` 完成后 |
| `memory_object_terminate(object, control, name)` | Mach → Pager | `memory_object` | 归还 control/name 等 rights，表示内核结束使用 | 最后映射解除或被 destroy |
| `memory_object_destroy(control, reason)` | Pager → Mach | `memory_control` | 要求内核停止该对象的分页活动 | Pager 主动拒绝/销毁对象 |

这里有三个方向性检查，可以用来排查大多数协议误读：

- `desired_access` 是“请求至少允许哪些访问”，而 `lock_value` 是“返回页面禁止哪些访问”。前者出现在内核的 request，后者出现在 Pager 的 supply。
- `should_clean` 不是简单的 `bool`。GNU Mach 使用 `memory_object_return_t` 表达不归还、归还 dirty 页或归还全部相关页等策略；`should_flush` 决定是否还要使缓存失效。
- `data_supply` 和 `data_return` 的数据通常是 MIG 的 out-of-line memory。发送方必须按消息描述符的移动/复制和 deallocate 位处理所有权；不能只看 C 指针类型就决定何时 `free`。数据 supply 只接受完整页，部分页会被丢弃。

`memory_object_terminate` 和 `memory_object_destroy` 也不是互相替换的“关闭函数”：前者是内核通知并归还 rights，后者是 Pager 请求内核停止分页。Pager 若要在 destroy 前取回脏缓存，应先发 `lock_request(should_clean, should_flush, ...)`，等待 `lock_completed`，再销毁对象。

`seqnos_memory_object_*` 是同一组消息的序列号分派版本。序列号用于检查来自同一发送关系的消息顺序，不改变页面、端口和生命周期语义；看到它时不要把它误认为另一套 VM protocol。

#### 8.4.3 Hurd `io_map`、`io_read` 与 `libpager` 的 ABI

Hurd 的 `.defs` 文件在 MIG 中同时描述消息方向和数据传输方式，生成的 C 原型还会加入隐含的 reply port、out 参数长度和类型转换。可以先用下面的“概念 IDL”读接口，再在目标系统的 `hurd/io.defs` 中核对精确声明：

```text
io_map(io_object) -> (read_memory_object, write_memory_object)
io_read(io_object, offset, amount) -> (bytes, bytes_count)
io_write(io_object, offset, bytes) -> (written_count)
```

这段写法表达的是稳定的协议角色，不承诺每个版本的 typedef、`in/out` 注解或错误类型完全相同。`io_map` 通常返回读用和写用的 memory-object send right；只读对象可能把写 port 返回为 `MACH_PORT_NULL`。文件长度通常另由 `io_stat` 等接口取得，映射的起始 offset 和长度再作为 `vm_map` 参数传给 Mach；不要假定 `io_map` 自己一定返回一个“范围结构”。

| 接口 | 接收端 | 端口角色 | 参数单位 | 返回路径 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| `io_map` | 文件 server/translator | 打开的 `io_t` port | 对象能力；不是页数据 | 返回 memory-object ports | 客户端调用 `vm_map` |
| `io_read` | 文件 server/translator | 打开的 `io_t` port | 字节 offset 和 amount | inline 或 out-of-line bytes | glibc 复制/接管到 `read` buffer |
| `io_write` | 文件 server/translator | 打开的 `io_t` port | 字节 buffer、offset 和 length | 返回实际写入字节数/错误 | server 更新后端或 pager 状态 |
| `pager_read_page` | `libpager` 的后端对象 | 进程内 callback，不是 port | memory-object 页 offset | 填充一页或报告错误 | `data_supply`/error/unavailable |
| `pager_write_page` | `libpager` 的后端对象 | 进程内 callback，不是 port | dirty 页和对象页 offset | 写入文件、设备或 store | 库按所有权规则完成回调后释放 OOL buffer |
| `pager_unlock_page` 等 | `libpager` 的后端对象 | 进程内 callback，不是 port | 页和保护状态 | 更新后端状态 | 响应 `data_unlock`/lock 状态 |

`io_read` 的一个教学陷阱是：`.defs` 中的 out-of-line `data` 通常会在生成的 C 函数中表现为 `data` 加 `data_len`（或 `dataCnt`）两个参数；服务器端 `trivfs_S_io_read` 还可能看到 `cred`、`reply`、`reply_type` 等由 `libtrivfs` 的 INTRAN/OUTTRAN 加入的参数。这些不是额外的 Hurd RPC，而是 MIG 和服务器库为同一条消息生成的包装层。

因此，读一次文件可能有两种完全不同的 ABI 路径：

```text
read(fd, buf, n)
  -> io_read(io_port, offset, n)
  -> inline/OOL bytes
  -> glibc 拷贝到 buf

mmap(fd, ...)
  -> io_map(io_port)
  -> vm_map(task, memory_object, offset, ...)
  -> memory_object_init/ready
  -> data_request/data_supply
  -> pager_read_page -> store_read
```

#### 8.4.4 如何从源码验证一张 ABI 表

本仓库保存的是 GNU Mach Reference Manual 和 Hurd 文档快照，没有与当前系统绑定的一份 `hurd/io.defs`、`memory_object.defs` 和 `<hurd/pager.h>`。在真实 Hurd checkout 或安装树中，应按以下顺序核对，而不是直接复制本文的概念原型：

```sh
# Hurd 对象协议和服务器回调
rg -n 'routine (io_map|io_read|io_write)' \
   /usr/include/hurd/io.defs hurd/io.defs
rg -n 'pager_(read_page|write_page|unlock_page)|pager_methods' \
   /usr/include/hurd/pager.h hurd/libpager

# GNU Mach external pager 协议
rg -n 'memory_object_(init|ready|data_request|data_supply|data_return|terminate)' \
   /usr/include/mach/*.defs gnumach/include/mach
```

然后对每个接口回答五个问题：

1. 消息从哪个对象发给哪个对象，使用的是 `memory_object`、`memory_control` 还是 reply port？
2. 参数是字节、页、保护位还是 port right？是否要求按 `memory_object_page_size` 对齐？
3. 数据是 inline 还是 out-of-line？发送后哪一方拥有并负责释放 buffer？
4. 调用是否有业务 reply，还是只有 MIG 的传输错误？完成通知在哪个端口返回？
5. 该调用发生在对象创建、缺页、写回、属性变更还是销毁阶段？

练习：任选一次 `mmap` 缺页，把 `io_map`、`vm_map`、`memory_object_data_request`、`memory_object_data_supply` 和 `pager_read_page` 写进同一张表。若能对每一行标出“端口、页/字节、buffer 所有权、完成通知”，就已经把 Hurd 的三层 RPC 和一层库回调分开了。

---

## 9. 一次 `read` 和一次 `mmap` 的差别

### 9.1 普通 `read`

本项目保存的 Hurd `io_path` 文档给出了一条具体调用链：

```text
glibc read
  -> _hurd_fd_read
  -> __io_read（MIG 生成的 RPC）
  -> translator 的 _Xio_read
  -> translator 的 io_read 回调
```

`libtrivfs` translator 可以直接填充一个 buffer；如果结果超过现有 buffer，MIG 可以使用 out-of-line memory，让 server 返回一块页对齐区域。客户端 stub 再把它转换成调用者提供的 `read` buffer。

对于基于 `libdiskfs` 的文件系统，`io_read` 可能调用 `pager_memcpy`，将文件对应的 memory object 临时投影到处理该 RPC 的文件服务器地址空间；服务器随后从这段投影读取数据，因而由服务器线程触发缺页。服务器完成复制后，再通过 inline 或 MIG out-of-line buffer 把结果返回客户端。这样即使用户调用的是 `read`，内部也可能走到 pager，而不是先把整个文件读进服务器的普通缓冲区。

### 9.2 `mmap`

`mmap` 更直接暴露 Mach 的对象模型：

1. glibc 对文件 port 发 `io_map`；
2. server 返回 memory object port；
3. glibc 调 `vm_map`；
4. 访问页时由 Mach 和 pager 按需补页；
5. 修改页时按 pager 协议写回。

`read` 的应用语义是“现在把字节交给我”；`mmap` 的应用语义是“给我一个地址范围，我以后自己访问”。后者允许 page cache、COW、预取和多 task 共享更自然地发生。

---

## 10. `exec` 如何使用 VM 能力

Hurd 的 `execve` 不是一个“内核直接装 ELF”的单一操作。其职责分布大致是：

1. glibc 发送请求给保存可执行文件的文件 server；
2. 文件 server 检查执行权限、setuid 等安全策略；
3. 文件 server 调用 exec server；
4. exec server 解析 ELF、脚本或其他 BFD 支持的格式；
5. exec server 为代码、只读数据、可写数据、栈和动态链接器在目标 task 中建立映射；目标 task 可能是复用的旧 task，也可能是文件 server 为安全策略创建的新 task；
6. 文件的可执行段可以由文件 server 提供的 memory object 支撑；
7. 新 task 从 C 库入口开始，再向 exec server 请求参数、环境、当前目录等启动状态。

文件 server 还要决定是否可以复用原 task：普通 exec 可以在既有 task 上替换映像；当 setuid、执行不可读文件或其他权限提升使旧 task 的 port 能力不应继续暴露时，文件 server 会要求在新的 Mach task 中装载映像。exec server 负责装载目标 task，而不是无条件决定“总是新建 task”。

这样设计的好处是：

- 文件格式解析不进 Mach；
- 可执行文件可以由不同 translator 提供；
- 代码段可以按页加载，而不是整个文件读入内存；
- 只读文本段可以在多个进程间共享；
- 安全敏感的 task/port 继承由文件 server、exec server 和 Mach task/port 权限共同决定。

---

## 11. 安全、死锁和性能：微内核 VM 最容易被忽略的部分

### 11.1 Port right 是服务能力，VM protection 是页能力

一个进程要映射文件，至少涉及两种权利：

- 找到文件 server 的 port，并获得 `io_map` 等 RPC 的调用权；
- 获得 server 返回的 memory object send right，并以受限的 current/max protection 映射。

目录权限、translator 的拥有者、Mach port right 和页保护共同构成最终安全边界。把文件名传给另一个 server，不等于把相同的访问能力传给它；直接传递 port right 才是能力转移。

### 11.2 缺页路径会阻塞，pager 不能依赖自己等待的资源

缺页是内核发给用户态 pager 的 RPC。若 pager 为了读页又访问一个会触发同一个 pager 缺页的映射，或者同步等待被缺页阻塞的线程，就可能形成环路。

本项目的 Mach 文档明确提醒：对普通用户态 server 做同步调用可能不安全，默认 pager 是特殊情况。实际设计中需要：

- 给 pager 准备独立 RPC 线程；
- 让关键启动代码和必要缓冲区 wired，避免启动阶段自身分页；
- 避免在持有文件系统锁时等待会回调同一文件系统的 pager 请求；
- 对 `memory_object_data_request`、数据写回和对象终止分别设计锁顺序。

### 11.3 外部 pager 的代价

优点：

- 文件系统和交换策略可以在用户态替换；
- 不同后端可以共享同一 pager 库；
- 允许按页按需读取和 COW；
- server 崩溃不会必然摧毁整个内核。

代价：

- 缺页可能增加 Mach IPC、线程调度和地址空间切换；
- pager 与文件系统锁组合复杂；
- 缓存一致性和写回顺序由多个组件共同决定；
- pager 崩溃时，已有映射可能阻塞或得到内存错误，恢复依赖 translator 重启和后端语义。

这也是为什么 Mach 的 VM 不是“只有几个 syscall”：它把很多高层 VM 策略放进了内核，同时把 backing-store 策略外移。Mach 是第一代微内核，VM、IPC 和物理内存管理仍然比较丰富；不能把它简单等同于只提供地址空间和 endpoint 的极小型内核。

### 11.4 Default Pager、双重分页与保留内存

文件 Pager 不能保证自己总能立即处理写回。例如它可能正在等待磁盘、持有文件系统锁，或者自己的线程和缓冲区也需要分页。如果 Mach 在这时把物理页完全交给它，回收路径就可能因为 Pager 卡住而耗尽内存。

Default Pager 负责的是另一类对象和保障路径。它通常管理内核创建的临时对象，例如 `vm_allocate` 产生的匿名内容、shadow object 的临时页，以及外部数据管理器没有及时接收的待回收页。它是受信任的系统组件，但并不理解文件名、inode 或目录权限。

Mach 论文描述的 **double paging** 可以按下面的方式理解：

```text
外部 Pager 暂时不能接收脏页
             |
             v
Mach 将待回收内容转移到临时 memory object
             |
             v
Default Pager 将临时对象保存到 swap/store
             |
             v
外部 Pager 恢复后，内核再按对象状态协调副本
```

这不是把文件永久变成交换区对象，而是让内核在外部服务失速时仍有一条保留页面内容的路径。Default Pager 的接口仍然是 memory object 协议，因此它也可以使用磁盘、网络或组合存储；在 Hurd 体系中，`storeio`/`libstore` 提供了把交换数据送到具体 store 的用户态层。

分页本身还需要一小块不会被回收的资源：内核必须有内存发送 pageout 消息，Default Pager 也要有线程栈和缓冲区处理请求。这就是 Mach 论文强调的 reserved memory pool 和启动阶段 wired memory 的原因。它们不是性能优化，而是避免“为了回收内存却需要先分配内存”的死锁条件。

Default Pager 不能替代文件 Pager：前者保存临时内容，后者解释文件偏移、文件大小、一致性和持久化语义。二者可以遵守相同的底层协议，却拥有完全不同的策略。

### 11.5 共享映射的一致性和文件页尾边界

多个 task 映射同一个 memory object 时，Mach 可以让它们共享 resident page；但“共享物理页”不自动等于“文件语义正确”。Pager 和文件 server 仍需协调：

- 同一对象的映射是否允许写入；
- 脏页何时对其他映射可见；
- `memory_object_lock_request` 是否需要先清理或刷新页面；
- 截断、删除和最后一个映射释放时，哪些页面还可以写回；
- 后端 I/O 失败时，其他映射收到错误还是继续使用已有缓存。

`MAP_SHARED` 通常要求修改按共享映射语义传播到同一对象和后端；`MAP_PRIVATE` 则通过 shadow/COW 把第一次写入隔离出来。真正的实现仍取决于 translator 的 `io_map` 和 Mach copy strategy，不能只根据 POSIX 标志猜测 Pager 会创建什么对象。

还要分开三个边界：虚拟映射长度、memory object 的有效范围和文件的逻辑大小。若文件大小为 5000 字节、页大小为 4096 字节，最后一页在页缓存中可能需要 4096 字节的容器，但其中只有前 904 字节来自文件。按常见 POSIX 文件映射语义，部分页的文件末尾之后应读为零，且这些填充字节的修改不能把文件无意扩展；访问完全落在文件逻辑大小之外的页通常会产生总线错误或实现定义的 fault。目标 Hurd 版本仍需用实际 `mmap`/Pager 代码确认细节。`pager_report_extent` 和文件服务的大小检查正是用来避免把“页对齐长度”误当成“文件长度”。

### 11.6 读缺页失败时的错误分层

一次失败可能发生在不同层次，调试时要先定位层次再判断错误：

| 层次 | 例子 | 结果 |
| --- | --- | --- |
| 文件/存储后端 | 磁盘 I/O 错误、网络断开 | Pager 读取失败，通常向 Mach 报告 `data_error` |
| Pager 协议 | 端口失效、页未对齐、重复完成请求 | IPC 错误或对象终止，可能使映射访问失败 |
| Mach VM | 没有可用权限、地址不在映射中 | `vm_map`/保护错误或线程收到 fault |
| 应用语义 | `MAP_PRIVATE` 写入、文件截断后访问 | 由 COW、越界和 POSIX 映射规则决定 |

`memory_object_data_unavailable` 只表示“这个范围没有由 Pager 提供的数据”，不能拿来掩盖磁盘损坏；对普通用户创建对象，Mach 会按协议提供解锁的零页，对 COW/默认对象则可能从原对象复制或按已初始化范围处理。把所有失败都转成零页会让上层看见静默的数据损失。`memory_object_data_error` 的后果通常是在缺页线程上产生内存异常或映射访问失败，不是自动变成一次普通 `read()` 的 errno；Pager 应保留错误原因与对象偏移，必要时再由 Hurd 文件接口把可诊断错误传给调用者。

---

## 12. 阅读源码时应建立的四张图

### 图 A：对象图

```text
task
 ├─ address map ── map entry ──> memory object
 ├─ threads
 └─ port name space ── send right ──> server/pager port
```

### 图 B：缺页时序图

```text
应用线程 -> Mach: fault
Mach -> pager: memory_object_data_request
pager -> store/device: read(offset)
pager -> Mach: memory_object_data_supply
Mach -> 应用线程: resume
```

### 图 C：Hurd I/O 层次图

```text
POSIX read/mmap
  -> glibc
  -> io/fs RPC
  -> translator
  -> libpager
  -> libstore
  -> device/file/network
```

### 图 D：fork/exec 图

```text
fork: task_create + VM_INHERIT_COPY + shadow object
exec: file server policy + exec server + vm_map(file objects)
```

每阅读一段代码，都问四个问题：

1. 这里保存的是虚拟地址、object offset 还是物理页？
2. 这个 port 是 data port、control port、name port 还是普通 Hurd object port？
3. 这是 Hurd 的文件/RPC 协议，还是 Mach 的 memory_object 协议？
4. 调用失败时，调用者会得到错误、缺页、阻塞，还是一个失效的 capability？

---

## 13. 建议的学习和实验顺序

### 第一阶段：只学 Mach VM 对象

阅读：

1. [GNU Mach Reference Manual：Virtual Memory Interface](../docs/references/gnu-mach-reference-manual.pdf) 第 5 章；
2. 同一手册的第 6 章 External Memory Management；
3. [Mach concepts](../docs/sources/gnu-hurd/microkernel/mach/concepts.mdwn)；
4. [Mach VM 论文](../docs/papers/mach-MIvmm.ps)。

练习：画出一个 `vm_map` 的参数，并说明每个参数影响的是地址、对象、保护还是继承。

### 第二阶段：手算 COW

画出父 task、子 task、shadow object 和 resident page 的状态。分别模拟：

- 父读、子读；
- 子写；
- 父写；
- 两次连续 fork 后子孙写入同一页。

### 第三阶段：读 Hurd 的 pager 路径

阅读：

- [Hurd `io_path`](../docs/sources/gnu-hurd/hurd/io_path.mdwn)；
- [Hurd `libpager`](../docs/sources/gnu-hurd/hurd/libpager.mdwn)；
- [Hurd bootstrap](../docs/sources/gnu-hurd/hurd/bootstrap.mdwn) 中 ext2fs 创建 pager 的部分；
- [Hurd Workshop 的 store abstraction](../docs/sources/gnu-hurd/hurd-talk.html) 中 `libstore` 部分。

目标是把下面每个名字定位到一层：`pager_memcpy`、`memory_object_data_request`、`pager_read_page`、`store_read`、`device_read`。

### 第四阶段：追踪用户可见协议

阅读：

- [Hurd RPC 文档](../docs/sources/gnu-hurd/hurd/rpc.mdwn)；
- [Hurd hacking guide](../docs/references/hurd-hacking-guide.txt) 中 MIG、`io` 和 translator 示例；
- [Hurd architecture overview](../docs/sources/gnu-hurd/overview.mdwn)。

重点区分 `io_read` 和 `io_map`：一个是显式字节传输，一个是返回 memory object 供 Mach 映射。

### 第五阶段：在真实 Hurd 上观察

在 Debian GNU/Hurd 或 QEMU 环境中，可以尝试：

- 用 `showtrans`/`fsysopts` 查看文件节点上的 translator；
- 用 `rpctrace` 观察 `dir_lookup`、`io_read` 和其他 RPC；
- 用 `vm_region` 类工具检查 task 的映射和 object name；
- 比较小文件 `read`、文件 `mmap`、`fork` 后写入的 page fault/COW 统计；
- 追踪 ext2fs pager 崩溃或后端 I/O 错误时客户端的行为。

本项目的 [POSIX shared-memory 实验](../docs/labs/posix/03_notification_shared_memory.c) 可以帮助理解“通知”和“共享数据”分离，但它只是类比模型，不是 Mach external pager 的实现。

---

## 14. 最重要的结论

### Mach 提供的是机制

Mach 对上暴露的核心 VM 能力是：

- task 级虚拟地址空间；
- memory object 映射；
- 页保护和继承；
- task 间读写、复制和 COW；
- 缺页和页缓存；
- external pager 的 memory_object RPC；
- 物理页回收、写回和统计；同时提供 `vm_set_default_memory_manager` 及 `memory_object_create` 这样的默认 memory manager 接口。具体的 Default Pager（例如 `mach-defpager`）属于用户态服务，而不是 Mach 内核本身。

### Hurd 提供的是对象语义和策略

Hurd 把文件、设备、网络资源和虚拟对象做成 server/translator，使用：

- `io`/`fs`/`fsys`/`exec` 等 Hurd RPC 面向用户和其他服务；
- `libpager` 把对象接到 Mach external pager；
- `libdiskfs`/`libnetfs`/`libtrivfs` 实现不同类型的对象 server；
- `libstore` 把具体 backing store 组合起来。

### 一句话心智模型

> **Mach 负责“哪个 task 的哪个虚拟页现在映射到哪一页物理内存、发生 fault 时如何与 pager 对话”；Hurd 负责“这个 memory object 的 offset 代表哪个文件/设备/网络对象，以及应该如何读取、缓存、授权和写回”。**

---

## 15. 资料索引

本教程使用的本地资料：

- [GNU Mach Reference Manual](../docs/references/gnu-mach-reference-manual.pdf)
- [Machine-Independent Virtual Memory Management](../docs/papers/mach-MIvmm.ps)
- [The Duality of Memory and Communication](../docs/papers/mach-dual-mem-com.ps)
- [Mach Kernel Principles](../docs/papers/mach-kernel-principles-1992.ps)
- [Hurd I/O path](../docs/sources/gnu-hurd/hurd/io_path.mdwn)
- [Hurd RPC](../docs/sources/gnu-hurd/hurd/rpc.mdwn)
- [Hurd libpager](../docs/sources/gnu-hurd/hurd/libpager.mdwn)
- [Hurd bootstrap](../docs/sources/gnu-hurd/hurd/bootstrap.mdwn)
- [Hurd overview](../docs/sources/gnu-hurd/overview.mdwn)
- [Hurd Hacking Guide](../docs/references/hurd-hacking-guide.txt)
- [Hurd architecture article](../docs/sources/gnu-hurd/hurd-paper.html)

原始项目入口和论文 URL 也记录在 [references.bib](../docs/references.bib) 与 [SOURCES.md](../docs/SOURCES.md) 中。

### 版本和实现边界

本文没有把历史 Mach 论文中的所有内部数据结构当成 GNU Mach 当前源码的逐字段 ABI。特别是：

- GNU Mach Reference Manual 记录的是 1.3.99 时代接口；
- Hurd 的 MIG `.defs` 文件是确认具体 RPC 签名的最终来源；
- Mach 论文中常见的 `pager_data_request`、`pager_data_provided` 等旧命名，和 GNU Mach 手册中的 `memory_object_*` 命名表达相近的角色，但不能直接当作当前 GNU Mach 的符号名称；
- `memory_object_data_write` 与 `memory_object_data_return` 等名称在不同 Mach 版本中并存；
- `memory_object_set_attributes` 是历史接口，新的实现通常使用 `memory_object_ready` 与 `memory_object_change_attributes`；
- `io_map` 是否高效、是否支持完整共享映射，取决于具体 translator；
- 默认 pager、设备驱动和 64 位/rump 方案仍可能随 GNU/Hurd 版本变化。

阅读本文时还应记住几条审校边界：

- 图中的“客户端调用 `vm_map`”和“Mach 向 Pager 发一条消息”是逻辑步骤，实际执行还包含 MIG stub、`mach_msg`、序列号分派、reply port 和 port-right 转移。
- `MAP_SHARED`、`MAP_PRIVATE`、文件大小和截断是 POSIX/Hurd 文件接口语义；Mach 只提供 memory object、保护、COW 和缓存管理机制，不能单独决定完整的文件语义。
- `io_map` 不是所有 translator 都实现的强制接口；若它缺失，服务仍可以通过 `io_read` 等 RPC 工作，只是不能获得同样的按页映射路径。
- “Pager 负责写回”与“Mach 决定何时回收”必须同时成立。调试写回问题时，要分别追踪 `memory_object_lock_request`、`memory_object_data_return` 和后端真正完成写入的时刻。

学习时应先使用本文的对象关系和时序模型，再对照正在运行的系统源码确认具体实现。
