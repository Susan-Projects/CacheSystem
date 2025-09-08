#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"


namespace CacheSystem {

template<typename Key, typename Value> class LfuCache;

template<typename Key, typename Value>
class FreqList{ 
//维护一个“相同访问频率”的所有节点（Node）的双向链表
public:
    using Node = typename LfuCache<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    
private:
    int     freq_;//当前链表表示的访问频率/当前访问频率的链表
    NodePtr head_;
    NodePtr tail_;

    friend class LfuCache<Key, Value>;
public:
    explicit FreqList(int freq): freq_(freq){
        head_ = std::make_shared<Node>();
        tail_ = std::make_shared<Node>();
        head_->next_ = tail_;
        tail_->prev_  = head_;
    }

    bool isEmpty() const{
        return head_->next_ == tail_;
    }

    void addNode(NodePtr node) {
        if (!node || !head_ || !tail_) return;
        node->prev_ = tail_->prev_;
        node->next_ = tail_;
        tail_->prev_.lock()->next_= node;
        //std::weak_ptr<Node> prev_;
        //尝试获取 shared_ptr，若已释放则返回空
        tail_->prev_= node;
    }

    void removeNode(NodePtr node)
    {
        if (!node || !head_ || !tail_) return;
        if (node->prev_.expired() || !node->next_) return;
        //尝试获取 shared_ptr，若已释放则返回空
        //释放返回true

        auto p = node->prev_.lock();
        p->next_ = node->next_;
        node->next_->prev_ = p;
        node->prev_.reset();
        node->next_ = nullptr;
    }

    NodePtr getFirstNode() const { return head_->next_; }
};


template<typename Key, typename Value>
class LfuCache: public CachePolicy<Key, Value>{
public:
    struct Node{
        Key key {};
        Value value {};
        int freq {1};
        std::weak_ptr<Node> prev_;
        std::shared_ptr<Node> next_;
        //智能指针默认构造的时候就是空指针

        Node()=default;

        Node(const Key& k, const Value& v): key(k), value(v), freq(1) {}
        Node(Key&& k, Value&& v): key(std::move(k)), value(std::move(v)), freq(1) {}
    };

    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
    //key → NodePtr 映射关系，便于快速定位缓存项的位置、值和访问频率。

public:
    explicit LfuCache(int capacity);
    ~LfuCache() override = default;

    void put(Key key, Value value) override;
    bool get(Key key, Value& value) override;
    Value get(Key key) override;
    void purge();//clear all

    public:
    void decayAllFreqs(int delta) {
        std::lock_guard<std::mutex> lock(mutex_);          // ← 加锁
        delta = std::max(1, delta);                        // ← 防止 0 衰减
        for (auto& kv : nodeMap_) {
            NodePtr node = kv.second;
            if (!node) continue;

            removeFromFreqListNoLock(node);
            node->freq = std::max(1, node->freq - delta);
            addToFreqListNoLock(node);
        }
        updateMinFreqNoLock();
    }
        
    int size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(nodeMap_.size());
    }

    // 驱逐并返回最少使用的 key
    Key evictOne() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodeMap_.empty()) return Key();
        auto it = freqListMap_.find(minFreq_);
        if (it == freqListMap_.end() || it->second->isEmpty()) {
            updateMinFreqNoLock();
            it = freqListMap_.find(minFreq_);
            if (it == freqListMap_.end() || it->second->isEmpty()) return Key();
        }
        NodePtr victim = it->second->getFirstNode();
        Key k = victim->key;
        it->second->removeNode(victim);
        nodeMap_.erase(k);
        if (it->second->isEmpty()) {
            freqListMap_.erase(it);
            updateMinFreqNoLock();
        }
        return k;
    }

    bool empty() const { return nodeMap_.empty(); }
    size_t size() const { return nodeMap_.size(); }

private:
    void promoteNolock(const NodePtr& node);
    void evictOneNoLock();
    void addToFreqListNoLock(const NodePtr& node);
    void removeFromFreqListNoLock(const NodePtr& node);
    void updateMinFreqNoLock();

private:
    mutable std::mutex mutex_;
    int capacity_;
    int minFreq_;
    NodeMap nodeMap_;
    std::unordered_map<int, std::unique_ptr<FreqList<Key, Value>>> freqListMap_;
    //维护一个“访问频率到对应频率链表”的映射
    //访问频率：int
    //对应频率链表：std::unique_ptr<FreqList<Key, Value>>，表示一个指向该频率的节点列表的智能指针
    //FreqList<Key, Value> 是一个链表类，它维护了所有具有同一访问频率的缓存节点（Node）
    
};

}//namespace
#include "../src/LfuCache_impl.hpp" 