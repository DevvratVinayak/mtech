#include "concurrent_stack.h"
#include <atomic>
#include <iostream>
#include <cassert>
#include <cstdlib>

void ConcurrentStack::push(int v) {
    Node* new_node = new Node(v);  
    Node* old_top = top.load(std::memory_order_relaxed);

    do {
        new_node->next = old_top;  
    } while (!top.compare_exchange_weak(old_top, new_node, std::memory_order_release, std::memory_order_relaxed));
    // Atomically set the new node as the top of the stack
}

int ConcurrentStack::pop() {
    Node* old_top = top.load(std::memory_order_acquire); 
    while (old_top != nullptr) {
        Node* next_node = old_top->next;
        if (top.compare_exchange_strong(old_top, next_node, std::memory_order_release, std::memory_order_acquire)) {
            int value = old_top->value;  
            delete old_top;  
            return value;
        }
    }
    return -1;
}

// Print the current stack
void ConcurrentStack::print() {
    Node* current = top.load(std::memory_order_acquire);  
    while (current != nullptr) {
        std::cout << current->value << " -> ";
        current = current->next;
    }
    std::cout << "NULL" << std::endl;
}
