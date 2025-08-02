#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "KICachePolicy.h"


namespace KamaCache {

template<typename Key, typename Value> 
class KLfuCache;

template<typename Key, typename Value>
class FreqList{ 
//维护一个“相同访问频率”的所有节点（Node）的双向链表
public:
    using Node = typename KLfuCache<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    
private:
    int freq_;//当前链表表示的访问频率/当前访问频率的链表
    NodePtr head_;
    NodePtr tail_;

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

        auto prev_ = node->prev_.lock();
        prev_->next_ = node->next_;
        node->next_->prev_ = prev_;
        node->next_ = nullptr;
    }

    NodePtr getFirstNode() const { return head_->next_; }

    
    friend class KLfuCache<Key, Value>;
};

}//namespace KamaCache