#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include "../include/KLruCache_v1.h"

using namespace std;
using namespace KamaCache;

void basicFunctionTest() {
    cout << "== KLruCache Basic function testing ==" << endl;
    KLruCache<int, string> cache(2);  // capacity == 2

    cache.put(1, "10");
    cache.put(2, "20");

    cout << cache.get(1) << endl; // 10

    cache.put(3, "30");             // rm 2
    cout << cache.get(2) << endl; // -1
    cout << cache.get(3) << endl; // 30

    cache.put(4, "40");             // rm 1
    cout << cache.get(1) << endl; // -1
    cout << cache.get(3) << endl; // 30
    cout << cache.get(4) << endl; // 40
}

void writer(KamaCache::KLruCache<int, int>& cache, int base) {
    for (int i = 0; i < 100; ++i) {
        cache.put(base + i, base + i + 1000);
    }
}

void reader(KamaCache::KLruCache<int, int>& cache, int base) {
    for (int i = 0; i < 100; ++i) {
        int val = cache.get(base + i);
        if (val != -1) {
            cout << "✅Hit: key=" << base + i << " val=" << val << endl;
        } else {
            cout << "⚠️Miss: key=" << base + i << endl;
        }
    }
}

void multiThreadTest() {
    cout << "== KLruCache Multithreaded testing ==" << endl;
    KLruCache<int, int> cache(200);

    thread t1(writer, ref(cache), 0);
    thread t2(reader, ref(cache), 0);
    thread t3(writer, ref(cache), 100);
    thread t4(reader, ref(cache), 100);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    cout << "✅ Multi-thread test complete!" << endl;
}

int main() {
    basicFunctionTest();
    multiThreadTest();
    return 0;
}