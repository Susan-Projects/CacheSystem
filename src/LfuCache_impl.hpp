#pragma once

#include <limits>
#include "../include/LfuCache.h"

namespace CacheSystem {

template<typename Key, typename Value>
LfuCache<Key, Value>::LfuCache(int capacity)
    : capacity_(capacity)
    , minFreq_(1) {}

template<typename Key, typename Value>
void LfuCache<Key, Value>::put(Key key, Value value){
    if(capacity_<= 0)  return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    //锁的粒度较大，全局锁
    //每次put/get都会上锁整个cache
    //nodeMap_ 与 freqListMap_ 是共享资源，没有分片设计

    auto it = nodeMap_.find(key);
    if(it!=nodeMap_.end()){ //find it
        it->second->value = value;
        promoteNolock(it->second);
        return;
    }
    if(static_cast<int>(nodeMap_.size()) >= capacity_){
        evictOneNoLock();
    }
    NodePtr node = std::make_shared<Node>(std::move(key), std::move(value));
    nodeMap_[node->key] = node;
    addToFreqListNoLock(node); // 放到 freq=1 的链表
    minFreq_ = 1;
}

template<typename Key, typename Value>
bool LfuCache<Key, Value>::get(Key key, Value& value){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if(it==nodeMap_.end())  return false;
    value = it->second->value;
    promoteNolock(it->second); // 访问一次，频次+1
    return true;
}

template<typename Key, typename Value>
Value LfuCache<Key, Value>::get(Key key){
    Value value{};
    (void)get(key, value);
    return value;
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::purge(){
    std::lock_guard<std::mutex> lock(mutex_);
    nodeMap_.clear();
    freqListMap_.clear();
    minFreq_=1;
    //minFreq_=std::numeric_limits<int>::max();
}


template<typename Key, typename Value>
void LfuCache<Key, Value>::promoteNolock(const NodePtr& node){
    int oldFreq = node->freq;
    removeFromFreqListNoLock(node);
    node->freq = oldFreq+1;
    addToFreqListNoLock(node);

    if (oldFreq == minFreq_) {
        while(!freqListMap_.count(minFreq_) ||
             (freqListMap_[minFreq_] && freqListMap_[minFreq_]->isEmpty())) {
                ++minFreq_;
                if (minFreq_ > node->freq) break;
        }
    }
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::evictOneNoLock(){
    auto itList = freqListMap_.find(minFreq_);
    if(itList == freqListMap_.end() || itList->second->isEmpty()){
        updateMinFreqNoLock();
        itList = freqListMap_.find(minFreq_);
        if (itList == freqListMap_.end() || itList->second->isEmpty()) return;
    }

    NodePtr victim = itList->second->getFirstNode();
    itList->second->removeNode(victim);
    nodeMap_.erase(victim->key);

    if(itList->second->isEmpty()){
        freqListMap_.erase(itList);
        updateMinFreqNoLock();
    }
}


template<typename Key, typename Value>
void LfuCache<Key, Value>::addToFreqListNoLock(const NodePtr& node) {
        int f = node->freq;
        auto it = freqListMap_.find(f);
        if (it == freqListMap_.end()) {
            auto fl = std::make_unique<FreqList<Key, Value>>(f);
            it = freqListMap_.emplace(f, std::move(fl)).first;
        }
        it->second->addNode(node);
    }

template<typename Key, typename Value>
void LfuCache<Key, Value>::removeFromFreqListNoLock(const NodePtr& node) {
    int f = node->freq;
    auto it = freqListMap_.find(f);
    if (it == freqListMap_.end()) return;
    it->second->removeNode(node);
    if (it->second->isEmpty()) {
        freqListMap_.erase(it);
    }
    }

template<typename Key, typename Value>
void LfuCache<Key, Value>::updateMinFreqNoLock() {
    if (nodeMap_.empty()) {
        minFreq_ = 1;
        return;
    }
    int best = std::numeric_limits<int>::max();
    for (auto& kv : freqListMap_) {
        if (kv.second && !kv.second->isEmpty()) {
            if (kv.first < best) best = kv.first;
        }
    }
    if (best == std::numeric_limits<int>::max()) best = 1;
    minFreq_ = best;
}

}