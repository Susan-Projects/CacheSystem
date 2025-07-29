#include <iostream>
#include "KLruKCache_v1.h"

using namespace KamaCache;
using namespace std;

void basicKLruKTest(){
    KLruKCache<int, string> cache(/*mainC=*/2, /*historyC=*/5, /*k=*/2);
    cout << "== KLruKCache Functional Test ==\n";

    cache.put(1,"one");
    string result = cache.get(1);//count==2==k
    cout<<"[1st get] Result: \""<<result<<"\" (expect default or \"one\" if inserted)\n";

    result = cache.get(1);
    cout << "[2nd get] Result: \"" << result << "\" (expect \"one\")\n";

    cache.put(2,"two");
    result = cache.get(3); // 访问次数 = 1
    cout << "[get 3] Result: \"" << result << "\" (expect default)\n";

    cache.put(3, "three");
    cache.get(3); // count = 2，插入缓存
    result = cache.get(3); // 从主缓存中拿出
    cout << "[get 3 again] Result: \"" << result << "\" (expect \"three\")\n";
}

int main() {
    basicKLruKTest();
    return 0;
}

