#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include "../include/BasicLRU.h"
using namespace std;

//------------------------基本功能测试------------------------------
void basicFunctionTest(){
    cout<<"==基本功能测试=="<<endl;
    BasicLRU cache(2);

    cache.put(1, 10);              // 缓存：{1=10}
    cache.put(2, 20);              // 缓存：{1=10, 2=20}
    cout << cache.get(1) << endl; // 输出 10，刷新顺序：{2=20, 1=10}
    cache.put(3, 30);              // 淘汰 2，缓存变为：{1=10, 3=30}
    cout << cache.get(2) << endl; // 输出 -1（已被淘汰）
    cout << cache.get(3) << endl; // 输出 30
    cache.put(4, 40);              // 淘汰 1，缓存变为：{3=30, 4=40}
    cout << cache.get(1) << endl; // 输出 -1（已被淘汰）
    cout << cache.get(3) << endl; // 输出 30
    cout << cache.get(4) << endl; // 输出 40
}


//------------------------多线程测试------------------------------
void writer(BasicLRU& cache, int base) {
    for (int i = 0; i < 100; ++i) {
        cache.put(base + i, base + i + 1000);
    }
}

void reader(BasicLRU& cache, int base) {
    for (int i = 0; i < 100; ++i) {
        int expected = base+i+1000;
        int val = cache.get(base+i);
        if(val!=-1){
            if(val!=expected){
                cout<<"❌cuowrong：key=" << base + i << " read=" << val << " which should be=" << expected << endl;
            }else {
                cout << "✅right：key=" << base + i << " value=" << val << endl;
            }
        }else {
            cout << "⚠️not hit：key=" << base + i << endl;
        }
        cache.get(base + i);
    }
}

void multiThreadTest(){
    cout<<"==多线程测试=="<<endl;
    BasicLRU cache(200);
    std::thread t1(writer, std::ref(cache), 0);
    std::thread t2(reader, std::ref(cache), 0);
    std::thread t3(writer, std::ref(cache), 100);
    std::thread t4(reader, std::ref(cache), 100);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    cout << "Multi-thread test complete!" << endl;
}

int main(){
    basicFunctionTest();
    multiThreadTest();
    return 0;
}