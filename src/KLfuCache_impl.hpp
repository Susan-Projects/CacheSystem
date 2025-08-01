#pragma once

#include "../include/KLfuCache.h"
#include "../include/FreqList.h"

namespace KamaCache {

template<typename Key, typename Value>
KLfuCache<Key, Value>::KLfuCache(int capacity)
    :capacity_(capacity), minFreq_(std::numeric_limits<int>::max()) {}
    //minFreq初始化为最大值

template<typename Key, typename Value>
void KLfuCache<Key, Value>::put(Key key, Value value){
    if(capacity_<= 0)  return;
    
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
    auto it = nodeMap_.find(key);
    if(it==nodeMap_.end())  return false;

    getInternal(it->second, value);
    return true;
}

template<typename Key, typename Value>
Value KLfuCache<Key, Value>::get(Key key){
    Value value;
    get(key, value);
    return value;
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::purge(){
    nodeMap_.clear();
    freqListMap_.clear();
    minFreq_=std::numeric_limits<int>::max();
}

//add new node
template<typename Key, typename Value>
void KLfuCache<Key, Value>::putInternal(Key key, Value value){
    if(nodeMap_.size()>=static_cast<size_t>(capacity_)){
        kickOut();
    }

    NodePtr node = std::make_shared<Node>(key, value);
    nodeMap_[key] = node;
    addToFreqList(node);
    minFreq_=1;
}


template<typename Key, typename Value>
void KLfuCache<Key, Value>::getInternal(NodePtr node, Value& value){
    value = node->value;
    removeFromFreqList(node);
    node->freq++;
    addToFreqList(node);

    if (freqListMap_[minFreq_]&&freqListMap_[minFreq_]->isEmpty()) {
        minFreq_++;
    }
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::kickOut(){
    auto& freqList = freqListMap_[minFreq_];
    auto node = freqList->getFirstNode();
    removeFromFreqList(node);
    nodeMap_.erase(node->key);
}

//remove node form the List(freq)
template<typename Key, typename Value>
void KLfuCache<Key, Value>::removeFromFreqList(NodePtr node){
    int freq = node->freq;
    freqListMap_[freq]->removeNode(node);
}

template<typename Key, typename Value>
void KLfuCache<Key, Value>::addToFreqList(NodePtr node){
    int freq = node->freq;
    if(freqListMap_.find(freq)==freqListMap_.end()){
        freqListMap_[freq]=std::make_unique<FreqList<Key, Value>>(freq);
    }
    freqListMap_[freq]->addNode(node);
}
}