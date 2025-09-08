#pragma once
#include <memory>
#include <unordered_map>
#include <list>
#include <mutex>
#include "CachePolicy.h"

/*  ArcCache_standard.h
    目标是 解决工作集切换 + 扫描型污染问题。
	LRU 容易被顺序访问的冷数据“污染”，ARC 通过 T1/T2 动态调节空间避免这种情况。
	重点：抗扫描能力强，适应性强。
*/ 

namespace CacheSystem {

template<typename Key, typename Value>
class ArcCache : public CachePolicy<Key, Value> {
public:
    explicit ArcCache(int capacity)
        : capacity_(capacity), p_(0) { init(); }

    ~ArcCache() override = default;

    void put(Key key, Value value) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (capacity_ <= 0) return;

        //hit T1: T1->T2
        auto itT1 = t1Map_.find(key);
        if (itT1 != t1Map_.end()) {
            itT1->second->value = std::move(value);
            //move将value以形参的形式存进缓存中，避免不必要的拷贝
            moveT1toT2(itT1->second);
            return;
        }
        //hit T2: only need to refresh
        auto itT2 = t2Map_.find(key);
        if (itT2 != t2Map_.end()) {
            itT2->second->value = std::move(value);
            moveToT2MRU(itT2->second);//更新到LRU链表表头
            return;
        }
        //自适应关键：命中 ghost list
        //hit B1: 说明“最近性/扫描”这类流量在当前 workload 中更重要，应该扩大 T1 的份额
        auto itB1 = b1Map_.find(key);
        if (itB1 != b1Map_.end()) {
            p_ = std::min(
                p_ + std::max(1, (int)b2List_.size() / (int)std::max<size_t>(1, b1List_.size())),
                capacity_);
            //用｜B2｜/｜B1｜作为步长的权重，更快的向T1偏，min(1,)表示至少+1；
            replace(key);
            //replace 的逻辑就是保证 T1+T2<=capacity
            b1List_.erase(itB1->second);
            b1Map_.erase(itB1);
            insertToT2(key, std::move(value));
            return;
        }
        //hit B2 同理
        auto itB2 = b2Map_.find(key);
        if (itB2 != b2Map_.end()) {
            p_ = std::max(p_ - std::max(1, (int)b1List_.size() / (int)std::max<size_t>(1, b2List_.size())), 0);
            replace(key);
            b2List_.erase(itB2->second);
            b2Map_.erase(itB2);
            insertToT2(key, std::move(value));
            return;
        }
        //都没有命中的情况下先进行长度约束
        if ((int)(t1Map_.size() + b1Map_.size()) == capacity_) {
        //用 |T1|+|B1| ≤ C 这个“影子额度”来防幽灵无限膨胀，保证反馈窗口大小有界。
            if ((int)t1Map_.size() < capacity_) { //B1更多
                evictGhostTail(b1List_, b1Map_);
                replace(key);
            } else {                              //T1.size()==capacity
                evictT1toB1();
            }
        } else {
            int total = (int)(t1Map_.size() + t2Map_.size() + b1Map_.size() + b2Map_.size());
            if (total >= 2 * capacity_) {
                evictGhostTail(b2List_, b2Map_);
                //整体接近 2C 时，通常是长期侧的体量（T2+B2）更大，所以从 B2 开始收缩
            }
        }
        //都没有命中则插入T1
        insertToT1(key, std::move(value));
    }

    bool get(Key key, Value& value) override {
        std::lock_guard<std::mutex> lk(mu_);
        auto itT1 = t1Map_.find(key);
        if (itT1 != t1Map_.end()) {
            value = itT1->second->value;
            moveT1toT2(itT1->second);
            return true;
        }
        auto itT2 = t2Map_.find(key);
        if (itT2 != t2Map_.end()) {
            value = itT2->second->value;
            moveToT2MRU(itT2->second);
            return true;
        }

        auto itB1 = b1Map_.find(key);
        if (itB1 != b1Map_.end()) {
            p_ = std::min(p_ + std::max(1, (int)b2List_.size() / (int)std::max<size_t>(1, b1List_.size())), capacity_);
            replace(key);
            b1List_.erase(itB1->second);
            b1Map_.erase(itB1);
            return false;
        }
        auto itB2 = b2Map_.find(key);
        if (itB2 != b2Map_.end()) {
            p_ = std::max(p_ - std::max(1, (int)b1List_.size() / (int)std::max<size_t>(1, b2List_.size())), 0);
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
    struct Node {
        Key key{};
        Value value{};
        std::weak_ptr<Node> prev_;
        std::shared_ptr<Node> next_;
        Node() = default;
        Node(const Key& k, const Value& v) : key(k), value(v) {}
        Node(Key&& k, Value&& v) : key(std::move(k)), value(std::move(v)) {}
    };
    using NodePtr = std::shared_ptr<Node>;

    void init() { makeDummy(t1Head_, t1Tail_); makeDummy(t2Head_, t2Tail_); }

    void makeDummy(NodePtr& head, NodePtr& tail) {
        head = std::make_shared<Node>();
        tail = std::make_shared<Node>();
        head->next_ = tail;
        tail->prev_ = head;
    }

    void insertAfter(const NodePtr& prev, const NodePtr& node) {
        node->next_ = prev->next_;
        node->prev_ = prev;
        prev->next_->prev_ = node;
        prev->next_ = node;
    }

    void removeNode(const NodePtr& node) {
        if (node->prev_.expired() || !node->next_) return;
        auto p = node->prev_.lock();
        p->next_ = node->next_;
        node->next_->prev_ = p;
        node->prev_.reset();
        node->next_.reset();
    }

    void moveToHead(const NodePtr& head, const NodePtr& node) {
        removeNode(node);
        insertAfter(head, node);
    }

    void insertToT1(const Key& key, Value&& v) {
        auto n = std::make_shared<Node>(key, std::move(v));
        insertAfter(t1Head_, n);
        t1Map_[key] = n;
    }

    void insertToT2(const Key& key, Value&& v) {
        auto n = std::make_shared<Node>(key, std::move(v));
        insertAfter(t2Head_, n);
        t2Map_[key] = n;
    }

    void moveT1toT2(const NodePtr& n) {
        auto key = n->key;
        removeNode(n);
        t1Map_.erase(key);
        insertAfter(t2Head_, n);
        t2Map_[key] = n;
    }

    void moveToT2MRU(const NodePtr& n) { moveToHead(t2Head_, n); }

    void evictT1toB1() {
        auto victim = t1Tail_->prev_.lock();
        if (!victim || victim == t1Head_) return;
        Key k = victim->key;
        removeNode(victim);
        t1Map_.erase(k);
        b1List_.push_front(k);
        b1Map_[k] = b1List_.begin();
        if ((int)b1Map_.size() > capacity_) evictGhostTail(b1List_, b1Map_);
    }

    void evictT2toB2() {
        auto victim = t2Tail_->prev_.lock();
        if (!victim || victim == t2Head_) return;
        Key k = victim->key;
        removeNode(victim);
        t2Map_.erase(k);
        b2List_.push_front(k);
        b2Map_[k] = b2List_.begin();
        if ((int)b2Map_.size() > capacity_) evictGhostTail(b2List_, b2Map_);
    }

    void evictGhostTail(std::list<Key>& L, std::unordered_map<Key, typename std::list<Key>::iterator>& M) {
        if (L.empty()) return;
        Key k = L.back();
        L.pop_back();
        M.erase(k);
    }

    void replace(const Key& x) {
        if (!t1Map_.empty() //T1非空则可以赶人
        && ( (b2Map_.count(x) && (int)t1Map_.size() == p_) //同时满足：x来自B2， T1==p_ 的配额
           ||(int)t1Map_.size() > p_)) { //或T1超过了p_的配额；
            evictT1toB1();
        } else {            
            evictT2toB2();
        }
    }

private:
    int capacity_;
    int p_;
    std::mutex mu_;

    NodePtr t1Head_, t1Tail_;
    NodePtr t2Head_, t2Tail_;
    std::unordered_map<Key, NodePtr> t1Map_;
    std::unordered_map<Key, NodePtr> t2Map_;
    //t1Map: 最近访问过的真实缓存，LRU队列
    //t2Map: 被二次访问，短期热点，LRU队列

    std::list<Key> b1List_, b2List_;
    //ghost list,只保存key
    std::unordered_map<Key, typename std::list<Key>::iterator> b1Map_, b2Map_;
    //维护一个哈希 ghost，方便O(1)查找
};

} // namespace CacheSystem