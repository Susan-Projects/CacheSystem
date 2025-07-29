#pragma once

#include "KICachePolicy.h"
#include "KLruCache_v1.h" 
// Or KLruKCache_v1.h if you want to shard LRU-K
#include <vector>
#include <memory>
#include <cmath>
#include <thread>
#include <functional>
#include <mutex>

namespace KamaCache {

template<typename Key, typename Value>
class KHashLruCache: public KICachePolicy<Key, Value>{
public:
    KHashLruCache(size_t totalCapacity, int sliceNum = std::thread::hardware_concurrency())
    //thread::hardware_concurrency()
        : sliceNum_(sliceNum>0 ? sliceNum:1)
        //确保传入的分片数量是合法的
        , capacity_(totalCapacity)
        {
            size_t sliceCap = std::ceil(totalCapacity / static_cast<double>(sliceNum_));
            //static_cast<double> 将整数除法转换为浮点数除法。
            //ceil将总容量 totalCapacity 分配给每个分片
            //ceil(10 / 3.0) = ceil(3.33) = 4 避免一部分出现容量不足的情况

            for(int i=0; i<sliceNum_; i++){
                shards_.emplace_back(std::make_unique<KLruCache<Key, Value>>(sliceCap));
                //std::make_unique<>()： 创建一个堆上的对象，返回 unique_ptr<T>，避免内存泄漏。
                //emplace_back()： 直接构造并添加到 shards_ 向量中（比 push_back 更高效）
            }
        }

    void put(Key key, Value value) override{
        getShared(key)->put(key,value);
    }    
    
    Value get(Key key) override{
        return getShared(key)->get(key);
    }

    bool get(Key key, Value& value) override{
        return getShared(key)->get(key, value);
    }

private:
    size_t getIndex(const Key& key) const{
        return std::hash<Key>{}(key)%sliceNum_;
        //将哈希值映射到 [0, sliceNum_-1] 区间，找到对应分片编号
    }
    KICachePolicy<Key, Value>* getShard(const Key& key) {
        return shards_[getIndex(key)].get();
        //shards_ 是个指针数组，所以我们用 .get() 拿出真正的 KLruCache 对象；
        //返回的是抽象接口指针 KICachePolicy*，可以统一支持不同的缓存实现（比如 LRU/LFU/K-LRU）
    }

    int sliceNum_;
    size_t capacity_;
    std::vector<std::unique_ptr<KICachePolicy<Key, Value>>> shards_;
    //用来存多个 LRU 分片；
    //每个分片都是一个 unique_ptr<KICachePolicy>，也就是说它可以是：
    //KLruCache<Key, Value>
    //KLruKCache<Key, Value>
    //以及其他算法

};

}