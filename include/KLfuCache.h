#pragma once

#include <unordered_map>
#include <memory>
#include <limits>
#include <mutex>
#include "FreqList.h"
#include "KICachePolicy.h"

namespace KamaCache {

template<typename Key, typename Value>
class FreqList;

template<typename Key, typename Value>
class KLfuCache: public KICachePolicy<Key, Value>{
public:
    KLfuCache(int capacity, int maxAverageNum_=10);
    ~KLfuCache() override = default;

    void put(Key key, Value value) override;
    bool get(Key key, Value& value) override;
    Value get(Key key) override;
    void purge();//clear all

public:
    struct Node{
        Key key;
        Value value;
        int freq;
        std::weak_ptr<Node> prev_;
        std::shared_ptr<Node> next_;

        Node(): freq(1), next_(nullptr){}

        Node(const Key& k, Value& v)
            : key(k), value(v), freq(1), next_(nullptr) {}
    };

    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
    //key → NodePtr 映射关系，便于快速定位缓存项的位置、值和访问频率。

private:
    void getInternal(NodePtr node, Value& value);
    void putInternal(Key key, Value value);
    void kickOut();
    void removeFromFreqList(NodePtr node);
    void addToFreqList(NodePtr node);

/* -------- updata v2.1.0 ------------ */
    void addFreqNum();              // 更新总访问次数与平均访问次数
    void decreaseFreqNum(int num);  // 减去频次
    void handleOverMaxAverage();    // 平均访问次数超限处理
    void updateMinFreq();           // 更新最小频次
/* -------- end v2.1.0 --------------- */

private:
/* --------- v2.1.0 ---------- */
    int maxAverageNum_;
    int curAverageNum_;
    int curTotalNum_;
/* --------- v2.0.1 ---------- */
    mutable std::mutex mutex_;
/* --------- v2.0.0 ---------- */
    int capacity_;
    int minFreq_;
    NodeMap nodeMap_;
    std::unordered_map<int, std::unique_ptr<FreqList<Key, Value>>> freqListMap_;
    //维护一个“访问频率到对应频率链表”的映射
    //访问频率：int
    //对应频率链表：std::unique_ptr<FreqList<Key, Value>>，表示一个指向该频率的节点列表的智能指针
    //FreqList<Key, Value> 是一个链表类，它维护了所有具有同一访问频率的缓存节点（Node）
    
};


}

#include "../src/KLfuCache_impl.hpp"

