#pragma once

#include "../include/KLfuCache.h"
#include "../include/FreqList.h"

namespace KamaCache {

template<typename Key, typename Value>
KLfuCache<Key, Value>::KLfuCache(int capacity, int maxAverageNum=10)
    :capacity_(capacity)
    , minFreq_(std::numeric_limits<int>::max()) 
    , maxAverageNum_(maxAverageNum) 
    , curAverageNum_(0)
    , curTotalNum_(0) {}
    //minFreq初始化为最大值

template<typename Key, typename Value>
void KLfuCache<Key, Value>::put(Key key, Value value){
    if(capacity_<= 0)  return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    //锁的粒度较大，全局锁
    //每次put/get都会上锁整个cache
    //nodeMap_ 与 freqListMap_ 是共享资源，没有分片设计

    auto it = nodeMap_.find(key);
    if(it!=nodeMap_.end()){ //find it
        it->second->value = value;
        Value dummy;
        getInternal(it->second, dummy);
        return;
    }
    putInternal(key, value);
}

template<typename Key, typename Value>
bool KLfuCache<Key, Value>::get(Key key, Value& value){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if(it==nodeMap_.end())  return false;

    getInternal(it->second, value);
    return true;
}

template<typename Key, typename Value>
Value KLfuCache<Key, Value>::get(Key key){
    std::lock_guard<std::mutex> lock(mutex_);
    Value value;
    get(key, value);
    return value;
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::purge(){
    nodeMap_.clear();
    freqListMap_.clear();
    //minFreq_=std::numeric_limits<int>::max();
}

//add new node
template<typename Key, typename Value>
void KLfuCache<Key, Value>::putInternal(Key key, Value value){
    if(nodeMap_.size()>=static_cast<size_t>(capacity_)){
        kickOut();
        //淘汰最小频率的第一个节点
        //冷启动问题，新数据很容易被淘汰
        //不适合短期热点
    }

    NodePtr node = std::make_shared<Node>(key, value);
    nodeMap_[key] = node;
    addToFreqList(node);
    addFreqNum();
    minFreq_=1;
}


template<typename Key, typename Value>
void KLfuCache<Key, Value>::getInternal(NodePtr node, Value& value){
    value = node->value;
    removeFromFreqList(node);
    node->freq++;
    //每次访问都会freq++,没有上限
    //会导致：频率过高而无法淘汰，频率链表会无限增长（横向变长）
    //unordered_map<int, FreqListPtr> 中的key增大，管理成本上升
    //达到INT_MAX会溢出
    addToFreqList(node);

    if (freqListMap_[minFreq_]&&freqListMap_[minFreq_]->isEmpty()) {
        minFreq_++;
    }

    addFreqNum();
    // 总访问频次和当前平均访问频次都随之增加
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::kickOut(){
    auto& freqList = freqListMap_[minFreq_];
    auto node = freqList->getFirstNode();
    removeFromFreqList(node);
    nodeMap_.erase(node->key);
    decreaseFreqNum(node->freq);
}

//remove node form the List(freq)
template<typename Key, typename Value>
void KLfuCache<Key, Value>::removeFromFreqList(NodePtr node){
    if(!node)   return;

    int freq = node->freq;
    //remove node 的时候只看freq， 不看时间
    //过时热点数据占据缓存
    freqListMap_[freq]->removeNode(node);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::addToFreqList(NodePtr node){
    if(!node)   return;
    int freq = node->freq;
    if(freqListMap_.find(freq)==freqListMap_.end()){
        freqListMap_[freq]=std::make_unique<FreqList<Key, Value>>(freq);
    }
    freqListMap_[freq]->addNode(node);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::addFreqNum(){
    curTotalNum_++;
    if(nodeMap_.empty())    curAverageNum_=0;
    else    curAverageNum_=curTotalNum_/nodeMap_.size();

    if(curAverageNum_>maxAverageNum_)   handleOverMaxAverage();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::decreaseFreqNum(int num){
    curTotalNum_ -= num;
    if(nodeMap_.empty())    curAverageNum_=0;
    else    curAverageNum_ = curTotalNum_/nodeMap_.size();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::handleOverMaxAverage(){
    if(nodeMap_.empty())    return;

    for(auto it = nodeMap_.begin(); it != nodeMap_.end(); it++){
        if(!it->second) continue;

        NodePtr node = it->second;
        removeFromFreqList(node);
        node->freq -= maxAverageNum_/2;
        if(node->freq < 1)  node->freq=1;

        addToFreqList(node);
    }
    updateMinFreq();
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::updateMinFreq(){
    minFreq_ = INT_MAX;
    for(const auto& pair: freqListMap_){
        if(pair.second && !pair.second->isEmpty()){
            minFreq_=std::min(minFreq_, pair.first);
        }
    }
    if(minFreq_==INT_MAX)   minFreq_=1;
}

}