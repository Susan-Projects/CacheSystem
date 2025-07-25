#pragma once

#include<memory>

namespace KamaCache {


template<typename Key, typename Value> 
class KLruCache;
//前项声明B，在 A 类中写了 friend class B;，那么 B 就可以访问 A 的 private 和 protected 成员，无论 B 是否已经完整声明或定义。
//friend 是单向授权

template<typename Key, typename Value>
class LruNode{ //节点类，被KLruCache使用
private:
    Key key_;
    Value value_;
    size_t accessCount_;  //unsigned int
    //weak_ptr & shared_ptr 智能指针
    std::weak_ptr<LruNode<Key, Value>> prev_;
    std::shared_ptr<LruNode<Key, Value>> next_;

public:
    //构造函数高效写法：
    LruNode(const Key& key, const Value& value)
    : key_(key)
    , value_(value)
    , accessCount_(1)
    {}

    //内敛函数定义 inline function definition, 处理单个节点的函数
    //等价于：
    //Key getKey() const; //函数声明
    //Key LruNode::getKey() const { return key_; } //函数定义，只有一条return
    Key getKey() const { return key_; }
    Value getValue() const { return value_; }
    void setValue(const Value& value) { value_ = value; }
    size_t getAccessCount() const { return accessCount_; }
    void incrementAccessCount() { ++accessCount_; }
    
    //friend类，表示KluCache 可以使用 LruNode中的private变量
    friend class KLruCache<Key, Value>;
};

}//namespace KamaCache