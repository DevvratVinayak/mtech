#include <atomic>
#include <cstdint>

struct Node {
    int value;
    std::atomic<uint64_t> next;  // Packed [ptr:48 | tag:16]
    explicit Node(int val) : value(val), next(0) {}  
};

class ConcurrentQueue {
private:
    std::atomic<uint64_t> head;
    std::atomic<uint64_t> tail;

    static constexpr uint64_t PTR_MASK = (1ULL << 48) - 1;  // 48-bit mask
    static constexpr uint64_t TAG_SHIFT = 48;

    Node* extract_ptr(uint64_t packed) {
        return reinterpret_cast<Node*>(packed & PTR_MASK);
    }

    uint16_t extract_tag(uint64_t packed) {
        return static_cast<uint16_t>(packed >> TAG_SHIFT);
    }

    uint64_t pack_ptr(Node* ptr, uint16_t tag) {
        return (reinterpret_cast<uint64_t>(ptr) & PTR_MASK) | (static_cast<uint64_t>(tag) << TAG_SHIFT);
    }

public:
    ConcurrentQueue() {
        Node* dummy = new Node(-1);
        head.store(pack_ptr(dummy, 0), std::memory_order_relaxed);
        tail.store(pack_ptr(dummy, 0), std::memory_order_relaxed);
    }

    ~ConcurrentQueue() {
        uint64_t h = head.load(std::memory_order_relaxed);
        Node* current = extract_ptr(h);
        while (current) {
            uint64_t next_packed = current->next.load(std::memory_order_relaxed);
            Node* next = extract_ptr(next_packed);
            delete current;
            current = next;
        }
    }

    std::string toString() {
        std::ostringstream oss;
        uint64_t current_head = head.load(std::memory_order_acquire);
        Node* current_node = extract_ptr(current_head);
        
        // Get first real node (skip dummy)
        uint64_t next_packed = current_node->next.load(std::memory_order_acquire);
        Node* node = extract_ptr(next_packed);
        
        while (node != nullptr) {
            oss << node->value;
            next_packed = node->next.load(std::memory_order_acquire);
            node = extract_ptr(next_packed);
            if (node) oss << " ";
        }
        
        return oss.str();
    }

    bool enq(int value) {
        Node* new_node = new Node(value);
        uint64_t new_packed = pack_ptr(new_node, 0);

        while (true) {
            uint64_t curr_tail = tail.load(std::memory_order_acquire);
            Node* tail_node = extract_ptr(curr_tail);
            uint64_t next_packed = tail_node->next.load(std::memory_order_acquire);

            if (next_packed != 0) {
                // Help advance tail
                uint16_t new_tag = extract_tag(curr_tail) + 1;
                tail.compare_exchange_weak(
                    curr_tail,
                    pack_ptr(extract_ptr(next_packed), new_tag),
                    std::memory_order_release
                );
                continue;
            }

            if (tail_node->next.compare_exchange_weak(
                next_packed,
                new_packed,
                std::memory_order_release
            )) {
                // Update tail (may fail)
                uint16_t new_tag = extract_tag(curr_tail) + 1;
                tail.compare_exchange_strong(
                    curr_tail,
                    pack_ptr(new_node, new_tag),
                    std::memory_order_release
                );
                return true;
            }
        }
    }

    bool deq(int& value) {
        while (true) {
            uint64_t curr_head = head.load(std::memory_order_acquire);
            Node* head_node = extract_ptr(curr_head);
            uint64_t next_packed = head_node->next.load(std::memory_order_acquire);

            if (next_packed == 0) return false; // Empty

            value = extract_ptr(next_packed)->value;
            uint16_t new_tag = extract_tag(curr_head) + 1;
            uint64_t new_head = pack_ptr(extract_ptr(next_packed), new_tag);

            if (head.compare_exchange_strong(
                curr_head,
                new_head,
                std::memory_order_release
            )) {
                delete head_node;
                return true;
            }
        }
    }
};