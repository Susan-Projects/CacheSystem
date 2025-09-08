#pragma once

#include "CachePolicy.h"
#include "LfuCache.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace CacheSystem {

// Aging 装饰器：在 LFU 基础上增加 Aging（频次衰减）
template<typename Key, typename Value>
class AgingLfuCache : public CachePolicy<Key, Value> {
public:
    AgingLfuCache(int capacity, int maxAverageNum)
        : base_(std::make_unique<LfuCache<Key, Value>>(capacity))
        , maxAverageNum_(maxAverageNum)
        , curTotalNum_(0)
        , curAverageNum_(0) {}

    void put(Key key, Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        base_->put(key, value);
        addFreqNum();
    }

    bool get(Key key, Value& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        bool hit = base_->get(key, value);
        if (hit) addFreqNum();
        return hit;
    }

    Value get(Key key) override {
        Value value{};
        (void)get(key, value);
        return value;
    }

    void purge() {
        std::lock_guard<std::mutex> lock(mutex_);
        base_->purge();
        curTotalNum_ = 0;
        curAverageNum_ = 0;
    }

private:
    void addFreqNum() {
        ++curTotalNum_;
        if (curAverageNum_ == 0) {
            curAverageNum_ = 1;
        } else {
            curAverageNum_ = curTotalNum_ / (base_->size() > 0 ? base_->size() : 1);
        }

        if (curAverageNum_ > maxAverageNum_) {
            handleOverMaxAverage();
        }
    }

    void handleOverMaxAverage() {
        // 这里调用底层 LfuCache 的接口，衰减所有节点的 freq
        // 在 LfuCache 里提供一个 public API: decayAllFreqs()
        // 假设 LfuCache 有个 decayAllFreqs()，否则就内联重写。
        base_->decayAllFreqs(maxAverageNum_ / 2);
        curTotalNum_ /= 2; 
        curAverageNum_ /= 2;
    }

private:
    std::unique_ptr<LfuCache<Key, Value>> base_;
    int maxAverageNum_;
    int curTotalNum_;
    int curAverageNum_;
    std::mutex mutex_;
};

} // namespace CacheSystem