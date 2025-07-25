#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include "LruNode.h"
#include "KICachePolicy.h"


namespace KamaCache {

template<typename Key, typename Value>
class KLruCache: public KICachePolicy<Key, Value> //子类继承抽象父类，实现具体功能
{
public:
    //using 简化类型命名
    using LruNodeType = LruNode<Key, Value>; //Cache Node type
    using NodePtr = std::shared_ptr<LruNodeType>; //当前节点的share_ptr智能指针
    using NodeMap = std::unordered_map<Key, NodePtr>; //哈希表

    explicit KLruCache(int capacity);
    ~KLruCache()    override = default;

    void put(Key key, Value value) override;
    bool get(Key key, Value& value) override;
    Value get(Key key) override;
    void remove(Key key);

private:
    void initializeList();
    void updateExistingNode(NodePtr node, const Value& value);
    void addNewNode(const Key& key, const Value& value);
    void moveToMostRecent(NodePtr node);
    void removeNode(NodePtr node);
    void insertNode(NodePtr node);
    void evictLeastRecent();

private:
    int             capacity_;
    NodeMap         nodeMap_;
    std::mutex      mutex_;
    NodePtr         dummyHead_;
    NodePtr         dummyTail_;
};


}//kama namespace

#include "../src/KLruCache_v1_impl.hpp" 
//typename 模版类必须包含hpp实现文件
//不能用cpp实现，因为编译器无法识别typename具体是int还是什么类型
//最后不需要编译.hpp文件，编译命令：-Iinclude 让编译器额外在 include/路径中查找头文件

