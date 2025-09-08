#pragma once

#include "CachePolicy.h"
#include "LruCache.h" 
#include <vector>
#include <memory>
#include <cmath>
#include <thread>
#include <functional>
#include <mutex>

namespace CacheSystem {

template<typename Key, typename Value>
class HashLruCache: public CachePolicy<Key, Value>{
public:
    explicit HashLruCache(size_t totalCapacity, int sliceNum = std::thread::hardware_concurrency())
    //thread::hardware_concurrency()
        : sliceNum_(sliceNum>0 ? sliceNum:1)
        //确保传入的分片数量是合法的
        , capacity_(totalCapacity)
        {
            shards_.reserve(sliceNum_);
            //size_t sliceCap = std::ceil(totalCapacity / static_cast<double>(sliceNum_));
            //static_cast<double> 将整数除法转换为浮点数除法。
            //ceil将总容量 totalCapacity 分配给每个分片
            //ceil(10 / 3.0) = ceil(3.33) = 4 避免一部分出现容量不足的情况

            // 公平拆分：前 r 片分配 base+1，其余 base；总和 == totalCapacity
            const size_t base = (sliceNum_ == 0) ? 0 : (totalCapacity / static_cast<size_t>(sliceNum_));
            const size_t rem  = (sliceNum_ == 0) ? 0 : (totalCapacity % static_cast<size_t>(sliceNum_));

            for(int i=0; i<sliceNum_; i++){
                size_t shardCap = base + (static_cast<size_t>(i) < rem ? 1u : 0u);
                shards_.emplace_back(std::make_unique<LruCache<Key, Value>>(static_cast<int>(shardCap)));
                //std::make_unique<>()： 创建一个堆上的对象，返回 unique_ptr<T>，避免内存泄漏。
                //emplace_back()： 直接构造并添加到 shards_ 向量中（比 push_back 更高效）
            }
        }

    void put(Key key, Value value) override{
        getShared(key)->put(std::move(key),std::move(value));
    }    
    
    Value get(Key key) override{
        return getShared(key)->get(std::move(key));
    }

    bool get(Key key, Value& value) override{
        return getShared(key)->get(key, value);
    }

private:
    size_t getIndex(const Key& key) const{
        return std::hash<Key>{}(key)%static_cast<size_t>(sliceNum_);
        //将哈希值映射到 [0, sliceNum_-1] 区间，找到对应分片编号
    }
    CachePolicy<Key, Value>* getShared(const Key& key) {
        return shards_[getIndex(key)].get();
        //shards_ 是个指针数组，所以我们用 .get() 拿出真正的 KLruCache 对象；
        //返回的是抽象接口指针 CachePolicy*，可以统一支持不同的缓存实现（比如 LRU/LFU/K-LRU）
    }

    int     sliceNum_;
    size_t  capacity_;
    std::vector<std::unique_ptr<CachePolicy<Key, Value>>> shards_;
    //用来存多个 LRU 分片；
    //每个分片都是一个 unique_ptr<KICachePolicy>，也就是说它可以是：
    //KLruCache<Key, Value>
    //KLruKCache<Key, Value>
    //以及其他算法
};

}
