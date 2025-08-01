# Cache System项目结构与学习记录

本项目基于《代码随想录》中 *Cache System* 算法的教学思路，逐步构建出具备模块化、模板化、线程安全的现代 C++ 缓存系统。

This is a Cache System algorithm implementation document in C++. Starting from the BasicLru algorithm in version 0, a modern C++ caching system with modularization, templating, and thread safety has been gradually constructed. At the same time establishing a targeted performance testing system.


### 第一步：BasicLRU 实现（v0.1.0）
**学习目标：**
-掌握最基础的 LRU 算法逻辑
-学会分离 .h 和 .cpp 文件
-理解项目编译与测试流程
**包含文件：**
-include/BasicLRU.h
-src/BasicLRU.cpp
-test/test_cpp
**编译命令：**
请先创建 `build/` 文件夹（如不存在）：mkdir -p build
```bash
g++ -std=c++17 -pthread src/BasicLRU.cpp test/test_cpp -o build/test_basic_lru
./build/test_basic_lru
```

### KLruCache_v1.0.0（相较于 BasicLru）

该版本基于 BasicLRU，进行了模块重构、模板泛化和线程安全增强。

This version is based on BasicLRU and has undergone module refactoring, template generalization, and thread safety enhancements.

####  1. 模板支持（Generic）
- 支持任意 `Key` / `Value` 类型，例如 `int → std::string`。
- 可灵活拓展为 `KLruCache<std::string, MyObject>` 等任意组合。

####  2. 智能指针管理
- 使用 `std::shared_ptr` 和 `std::weak_ptr` 管理 LRU 双向链表节点，避免内存泄漏和循环引用。

####  3. 多线程支持
- 加入 `std::mutex` 锁，确保 `put()` / `get()` / `remove()` 接口的线程安全。
- 通过多线程测试验证，读写操作无数据竞争、无崩溃。

####  4. 抽象接口分离（KICachePolicy）
- 实现了 `KICachePolicy<Key, Value>` 抽象接口，便于未来支持 LFU/FIFO 等策略的多态切换。

####  5. 更清晰的模块划分
- `LruNode.h`：节点类，封装键值、访问计数和链表指针。
- `KICachePolicy.h`：策略接口，定义缓存操作抽象接口。
- `KLruCache_v1.h/.hpp`：核心类实现，完整封装 LRU 逻辑。
- `test/test_KLruCache.cpp`：测试代码，覆盖单线程和多线程场景。

#### 编译方法
```bash
g++ -std=c++17 -pthread test/test_KLruCache.cpp -Iinclude -o build/test_lru
./build/test_lru
```

### KLruKCache_v1.1.0（基于访问历史的 LRU-K 策略）
KLruKCache 在原有 LRU 算法基础上增加了“历史访问队列”，有效避免“冷数据污染主缓存”的问题。

#### 原理简述：
- 设置两个缓存队列：
1. 访问历史队列（historyList）：记录每个 Key 的访问次数。
2. 主缓存队列（主 LRU）：仅缓存访问次数达到 k 次以上的热点数据。

- 每次访问时：
1. 若已进入主缓存：直接命中。
2. 若访问次数未满 k：只更新历史计数。
3. 若访问次数达到 k 且存在旧值：加入主缓存。

#### 编译方法
```bash
mkdir -p build
g++ -std=c++17 -pthread test/test_KLruKCache.cpp -Iinclude -o build/test_lruk
./build/test_lruk
```
### KHashLruCache_v1.2.0（LRU分片增强版本）

该版本在 KLruCache 的基础上，新增了支持**高并发场景的**Hash**分片**LRU缓存结构。具备以下特点：
1. 高并发支持（Shard-based）
将整个缓存按 key 的 hash 值划分为多个分片（Shard），每个分片是一个独立的 KLruCache 实例。每个分片拥有自己的一把锁，最大限度减小线程争用，提升并发性能。
2. 分片粒度可控
分片数量由构造函数传入（sliceNum），也可默认使用系统硬件线程数自动分片。解决了KLruCache代码中由于锁的粒度很大，导致在每次读写操作之前都有加锁操作，完成读写操作之后还有解锁操作。在低QPS下，锁的竞争的耗时基本可以忽略；但在高并发的情况下，大量的时间消耗在等待锁的操作上，导致耗时增长。
3. 完全复用已有缓存结构
每个分片是标准的 KICachePolicy 派生对象，因此可以支持 KLruCache、KLruKCache 甚至未来的 LFUCache。
4. 模块说明
KHashLruCache.h：Hash 分片核心实现；
分片内部调用：std::hash<Key> → index → shard vector → get()/put()；
封装设计可作为通用“并发缓存容器”。
5. 设计模式：装饰器模式 + 组合模式。

**‼️值得注意的点：**
- LRU（Least Recently Used）：每次淘汰「全局最久未使用的那个元素」

全局策略意味着整个缓存空间是统一的：所有元素共享一个淘汰优先级列表。
- 分片后不再是“全局”最旧/最少

在引入分片（Sharding）之后，每一个 shard（分片）都维护一个独立的 LRU 缓存。
也就是说只在该shared内部淘汰而不是全局访问。
- 但这是可以接受的

因为性能权衡上是值得的：
通过分片，每个 shard 上的加锁操作是独立的，大大提升了并发性能；而淘汰“局部最旧/最少”也是一个合理的近似策略：每个 shard 自治淘汰本地的最劣节点。
