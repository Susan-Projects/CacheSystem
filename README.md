# Cache System项目结构与学习记录

本项目基于《代码随想录》中关于 LRU 算法的教学思路，进一步规范为完整的 C++ 项目结构，并逐步从基础实现过渡到现代模板化设计。

This is a thread-safe LRU cache implemented in C++.  
Includes basic functional test and multithread test.


### 第一步：BasicLRU 实现（v0.1.0）
- 学习目标：

	•	掌握最基础的 LRU 算法逻辑

	•   学会分离 .h 和 .cpp 文件

	•	理解项目编译与测试流程

- 相关文件：

	•	include/BasicLRU.h

	•	src/BasicLRU.cpp

	•	test/test_cpp

编译命令：g++ -std=c++17 -pthread src/BasicLRU.cpp test/test_cpp -o build/test_basic_lru

### KLruCache_v1.0.0（相较于 BasicLru）

该版本是基于**知识星球** *缓存系统基础LRU代码* 以及基础版本 `BasicLru` 的增强实现，具备以下显著升级点：

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

#### 🛠️ 编译方法

请先创建 `build/` 文件夹（如不存在）：mkdir -p build

g++ -std=c++17 -pthread test/test_KLruCache.cpp -Iinclude -o build/test_lru

./build/test_lru
