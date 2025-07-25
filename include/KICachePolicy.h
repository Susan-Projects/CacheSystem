#pragma once
//避免头文件多次include
//等价于 #ifndef ... #define ... #endif

namespace KamaCache {

template<typename Key, typename Value>
class KICachePolicy{
public:
    virtual ~KICachePolicy() {};
    virtual void put(Key key, Value value) = 0; //纯虚函数，必须由子类实现
    virtual bool get(Key key, Value& value) = 0;
    virtual Value get(Key key) = 0;
};

} // namespace 防止命名冲突