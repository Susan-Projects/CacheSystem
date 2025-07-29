#pragma  once

#include <unordered_map>
#include <memory>
#include "KLruCache_v1.h"

namespace KamaCache {

template<typename Key, typename Value>
class KLruKCache : public KLruCache<Key, Value>{
public:
    KLruKCache(int capacity, int historyCapacity, int K);
    //重写 get put
    Value get(Key key) override;
    void put(Key key, Value value) override;

private:
    int                                     k_; //访问阈值
    std::unique_ptr<KLruCache<Key, size_t>>  historyList_; //历史访问次数队列, save count;
    std::unordered_map<Key, Value>          historyValueMap_; //临时map, save value;
};

}//namespace KamaCache

#include "../src/KLruKCache_v1_impl.hpp" 