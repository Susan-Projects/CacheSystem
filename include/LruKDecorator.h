#pragma  once

#include <unordered_map>
#include <memory>
#include <mutex>
#include "LruCache.h"
#include "CachePolicy.h"

namespace CacheSystem {
//LRU-K算法用装饰器模式实现：
//对外：统一继承CachePolicy抽象接口
//对内：组合LruCache底座
template<typename Key, typename Value>
class LruKDecorator : public CachePolicy<Key, Value> {
public:
    LruKDecorator(int capacity, int historyCapacity, int k)
        //base_组合LruCache实现
        : base_(std::make_unique<LruCache<Key, Value>>(capacity))
        , historyList_(std::make_unique<LruCache<Key,size_t>>(historyCapacity))
        , k_(k)
    {}

    bool get(Key key, Value& value) override{
        std::lock_guard<std::mutex> lock(mutex_);
        //查主缓存
        if(base_->get(key, value)){
            bumpHistoryNoStoreLocked(key);
            return true;
        }
        //not hit
        size_t historyCount = bumpHistoryNoStoreLocked(key);
        //达到阈值
        if(historyCount >= static_cast<size_t>(k_)){
            auto it = staged_.find(key);
            if(it != staged_.end()){
                Value storedValue = std::move(it->second);
                staged_.erase(it);
                historyList_->remove(key);
                base_->put(key, storedValue);
                value = std::move(storedValue);
                return true;
            }
        }
        return false;
    }

    Value get(Key key) override{
        Value value{};
        (void) get(key, value);
        return value;
    }

    void put(Key key, Value value) override{
        std::lock_guard<std::mutex> lock(mutex_);
        //在主缓存
        Value existing{};
        if (base_->get(key, existing)) {
            base_->put(key, std::move(value));
            return;
        }
        //不在主缓存
        size_t historyCount = bumpHistoryNoStoreLocked(key);
        staged_[key] = std::move(value);
        //达到阈值
        if (historyCount >= static_cast<size_t>(k_)) {
            auto it = staged_.find(key);
            Value v = std::move(it->second);
            staged_.erase(it);
            historyList_->remove(key);
            base_->put(key, std::move(v));
        }
    }
private:
    size_t bumpHistoryNoStoreLocked(const Key& key){
        size_t cnt = historyList_->get(key);
        ++cnt;
        historyList_->put(key, cnt);
        return cnt;
    }
    std::mutex                                  mutex_;
    std::unique_ptr<LruCache<Key, Value>>       base_;
    int                                         k_; //访问阈值
    std::unique_ptr<LruCache<Key, size_t>>      historyList_; //历史访问次数队列, save count;
    std::unordered_map<Key, Value>              staged_; //临时map, save value;
};

}//namespace CacheSystem
