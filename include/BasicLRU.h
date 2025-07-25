#ifndef BasicLRU_H
#define BasicLRU_H

#include <unordered_map>
#include<mutex>

class BasicLRU{
private:
//double-link-list define
    struct Node{
        int key;
        int value;
        Node *pre, *next;
        Node(int k, int v);
    };

    int n; //capacity
    std::unordered_map<int, Node *> hash;
    Node *L, *R; //hummy head and tail;
    std::mutex mtx;

    void remove(Node *node);
    void insert(int key, int value);

public:
    BasicLRU(int capacity);
    ~BasicLRU();
    int get(int key);
    void put(int key, int value);
};

#endif