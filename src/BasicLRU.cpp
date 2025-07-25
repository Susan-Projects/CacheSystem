#include "../include/BasicLRU.h"
//该文件为项目模版练习代码，进阶代码风格（basic实现）在LRU_v1中
//Node
BasicLRU::Node::Node(int k, int v):key(k), value(v), pre(nullptr), next(nullptr){}

BasicLRU::BasicLRU(int capacity):n(capacity){
    L=new Node(-1,-1);
    R=new Node(-1,-1);
    L->next = R;
    R->pre = L;
}
BasicLRU::~BasicLRU(){
    Node* cur = L;
    while(cur != nullptr){
        Node* next = cur->next;
        delete cur;
        cur = next;
    }
}
void BasicLRU::remove(Node *node){
    Node* pre = node->pre;
    Node* next = node->next;
    pre->next = next;
    next->pre = pre;
    hash.erase(node->key);
}
void BasicLRU::insert(int key, int value) {
    Node* pre = R->pre;
    Node* newNode = new Node(key, value);
    pre->next = newNode;
    newNode->pre = pre;
    newNode->next = R;
    R->pre = newNode;
    hash[key] = newNode;
}

int BasicLRU::get(int key) {
    std::lock_guard<std::mutex> lock(mtx);
    if (hash.count(key)) {
        Node* node = hash[key];
        remove(node);
        insert(node->key, node->value);
        return node->value;
    }
    return -1;
}

void BasicLRU::put(int key, int value) {
    std::lock_guard<std::mutex> lock(mtx);
    if (hash.count(key)) {
        Node* node = hash[key];
        remove(node);
        insert(key, value);
    } else {
        if (hash.size() == n) {
            Node* node = L->next;
            remove(node);
            delete node;  // 别忘了释放旧的
        }
        insert(key, value);
    }
}
