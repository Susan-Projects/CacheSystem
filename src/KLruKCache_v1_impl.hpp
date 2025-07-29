#pragma once
#include <cstddef>

namespace KamaCache {

template<typename Key, typename Value>
KLruKCache<Key, Value>::KLruKCache(int capacity, int historyCapacity, int k)
    : KLruCache<Key, Value>(capacity)
    , historyList_(std::make_unique<KLruCache<Key, size_t>>(historyCapacity))
    , k_(k)
    {}

template<typename Key, typename Value>
Value KLruKCache<Key, Value>::get(Key key){
    Value value{};  
    bool inMainCache = KLruCache<Key, Value>::get(key, value);
    //update history
    size_t historyCount = historyList_->get(key);
    historyCount++;
    historyList_->put(key, historyCount);

    if(inMainCache){
        return value;
    }

    if(historyCount >= k_){
        auto it = historyValueMap_.find(key);
        if(it != historyValueMap_.end()) {
            Value storedValue = it->second;
            historyList_->remove(key);
            historyValueMap_.erase(it);
            KLruCache<Key, Value>::put(key, storedValue);
            return storedValue;
        }
    }
    return value;
}

template<typename Key, typename Value>
void KLruKCache<Key, Value>::put(Key key, Value value)
{
    Value existingValue{};
    bool inMainCache = KLruCache<Key, Value>::get(key, existingValue);
    
    if (inMainCache) {
        KLruCache<Key, Value>::put(key, value);
        return;
    }

    size_t historyCount = historyList_->get(key);
    historyCount++;
    historyList_->put(key, historyCount);

    historyValueMap_[key] = value;

    if (historyCount >= k_) {
        historyList_->remove(key);
        historyValueMap_.erase(key);
        KLruCache<Key, Value>::put(key, value);
    }
}
}