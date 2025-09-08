#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include "CachePolicy.h"


namespace CacheSystem {


template<typename Key, typename Value> 
class LruCache;
//friend 是单向授权
//在A类中写：friend class B
//B类中可以访问A的 private 和 protected 成员
//但是B需要前向声明

template<typename Key, typename Value>
class LruNode{ 
private:
    Key key_;
    Value value_;
    size_t accessCount_;  //unsigned int
    //weak_ptr & shared_ptr 智能指针防止循环引用
    std::weak_ptr<LruNode<Key, Value>> prev_;
    std::shared_ptr<LruNode<Key, Value>> next_;

    //friend类，表示KluCache 可以使用 LruNode中的private变量
    friend class LruCache<Key, Value>;

public:
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
};

template<typename Key, typename Value>
class LruCache: public CachePolicy<Key, Value> 
{
public:
    //using 简化类型命名
    using LruNodeType = LruNode<Key, Value>; //Cache Node type
    using NodePtr = std::shared_ptr<LruNodeType>; //当前节点的share_ptr智能指针
    using Map = std::unordered_map<Key, NodePtr>; //哈希表

    explicit LruCache(int capacity);
    ~LruCache() override = default;

    void put(Key key, Value value) override;
    bool get(Key key, Value& value) override;
    Value get(Key key) override; //注意未命中的情况
    void remove(Key key);

        // 驱逐并返回最久未使用的 key
    Key evictOne() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodeMap_.empty()) return Key(); 
        NodePtr victim = dummyHead_->next_;
        Key k = victim->getKey();
        removeNode(victim);
        nodeMap_.erase(k);
        return k;
    }

    bool empty() const { return nodeMap_.empty(); }
    size_t size() const { return nodeMap_.size(); }

private:
    void initializeList();
    void updateExistingNode(NodePtr node, const Value& value);
    void addNewNode(const Key& key, const Value& value);
    void moveToMostRecent(NodePtr node);
    void removeNode(NodePtr node);
    void insertNode(NodePtr node);
    void evictLeastRecent();

private:
    size_t          capacity_;
    Map             nodeMap_;
    std::mutex      mutex_;
    NodePtr         dummyHead_;
    NodePtr         dummyTail_;
};



}//namespace

#include "../src/LruCache_impl.hpp" 
//typename 模版类必须包含hpp实现文件
//不能用cpp实现，因为编译器无法识别typename具体是int还是什么类型
//最后不需要编译.hpp文件，编译命令：-Iinclude 让编译器额外在 include/路径中查找头文件
