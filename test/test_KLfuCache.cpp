
#include "../src/KLfuCache_impl.hpp"
#include <iostream>
#include <cassert>
using namespace KamaCache;

//基础功能测试
void testBasic() {
    KLfuCache<int, std::string> cache(2);
    cache.put(1, "one");
    cache.put(2, "two");

    assert(cache.get(1) == "one"); //1访问频率+1
    cache.put(3, "three"); //淘汰 key=2（因为访问频率低）

    std::string dummy;
    assert(!cache.get(2, dummy)); // key 2 should be evicted
    assert(cache.get(1) == "one");
    assert(cache.get(3) == "three");
    std::cout << "[PASS] testBasic";
}

//频率淘汰测试
void testEvictionByFrequency() {
    KLfuCache<int, std::string> cache(3);
    cache.put(1, "A"); //freq=1
    cache.put(2, "B"); //freq=1
    cache.put(3, "C"); //freq=1

    cache.get(1); //freq=2
    cache.get(2); cache.get(2); //freq=3

    cache.put(4, "D"); //淘汰3

    std::string dummy;
    assert(!cache.get(3, dummy)); // key 3 should be evicted
    assert(cache.get(1) == "A");
    assert(cache.get(2) == "B");
    assert(cache.get(4) == "D");
    std::cout << "[PASS] testEvictionByFrequency";
}

//更新值测试
void testUpdateValue() {
    KLfuCache<int, std::string> cache(2);
    cache.put(1, "old");
    cache.put(1, "new");

    assert(cache.get(1) == "new");
    std::cout << "[PASS] testUpdateValue";
}

//capacity=0
void testZeroCapacity() {
    KLfuCache<int, int> cache(0);
    cache.put(1, 100);
    int value;
    assert(!cache.get(1, value));
    std::cout << "[PASS] testZeroCapacity";
}

void testPurge() {
    KLfuCache<int, std::string> cache(2);
    cache.put(1, "one");
    cache.put(2, "two");
    cache.purge();

    std::string value;
    assert(!cache.get(1, value));
    assert(!cache.get(2, value));
    std::cout << "[PASS] testPurge";
}

int main() {
    testBasic();
    testEvictionByFrequency();
    testUpdateValue();
    testZeroCapacity();
    testPurge();
    std::cout << "✅ All tests passed successfully.";
    return 0;
}
