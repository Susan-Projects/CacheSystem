#pragma once
//避免头文件多次include
//等价于 #ifndef ... #define ... #endif

namespace CacheSystem {

template<typename Key, typename Value>
class CachePolicy{
//抽象基类，策略模式，对应实现三种不同算法
public:
    virtual ~CachePolicy() {};
    virtual void put(Key key, Value value) = 0; 
    virtual bool get(Key key, Value& value) = 0;
    virtual Value get(Key key) = 0;
    //纯虚函数(=0) vs 虚函数(virtual)
    //常见bug：基类析构函数一定要设为 virtual
};

} // namespace 防止命名冲突