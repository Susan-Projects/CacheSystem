#include <iostream>

#include "../include/CachePolicy.h"
//lru
#include "../include/LruCache.h"
#include "../include/LruKDecorator.h"
#include "../include/HashLruCache.h"
//lfu
#include "../include/LfuCache.h"
#include "../include/LfuAgingDecorator.h"
#include "../include/HashLfuCache.h"
//arc
#include "../include/ArcCache.h"
#include "../include/ArcHybridCache.h"

using Key = int;
using Val = int;

struct Op { bool isPut; Key key; Val val; };

// =============== 场景请求序列生成（一次生成，多算法回放，确保公平） ===============
std::vector<Op> gen_hotspot(size_t ops, int hotN, int coldN, int p_hot=70, int p_put=30, unsigned seed=42){
    std::mt19937 g(seed);
    std::vector<Op> v; v.reserve(ops);
    for (size_t i=0;i<ops;++i){
        bool isPut = (g()%100) < p_put;
        bool useHot = (g()%100) < p_hot;
        Key k = useHot ? (g()%hotN) : (hotN + g()%coldN);
        v.push_back({isPut, k, (Val)k});
    }
    return v;
}
std::vector<Op> gen_scan(size_t ops, int loopN, int p_jump=30, int p_out=10, int p_put=20, unsigned seed=7){
    std::mt19937 g(seed); Key cur=0;
    std::vector<Op> v; v.reserve(ops);
    for (size_t i=0;i<ops;++i){
        int r = g()%100; Key k;
        if (r < 100 - p_jump - p_out) { k = cur; cur = (cur+1)%loopN; }
        else if (r < 100 - p_out)     { k = g()%loopN; }
        else                           { k = loopN + g()%loopN; }
        bool isPut = (g()%100) < p_put;
        v.push_back({isPut, k, (Val)k});
    }
    return v;
}
std::vector<Op> gen_bursty(size_t ops, int phases, int universe, int window=200, int p_put=20, unsigned seed=9){
    std::mt19937 g(seed);
    std::vector<Op> v; v.reserve(ops);
    size_t per = ops / std::max(1, phases);
    Key base = 0;
    for (size_t i=0;i<ops;++i){
        if (i%per==0){
            std::uniform_int_distribution<Key> pick(0, std::max(0, universe-window));
            base = pick(g);
        }
        Key k = base + (g()%window);
        bool isPut = (g()%100) < p_put;
        v.push_back({isPut, k, (Val)k});
    }
    return v;
}

// =============== 单实例命中率跑法（预热 + 回放同一序列） ===============
struct HitStats { size_t req=0, hit=0; };
template<class MakePolicy>
HitStats run_hitrate_once(MakePolicy make, const std::vector<Op>& ops, size_t warm, const std::vector<Key>& warm_keys){
    auto cache = make(); HitStats s{};
    Val out{};
    // 预热
    for (auto k : warm_keys) cache->put(k, (Val)k);
    // 回放
    for (size_t i=0;i<ops.size(); ++i){
        const auto& op = ops[i];
        if (i < warm){
            if (op.isPut) cache->put(op.key, op.val);
            else          cache->get(op.key, out);
            continue;
        }
        if (op.isPut) cache->put(op.key, op.val);
        else {
            s.req++;
            if (cache->get(op.key, out)) s.hit++;
        }
    }
    return s;
}

// =============== 命中率总控：一次性跑 LRU / LRU-K / LFU / LFU-Aging / ARC / ARC-H ===============
void run_all_hitrate(){
    const int CAP = 200;                 // 主缓存容量（元素）
    const size_t WARM = 20000;
    // 三个场景的请求序列（生成一次，多算法回放）
    auto ops_hot  = gen_hotspot(200000, /*hot*/200, /*cold*/8000, 70, 30, 123);
    auto ops_scan = gen_scan   (200000, /*loop*/10000, 30, 10, 20, 321);
    auto ops_bst  = gen_bursty (200000, /*phases*/5, /*U*/20000, 300, 20, 777);

    std::vector<Key> warm_keys(1000); std::iota(warm_keys.begin(), warm_keys.end(), 0);

    struct Item { std::string name; std::function<std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>()> make; };
    std::vector<Item> algos = {
        {"LRU",        [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::LruCache<Key,Val>(CAP)); }},
        {"LRU-K(K=2)", [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::LruKDecorator<Key,Val>(CAP, /*history*/ 100000, /*K*/2)); }},
        {"LFU",        [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::LfuCache<Key,Val>(CAP)); }},
        {"LFU-Aging",  [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::LfuAgingDecorator<Key,Val>(CAP, /*maxAvg*/ 5000)); }},
        {"ARC",        [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::ArcCache<Key,Val>(CAP)); }},
        {"ARC-Hybrid", [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::ArcHybridCache<Key,Val>(CAP, /*threshold*/2)); }},
    };

    auto run_block = [&](const std::string& title, const std::vector<Op>& ops){
        std::cout << "\n=== " << title << " ===\n";
        for (auto& a : algos){
            auto st = run_hitrate_once(a.make, ops, WARM, warm_keys);
            double hr = 100.0 * st.hit / std::max<size_t>(1, st.req);
            std::cout << std::left << std::setw(12) << a.name
                      << " hit=" << std::fixed << std::setprecision(2) << hr
                      << "% (" << st.hit << "/" << st.req << ")\n";
        }
    };

    run_block("热点访问（80/20 近似）", ops_hot);
    run_block("循环扫描",             ops_scan);
    run_block("阶段性热点突变",       ops_bst);
}

// =============== 通用 Hash 分片装饰器（用于 QPS/延迟基准，任意算法都能分片） ===============
template<typename KeyT, typename ValT>
class ShardedCache : public CacheSystem::CachePolicy<KeyT, ValT> {
public:
    using Factory = std::function<std::unique_ptr<CacheSystem::CachePolicy<KeyT,ValT>>(size_t shardCap, int shardIdx)>;
    ShardedCache(size_t totalCap, int shards, Factory f)
        : shards_(std::max(1, shards)) {
        size_t per = (totalCap + shards_ - 1) / shards_;
        caches_.reserve(shards_);
        for (int i=0;i<shards_; ++i) caches_.push_back(f(per, i));
    }
    void put(KeyT k, ValT v) override { caches_[idx(k)]->put(k, v); }
    bool get(KeyT k, ValT& v) override { return caches_[idx(k)]->get(k, v); }
    ValT get(KeyT k) override { ValT v{}; (void)get(k, v); return v; }
private:
    size_t idx(const KeyT& k) const { return std::hash<KeyT>{}(k) % shards_; }
    int shards_;
    std::vector<std::unique_ptr<CacheSystem::CachePolicy<KeyT,ValT>>> caches_;
};

// =============== 多线程延迟/QPS 基准（固定时间窗口） ===============
struct LatQps { double avg_us=0; double qps=0; double hit_rate=0; };
template<class MakeCache, class Gen>
LatQps run_qps(MakeCache make, Gen gen, int threads, std::chrono::seconds duration,
               std::chrono::microseconds hitCost = std::chrono::microseconds(2),
               std::chrono::microseconds missCost= std::chrono::microseconds(200))
{
    auto cache = make();                       // 共享实例（分片可减少锁竞争）
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> ops{0}, hits{0};
    std::atomic<uint64_t> total_ns{0};

    auto worker = [&](){
        Val out{};
        auto g = gen(); // 每线程一份生成器（避免锁）
        while (!stop.load(std::memory_order_relaxed)){
            auto begin = std::chrono::high_resolution_clock::now();
            Key k = g();
            bool ok = cache->get(k, out);
            if (!ok) cache->put(k, k);
            auto end = std::chrono::high_resolution_clock::now();
            auto service = ok ? hitCost : missCost;
            auto elapsed = (end - begin) + service;
            ops.fetch_add(1, std::memory_order_relaxed);
            if (ok) hits.fetch_add(1, std::memory_order_relaxed);
            total_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count(),
                               std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> ts;
    for (int i=0;i<threads;++i) ts.emplace_back(worker);
    std::this_thread::sleep_for(duration);
    stop.store(true);
    for (auto& t : ts) t.join();

    LatQps r{};
    uint64_t nops = ops.load(), nhit = hits.load(), nns = total_ns.load();
    r.qps = nops / double(duration.count());
    r.hit_rate = nops ? (100.0 * nhit / nops) : 0.0;
    r.avg_us = nops ? (nns / 1000.0 / nops) : 0.0;
    return r;
}

// 简单生成器：热点 80/20（用于 QPS）
auto make_hot_keygen(size_t universe=100000, double hot_ratio=0.2, double p_hot=0.8, unsigned seed=2025){
    return [=]() mutable {
        std::mt19937 rng(seed ^ std::hash<std::thread::id>{}(std::this_thread::get_id()));
        size_t hotN = std::max<size_t>(1, universe * hot_ratio);
        std::uniform_int_distribution<Key> hot(0, (Key)hotN-1);
        std::uniform_int_distribution<Key> cold((Key)hotN, (Key)universe-1);
        std::bernoulli_distribution pick_hot(p_hot);
        return [=]() mutable { return pick_hot(rng) ? hot(rng) : cold(rng); };
    };
}

// =============== QPS 场景：对比分片 LRU / 分片 LFU / 分片 ARC（通用分片器） ===============
void run_all_qps(){
    const size_t TOTAL_CAP = 100000;   // 总容量
    const int SHARDS = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 8;
    const int T = SHARDS;              // 线程数与分片数一致
    auto keygen = make_hot_keygen(200000, 0.2, 0.8);

    struct Item { std::string name; std::function<std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>()> make; };
    std::vector<Item> items;

    // 1) 你现有的 HashLruCache / HashLfuCache（如果已实现，直接用）
    items.push_back({"Hash LRU",
        [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::HashLruCache<Key,Val>(TOTAL_CAP, SHARDS)); }});
    items.push_back({"Hash LFU",
        [=]{ return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::HashLfuCache<Key,Val>(TOTAL_CAP, SHARDS)); }});

    // 2) 任意算法的通用分片（示例：ARC）
    items.push_back({"Shard ARC",
        [=]{
            using C = ShardedCache<Key,Val>;
            return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(
                new C(TOTAL_CAP, SHARDS,
                      [](size_t cap, int idx){
                          (void)idx;
                          return std::unique_ptr<CacheSystem::CachePolicy<Key,Val>>(new CacheSystem::ArcCache<Key,Val>((int)cap));
                      })
            );
        }});

    std::cout << "\n=== 并发 QPS / 延迟（热点负载, " << T << " 线程, " << SHARDS << " 分片）===\n";
    for (auto& it : items){
        auto r = run_qps(it.make, keygen, T, std::chrono::seconds(10));
        std::cout << std::left << std::setw(10) << it.name
                  << "  hit=" << std::fixed << std::setprecision(2) << r.hit_rate << "% "
                  << " avg=" << r.avg_us << "us "
                  << " QPS=" << r.qps << "\n";
    }
}

int main(){
    // 1) 命中率对比（单实例，三场景）
    run_all_hitrate();

    // 2) 并发延迟/QPS（分片，热点场景）
    run_all_qps();

    return 0;
}