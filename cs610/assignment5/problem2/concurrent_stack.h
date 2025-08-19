#ifndef CONC_STACK_H
#define CONC_STACK_H

#include <atomic>
#include <iostream>

struct Node {
    int value;         
    Node* next;        

    Node(int v) : value(v), next(nullptr) {}
};

class ConcurrentStack {
private:
    std::atomic<Node*> top;        
    std::atomic<int> pop_count;    
public:
    ConcurrentStack() : top(nullptr), pop_count(0) {}

    void push(int v);

    int pop();

    void print();
};

#endif 
