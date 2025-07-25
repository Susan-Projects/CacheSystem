#pragma once

#include "KLruCache_v1.h"
namespace KamaCache{

template<typename Key, typename Value>
KLruCache<Key, Value>::KLruCache(int capacity)
    :   capacity_(capacity){
        initializeList();
    }

//add or update cache
template<typename Key, typename Value>
void KLruCache<Key, Value>::put(Key key, Value value){
    if(capacity_<=0)    return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it != nodeMap_.end()) {
        updateExistingNode(it->second, value);
        return;
    }
    addNewNode(key, value);
}

template<typename Key, typename Value>
bool KLruCache<Key, Value>::get(Key key, Value& value){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if(it!=nodeMap_.end()){
        moveToMostRecent(it->second);
        value = it->second->getValue();
        return true;
    }
    return false;
}

template<typename Key, typename Value>
Value KLruCache<Key, Value> ::get(Key key){
    Value value{};
    get(key, value);//内部调用了第一个版本
    return value;
}

template<typename Key, typename Value>
void KLruCache<Key, Value> ::remove(Key key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if(it!=nodeMap_.end()){
        removeNode(it->second);
        nodeMap_.erase(it);
    }
}

//private 
template<typename Key, typename Value>
void KLruCache<Key, Value> ::initializeList(){
    dummyHead_ = std::make_shared<LruNodeType>(Key(),Value());
    dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
    dummyHead_->next_ = dummyTail_;
    dummyTail_->prev_ = dummyHead_;
}

template<typename Key, typename Value>
void KLruCache<Key, Value>::updateExistingNode(NodePtr node, const Value& value){
    node->setValue(value);
    moveToMostRecent(node);
}

//if full, rm the last one, add at the tail
template<typename Key, typename Value>
void KLruCache<Key, Value>::addNewNode(const Key& key, const Value& value){
    if (nodeMap_.size() >= capacity_) {
        evictLeastRecent();//expel the least recent visits
    }
    NodePtr newNode = std::make_shared<LruNodeType>(key, value);
    //std::shared_ptr<LruNodeType> newNode(new LruNodeType(key, value));
    insertNode(newNode);
    nodeMap_[key] = newNode;
}


template<typename Key, typename Value>
void KLruCache<Key, Value>::moveToMostRecent(NodePtr node){
    removeNode(node);
    insertNode(node);
}

//discinnect from the linked list
template<typename Key, typename Value>
void KLruCache<Key, Value>::removeNode(NodePtr node){
    if(!node->prev_.expired() && node->next_){
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;
    }
}

template<typename Key, typename Value>
void KLruCache<Key, Value>::insertNode(NodePtr node) {
    node->next_ = dummyTail_;
    node->prev_ = dummyTail_->prev_;
    dummyTail_->prev_.lock()->next_ = node;
    dummyTail_->prev_ = std::weak_ptr<LruNodeType>(node);
}

template<typename Key, typename Value>
void KLruCache<Key, Value>::evictLeastRecent() {
    NodePtr leastRecent = dummyHead_->next_;
    removeNode(leastRecent);
    nodeMap_.erase(leastRecent->getKey());
}

}
