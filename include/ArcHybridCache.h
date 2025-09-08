#pragma once

#include <unordered_map>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <algorithm>
#include "CachePolicy.h"
#include "LruCache.h"
#include "LfuCache.h"

/*  LRU+LFU 混合
	目标是 兼顾短期热点（LRU）和长期热点（LFU）。
	LRU 照顾“新近使用”，LFU 照顾“常用但不一定近期”的。
	重点：区分短期 vs 长期热点，LFU 部分能保留长期热门元素
    组合实现
*/

namespace CacheSystem {

template<typename Key, typename Value>
class ArcHybridCache : public CachePolicy<Key, Value> {
public:
    explicit ArcHybridCache(int capacity)
        : capacity_(std::max(0, capacity)), p_(0)
        , t1_(std::make_unique<LruCache<Key, Value>>(capacity))
        , t2_(std::make_unique<LfuCache<Key, Value>>(capacity)) {}
    //实际的capacity通过p来限制，并不是2*capacity

    void put(Key key, Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if(capacity_ <= 0) return;

        Value v{};
        if (t1_->get(key,v)) {
            t1_->remove(key);
            t2_->put(key,std::move(value));
            return;
        }
        // 命中 T2 -> 更新
        if (t2_->get(key,v)) {
            t2_->put(key,std::move(value));
            return;
        }
        // hit B1
        auto itB1 = b1Map_.find(key);
        if(itB1 != b1Map_.end()){
            p_ = std::min(p_ + std::max(1, (int) b2List_.size() / 
                                           (int)std::max<size_t>(1, b1List_.size())),
                          capacity_);
            replace(key);
            b1List_.erase(itB1->second);
            b1Map_.erase(itB1);
            t2_->put(key, std::move(value));
            return;
        }
        auto itB2 = b2Map_.find(key);
        if (itB2 != b2Map_.end()) {
            p_ = std::max(p_ - std::max(1, (int)b1List_.size() /
                                           (int)std::max<size_t>(1, b2List_.size())),
                          0);
            replace(key);
            b2List_.erase(itB2->second);
            b2Map_.erase(itB2);
            t2_->put(key, std::move(value));
            return;
        }

        //Miss
        if ((int)(t1_->size() + b1Map_.size()) == capacity_) {
            if ((int)t1_->size() < capacity_) {
                evictGhostTail(b1List_, b1Map_);
                replace(key);
            } else {
                evictT1toB1();
            }
        } else {
            int total = (int)(t1_->size() + t2_->size() +
                              b1Map_.size() + b2Map_.size());
            if (total >= 2 * capacity_) {
                evictGhostTail(b2List_, b2Map_);
            }
        }
        t1_->put(key, std::move(value));
    }

    bool get(Key key, Value& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        //hit t1
        if (t1->get(key, value)) {
            t1_->remove(key);
            t2->put(key, value);
            return true;
        }
        //hit t2
        if (t2_->get(key, value)) {
            return true;
        }
        //hit B1/B2, 不返回只调整参数
        auto itB1 = b1Map_.find(key);
        if (itB1 != b1Map_.end()) {
            p_ = std::min(p_ + std::max(1, (int)b2List_.size() /
                                          (int)std::max<size_t>(1, b1List_.size())),
                          capacity_);
            replace(key);
            b1List_.erase(itB1->second);
            b1Map_.erase(itB1);
            return false;
        }
        auto itB2 = b2Map_.find(key);
        if (itB2 != b2Map_.end()) {
            p_ = std::max(p_ - std::max(1, (int)b1List_.size() /
                                          (int)std::max<size_t>(1, b2List_.size())),
                          0);
            replace(key);
            b2List_.erase(itB2->second);
            b2Map_.erase(itB2);
            return false;
        }
        return false;
    }

    Value get(Key key) override {
        Value v{};
        (void)get(key, v);
        return v;
    }

private:
    void replace(const Key& x) {
        if (!t1_->empty() &&
            ((b2Map_.count(x) && (int)t1_->size() == p_) ||
             (int)t1_->size() > p_)) {
            evictT1toB1();
        } else {
            evictT2toB2();
        }
    }

    void evictT1toB1() {
        Key victim = t1_->evictOne();
        if (victim == Key()) return;
        b1List_.push_front(victim);
        b1Map_[victim] = b1List_.begin();
    }

    void evictT2toB2() {
        Key victim = t2_->evictOne();
        if (victim == Key()) return;
        b2List_.push_front(victim);
        b2Map_[victim] = b2List_.begin();
    }

    void evictGhostTail(std::list<Key>& ghostList,
                        std::unordered_map<Key, typename std::list<Key>::iterator>& ghostMap) {
        if (ghostList.empty()) return;
        Key victim = ghostList.back();
        ghostMap.erase(victim);
        ghostList.pop_back();
    }


private:
    int capacity_;
    int p_;
    std::mutex mutex_;
    
    std::unique_ptr<LruCache<Key,Value>> t1_;
    std::unique_ptr<LfuCache<Key,Value>> t2_;

    std::list<Key> b1List_, b2List_;
    std::unordered_map<Key, typename std::list<Key>::iterator> b1Map_, b2Map_;
};


}