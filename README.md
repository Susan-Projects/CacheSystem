# Cache System项目结构与学习记录

本项目基于《代码随想录》中关于 LRU 算法的教学思路，进一步规范为完整的 C++ 项目结构，并逐步从基础实现过渡到现代模板化设计。

This is a thread-safe LRU cache implemented in C++.  
Includes basic functional test and multithread test.


### 第一步：BasicLRU 实现（非模板）
- 学习目标：

	•	掌握最基础的 LRU 算法逻辑

	•   学会分离 .h 和 .cpp 文件

	•	理解项目编译与测试流程

- 相关文件：

	•	include/BasicLRU.h

	•	src/BasicLRU.cpp

	•	test/test_cpp

编译命令：g++ -std=c++17 -pthread src/BasicLRU.cpp test/test_cpp -o build/test_basic_lru
