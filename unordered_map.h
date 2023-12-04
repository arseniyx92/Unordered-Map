#pragma once

#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <cassert>
#include <type_traits>

template<typename Key
        , typename Value
        , typename Hash = std::hash<Key>
        , typename Equal = std::equal_to<Key>
        , typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap;

struct BaseNode {
    BaseNode* previous = nullptr;
    BaseNode* next = nullptr;
    BaseNode(): previous(this), next(this) {}
    BaseNode(BaseNode* previous, BaseNode* next)
            : previous(previous)
            , next(next) {}
};

template<typename T>
struct TemplateNode : public BaseNode {
    T value;
    TemplateNode(BaseNode* previous, BaseNode* next)
            : BaseNode(previous, next)
            , value() {}
    TemplateNode(BaseNode* previous, BaseNode* next, const T& given_value)
            : BaseNode(previous, next)
            , value(given_value) {}
    TemplateNode(BaseNode* previous, BaseNode* next, T&& given_value)
            : BaseNode(previous, next)
            , value(std::move(given_value)) {}
};

template<typename T, typename Alloc = std::allocator<T>>
class List : private std::allocator_traits<Alloc>::template rebind_alloc<TemplateNode<T>> {
public:
    using value_type = T;
private:
    template<typename Key
            , typename Value
            , typename Hash
            , typename Equal
            , typename Allocator>
    friend class UnorderedMap;
    using Node = TemplateNode<value_type>;
    using AllocTraits = std::allocator_traits<Alloc>;
    using NodeAlloc = typename AllocTraits::template rebind_alloc<Node>;
    using NodeAllocTraits = std::allocator_traits<NodeAlloc>;
    BaseNode root;
    size_t list_size;

    template<typename Value>
    // NOLINTNEXTLINE
    class BaseIterator {
    public:
        friend List;
        using difference_type = std::ptrdiff_t;
        using value_type = Value;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::bidirectional_iterator_tag;
    private:
        BaseNode* node;
    public:

        BaseNode* return_base_node() {
            return node;
        }
        ~BaseIterator() = default;
        BaseIterator(): node(nullptr) {}
        explicit BaseIterator(BaseNode* given_node): node(given_node) {}
        BaseIterator(const BaseIterator& other): node(other.node) {}
        BaseIterator& operator=(BaseIterator other) {
            node = other.node;
            return *this;
        }

        value_type& operator*() const {
            return static_cast<Node*>(node)->value;
        }

        value_type* operator->() const {
            return &(static_cast<Node*>(node)->value);
        }

        BaseIterator& operator++() {
            node = node->next;
            return *this;
        }

        BaseIterator operator++(int) {
            BaseNode* copy = node;
            node = node->next;
            return BaseIterator(copy);
        }

        BaseIterator& operator--() {
            node = node->previous;
            return *this;
        }

        BaseIterator operator--(int) {
            BaseNode* copy = node;
            node = node->previous;
            return BaseIterator(copy);
        }

        operator BaseIterator<const Value>() const {
            BaseIterator<const Value> copy(node);
            return copy;
        }

        bool operator==(const BaseIterator& other) const = default;
    };
    void switch_alloc(const Alloc& other);
    void move_ones_nodes_to_the_other(List&& other);
public:
    using iterator = BaseIterator<value_type>;
    using const_iterator = BaseIterator<const value_type>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    List(const Alloc& allocator = Alloc());
    List(size_t size);
    List(size_t size, const value_type& value,
         const Alloc& allocator = Alloc());
    List(size_t size, const Alloc& allocator);
    Alloc get_allocator() const;
    List(const List& other);
    List(List&& other);
    ~List();
    List& operator=(const List& other);
    List& operator=(List&& other);
    size_t size() const;
    bool empty() const;
    void push_back(const value_type& value);
    void push_back(value_type&& value);
    void push_front(const value_type& value);
    void pop_back();
    void pop_front();
    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend() const;
    const_iterator begin() const;
    const_iterator end() const;
    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator crbegin() const;
    const_reverse_iterator crend() const;
    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;
    template<typename Value>
    BaseIterator<Value> pointer_inserter
            ( BaseIterator<Value> it
                    , const value_type* value
                    , size_t number_of_elements = 1);
    template<typename Value>
    BaseIterator<Value> insert(BaseIterator<Value> it, const value_type& value);
    template<typename Value>
    BaseIterator<Value> insert(BaseIterator<Value> it, value_type&& value);
    template<typename Value>
    BaseIterator<Value> erase(BaseIterator<Value> it);
};

template<typename T, typename Alloc>
List<T, Alloc>::List(const Alloc& allocator): NodeAlloc(allocator), root(BaseNode()), list_size(0) {}


template<typename T, typename Alloc>
List<T, Alloc>::List(size_t size, const value_type& value, const Alloc& allocator)
        : NodeAlloc(allocator)
        , root(BaseNode())
        , list_size(0) {
    pointer_inserter(end(), &value, size);
}

template<typename T, typename Alloc>
List<T, Alloc>::List(size_t size)
        : NodeAlloc(NodeAlloc())
        , root(BaseNode())
        , list_size(0) {
    pointer_inserter(end(), nullptr, size);
}

template<typename T, typename Alloc>
List<T, Alloc>::List(size_t size, const Alloc& allocator)
        : NodeAlloc(allocator)
        , root(BaseNode())
        , list_size(0) {
    pointer_inserter(end(), nullptr,size);
}

template<typename T, typename Alloc>
List<T, Alloc>::List(const List& other)
        : NodeAlloc(AllocTraits::select_on_container_copy_construction(
        other.get_allocator()))
        , list_size(0) {
    try {
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(*it);
        }
    } catch(...) {
        while (!empty()) {
            pop_back();
        }
        throw;
    }
}

template<typename T, typename Alloc>
List<T, Alloc>::List(List&& other)
        : list_size(0) {
    try {
        const NodeAlloc& other_alloc = other;
        switch_alloc(other_alloc);
        while (!other.empty()) {
            auto it = other.begin();
            BaseNode* node = it.return_base_node();
            node->previous->next = node->next;
            node->next->previous = node->previous;
            BaseNode* last = (--end()).return_base_node();
            BaseNode* after_last = last->next;
            node->next = after_last;
            node->previous = last;
            last->next = node;
            after_last->previous = node;
            --other.list_size;
            ++list_size;
        }
    } catch(...) {
        assert(false);
        while (!empty()) {
            pop_back();
        }
        throw;
    }
}

template<typename T, typename Alloc>
Alloc List<T, Alloc>::get_allocator() const {
    return static_cast<NodeAlloc>(*this);
}

template<typename T, typename Alloc>
List<T, Alloc>::~List() {
    while (!empty()) {
        pop_back();
    }
}

template<typename T, typename Alloc>
void List<T, Alloc>::switch_alloc(const Alloc& other_alloc) {
    NodeAlloc& alloc = *this;
    alloc = other_alloc;
}

template<typename T, typename Alloc>
List<T, Alloc>& List<T, Alloc>::operator=(const List& other) {
    if (this == &other) {
        return *this;
    }
    NodeAlloc deprecated_alloc = *this;
    size_t previous_size = size();
    bool should_copy_allocator = AllocTraits::propagate_on_container_copy_assignment::value;
    if (should_copy_allocator) {
        const NodeAlloc& other_alloc = other;
        switch_alloc(other_alloc);
    }
    try {
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(*it);
        }
    } catch(...) {
        while (size() - previous_size) {
            pop_back();
        }
        if (should_copy_allocator) {
            switch_alloc(deprecated_alloc);
        }
        throw;
    }
    if (should_copy_allocator) {
        switch_alloc(deprecated_alloc);
    }
    while (previous_size--) {
        pop_front();
    }
    if (should_copy_allocator) {
        const NodeAlloc& other_alloc = other;
        switch_alloc(other_alloc);
    }
    return *this;
}

template<typename T, typename Alloc>
void List<T, Alloc>::move_ones_nodes_to_the_other(List&& other) {
    while (!other.empty()) {
        auto it = other.begin();
        BaseNode* node = it.return_base_node();
        node->previous->next = node->next;
        node->next->previous = node->previous;
        BaseNode* last = (--end()).return_base_node();
        last->next->previous = node;
        node->next = last->next;
        node->previous = last;
        last->next = node;
        --other.list_size;
        ++list_size;
    }
}

template<typename T, typename Alloc>
List<T, Alloc>& List<T, Alloc>::operator=(List&& other) {
    if (this == &other) {
        return *this;
    }
    try {
        while (!empty()) pop_back();
        bool should_move_allocator = AllocTraits::propagate_on_container_move_assignment::value;
        if (should_move_allocator) {
            NodeAlloc& alloc = *this;
            const NodeAlloc& other_alloc = other;
            alloc = std::move(other_alloc);
        }
        NodeAlloc& alloc = *this;
        const NodeAlloc& other_alloc = other;
        if (alloc == other_alloc) {
            move_ones_nodes_to_the_other(std::move(other));
            return *this;
        } else {
            while (!other.empty()) {
                auto it = other.begin();
                BaseNode *node = it.return_base_node();
                Node *new_node = NodeAllocTraits::allocate(*this, 1);
                BaseNode *last = (--end()).return_base_node();
                NodeAllocTraits::construct(*this, new_node, last, last->next,
                                           std::move(static_cast<Node *>(node)->value));
                other.erase(other.begin());
                last->next->previous = new_node;
                last->next = new_node;
                ++list_size;
            }
        }
    } catch(...) {
        assert(false);
        throw;
    }
    return *this;
}

template<typename T, typename Alloc>
size_t List<T, Alloc>::size() const {
    return list_size;
}

template<typename T, typename Alloc>
bool List<T, Alloc>::empty() const {
    return list_size == 0;
}

template<typename T, typename Alloc>
void List<T, Alloc>::push_back(const value_type& value) {
    insert(end(), value);
}

template<typename T, typename Alloc>
void List<T, Alloc>::push_back(value_type&& value) {
    insert(end(), std::move(value));
}

template<typename T, typename Alloc>
void List<T, Alloc>::push_front(const value_type& value) {
    insert(begin(), value);
}

template<typename T, typename Alloc>
void List<T, Alloc>::pop_back() {
    erase(--end());
}

template<typename T, typename Alloc>
void List<T, Alloc>::pop_front() {
    erase(begin());
}

template<typename T, typename Alloc>
typename List<T, Alloc>::iterator List<T, Alloc>::begin() {
    return iterator(root.next);
}

template<typename T, typename Alloc>
typename List<T, Alloc>::iterator List<T, Alloc>::end() {
    return iterator(&root);
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_iterator List<T, Alloc>::cbegin() const {
    return iterator(root.next);
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_iterator List<T, Alloc>::cend() const {
    return iterator(root.next->previous);
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_iterator List<T, Alloc>::begin() const {
    return cbegin();
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_iterator List<T, Alloc>::end() const {
    return cend();
}

template<typename T, typename Alloc>
typename List<T, Alloc>::reverse_iterator List<T, Alloc>::rbegin() {
    return static_cast<reverse_iterator>(end());
}

template<typename T, typename Alloc>
typename List<T, Alloc>::reverse_iterator List<T, Alloc>::rend() {
    return static_cast<reverse_iterator>(begin());
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_reverse_iterator List<T, Alloc>::crbegin() const {
    return static_cast<const_reverse_iterator>(end());
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_reverse_iterator List<T, Alloc>::crend() const {
    return static_cast<const_reverse_iterator>(begin());
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_reverse_iterator List<T, Alloc>::rbegin() const {
    return crbegin();
}

template<typename T, typename Alloc>
typename List<T, Alloc>::const_reverse_iterator List<T, Alloc>::rend() const {
    return crend();
}

template<typename T, typename Alloc>
template<typename Value>
typename List<T, Alloc>::template BaseIterator<Value> List<T, Alloc>::pointer_inserter(
        BaseIterator<Value> it, const value_type* value, size_t number_of_elements) {
    Node* new_node = static_cast<Node*>(it.node);
    size_t instance = 0;
    try {
        for (; instance < number_of_elements; ++instance) {
            BaseNode* insertion_node = it.node;
            BaseNode* previos_node = insertion_node->previous;
            new_node = NodeAllocTraits::allocate(*this, 1);
            try {
                if (value == nullptr) {
                    if constexpr (std::is_default_constructible<value_type>::value) {
                        NodeAllocTraits::construct(*this, new_node, previos_node, insertion_node);
                    }
                } else {
                    NodeAllocTraits::construct(*this, new_node, previos_node, insertion_node, *value);
                }
            } catch(...) {
                NodeAllocTraits::deallocate(*this, new_node, 1);
                throw;
            }
            insertion_node->previous = new_node;
            previos_node->next = new_node;
            ++list_size;
        }
    } catch(...) {
        while(instance--) {
            auto copy = it;
            erase(--copy);
        }
        throw;
    }
    return iterator(new_node);
}

template<typename T, typename Alloc>
template<typename Value>
typename List<T, Alloc>::template BaseIterator<Value> List<T, Alloc>::insert(
        BaseIterator<Value> it, const value_type& value) {
    return pointer_inserter(it, &value);
}

template<typename T, typename Alloc>
template<typename Value>
typename List<T, Alloc>::template BaseIterator<Value> List<T, Alloc>::insert(
        BaseIterator<Value> it, value_type&& value) {
    Node* new_node = static_cast<Node*>(it.node);
    size_t number_of_elements = 1;
    size_t instance = 0;
    try {
        for (; instance < number_of_elements; ++instance) {
            BaseNode* insertion_node = it.node;
            BaseNode* previos_node = insertion_node->previous;
            new_node = NodeAllocTraits::allocate(*this, 1);
            try {
                NodeAllocTraits::construct(*this, new_node, previos_node, insertion_node, std::move(value));
            } catch(...) {
                NodeAllocTraits::deallocate(*this, new_node, 1);
                throw;
            }
            insertion_node->previous = new_node;
            previos_node->next = new_node;
            ++list_size;
        }
    } catch(...) {
        while(instance--) {
            auto copy = it;
            erase(--copy);
        }
        throw;
    }
    return iterator(new_node);
}

template<typename T, typename Alloc>
template<typename Value>
typename List<T, Alloc>::template BaseIterator<Value> List<T, Alloc>::erase(
        BaseIterator<Value> it) {
    BaseNode* erasing_node = it.node;
    BaseNode* next = erasing_node->next;
    BaseNode* previous = erasing_node->previous;
    NodeAllocTraits::destroy(*this, static_cast<Node*>(erasing_node));
    NodeAllocTraits::deallocate(*this, static_cast<Node*>(erasing_node), 1);
    erasing_node = nullptr;
    next->previous = previous;
    previous->next = next;
    --list_size;
    return iterator(next);
}


#include <initializer_list>
#include <stdexcept>
#include <cmath>
#include <utility>
#include <vector>

template<typename Key
        , typename Value
        , typename Hash
        , typename Equal
        , typename Alloc>
class UnorderedMap {
public:
    using NodeType = std::pair<const Key, Value>;
private:
    using LowSecurityNodeType = std::pair<Key, Value>;
    struct HashedNode {
        LowSecurityNodeType element;
        size_t hash;
    };
    using AllocTraits = std::allocator_traits<Alloc>;
    using HashedNodeAlloc = typename AllocTraits::template rebind_alloc<HashedNode>;
    template<typename ItValue>
    using ListBaseIt = typename List<HashedNode, HashedNodeAlloc>:: template BaseIterator<ItValue>;
    using ListIt = typename List<HashedNode, HashedNodeAlloc>::iterator;
    using ListConstIt = typename List<HashedNode, HashedNodeAlloc>::const_iterator;


    template<typename ItValue>
    // NOLINTNEXTLINE
    class BaseIterator {
    public:
        friend UnorderedMap;
        using difference_type = std::ptrdiff_t;
        using value_type = ItValue;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::bidirectional_iterator_tag;
    private:
        BaseNode* node;
    public:
        BaseNode* return_base_node() {
            return node;
        }
        BaseIterator() = default;
        ~BaseIterator() = default;
        explicit BaseIterator(BaseNode* given_node): node(given_node) {}
        explicit BaseIterator(ListBaseIt<HashedNode> given_list_it): node(given_list_it.return_base_node()) {}
        explicit BaseIterator(ListBaseIt<const HashedNode> given_list_it): node(given_list_it.return_base_node()) {}
        BaseIterator(const BaseIterator& other): node(other.node) {}
        BaseIterator& operator=(BaseIterator other) {
            node = other.node;
            return *this;
        }
        value_type& operator*() const {
            return *reinterpret_cast<NodeType*>(&(static_cast<TemplateNode<HashedNode>*>(node)->value.element));
        }
        value_type* operator->() const {
            return reinterpret_cast<NodeType*>(&(static_cast<TemplateNode<HashedNode>*>(node)->value.element));
        }

        BaseIterator& operator++() {
            node = node->next;
            return *this;
        }

        BaseIterator operator++(int) {
            BaseNode* copy = node;
            node = node->next;
            return BaseIterator(copy);
        }

        BaseIterator& operator--() {
            node = node->previous;
            return *this;
        }

        BaseIterator operator--(int) {
            BaseNode* copy = node;
            node = node->previous;
            return BaseIterator(copy);
        }

        operator BaseIterator<const ItValue>() const {
            BaseIterator<const ItValue> copy(node);
            return copy;
        }

        bool operator==(const BaseIterator& other) const = default;
    };
public:
    using iterator = BaseIterator<NodeType>;
    using const_iterator = BaseIterator<const NodeType>;
private:
    [[no_unique_address]] Hash hash;
    [[no_unique_address]] Equal equal;
    [[no_unique_address]] Alloc alloc;
    float max_load_factor_value = 1.0;
    const static size_t default_bucket_count = 5;
    List<HashedNode, HashedNodeAlloc> list;
    std::vector<BaseNode*> buckets;
    void rehash(size_t new_bucket_count);
    size_t bucketId(size_t given_hash) const;
    void update_buckets(size_t new_bucket_count);

    template<typename U>
    std::pair<iterator, bool> insert_impl(U&& element);
public:
    iterator begin();
    const_iterator cbegin() const;
    const_iterator begin() const;
    iterator end();
    const_iterator cend() const;
    const_iterator end() const;
    void reserve(size_t count);
    size_t max_size() const noexcept;
    float load_factor() const noexcept;
    float max_load_factor() const noexcept;
    void max_load_factor(float ml);
    UnorderedMap() = default;
    ~UnorderedMap() {
        while (size() != 0) {
            erase(begin());
        }
    }
    explicit UnorderedMap(size_t bucket_count
            , const Hash& hash = Hash()
            , const Equal& equal = Equal()
            , const Alloc& alloc = Alloc());
    UnorderedMap(size_t bucket_count
            , const Alloc& alloc)
            : UnorderedMap(bucket_count, Hash(), Equal(), alloc) {}
    UnorderedMap(size_t bucket_count
            , const Hash& hash
            , const Alloc& alloc)
            : UnorderedMap(bucket_count, hash, Equal(), alloc) {}
    explicit UnorderedMap(const Alloc& alloc);
//    template<typename InputIt>
//    UnorderedMap(InputIt first, InputIt last
//            , size_t bucket_count = 0
//            , const Hash& hash = Hash()
//            , const Equal& equal = Equal()
//            , const Alloc& alloc = Alloc());
    template<typename InputIt>
    UnorderedMap(InputIt first, InputIt last
            , size_t bucket_count
            , const Alloc& alloc)
            : UnorderedMap(first, last, bucket_count, Hash(), Equal(), alloc) {}
    template<typename InputIt>
    UnorderedMap(InputIt first, InputIt last
            , size_t bucket_count
            , const Hash& hash
            , const Alloc& alloc)
            : UnorderedMap(first, last, bucket_count, hash, Equal(), alloc) {}
    UnorderedMap(const UnorderedMap& other);
    UnorderedMap(UnorderedMap&& other);
    UnorderedMap(std::initializer_list<NodeType> init
            , size_t bucket_count = 0
            , const Hash& hash = Hash()
            , const Equal& equal = Equal()
            , const Alloc& alloc = Alloc());
    UnorderedMap(std::initializer_list<NodeType> init
            , size_t bucket_count
            , const Alloc& alloc)
            : UnorderedMap(init, bucket_count,
                           Hash(), Equal(), alloc) {}
    UnorderedMap(std::initializer_list<NodeType> init
            , size_t bucket_count
            , const Hash& hash
            , const Alloc& alloc)
            : UnorderedMap(init, bucket_count,
                           hash, Equal(), alloc) {}
    UnorderedMap& operator=(const UnorderedMap& other);
    UnorderedMap& operator=(UnorderedMap&& other);
    size_t size() const;
    Value& at(const Key& key);
    const Value& at(const Key& key) const;
    Value& operator[](const Key& key);
    Value& operator[](Key&& key);
    std::pair<iterator, bool> insert(const NodeType& element);
    std::pair<iterator, bool> insert(NodeType&& element);
    template<typename InputIt>
    void insert(InputIt first, InputIt last);
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);
    iterator erase(const_iterator pos);
    iterator erase(const_iterator first, const_iterator last);
    iterator find(const Key& key);
    const_iterator find(const Key& key) const;
    void swap(UnorderedMap& other);
    Alloc get_allocator() const;
};

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
Alloc UnorderedMap<Key, Value, Hash, Equal, Alloc>::get_allocator() const {
    return alloc;
}


template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
size_t UnorderedMap<Key, Value, Hash, Equal, Alloc>::bucketId(size_t given_hash) const {
    return given_hash % buckets.size();
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
float UnorderedMap<Key, Value, Hash, Equal, Alloc>::max_load_factor() const noexcept {
    return max_load_factor_value;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::max_load_factor(float ml) {
    max_load_factor_value = ml;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(size_t bucket_count
        , const Hash& hash
        , const Equal& equal
        , const Alloc& alloc)
        : hash(hash), equal(equal), alloc(alloc), list(alloc), buckets(alloc) {
    rehash(bucket_count);
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(const Alloc& alloc)
        : hash(Hash()), equal(Equal()), alloc(alloc), list(alloc), buckets(alloc) {}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(const UnorderedMap& other)
        : hash(other.hash)
        , equal(other.equal)
        , alloc(AllocTraits::select_on_container_copy_construction(other.alloc))
        , max_load_factor_value(other.max_load_factor_value)
        , list(other.list)
        , buckets(alloc) {
    update_buckets(other.buckets.size());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(UnorderedMap&& other)
        : hash(std::move(other.hash))
        , equal(std::move(other.equal))
        , alloc(std::move(other.alloc))
        , max_load_factor_value(std::move(other.max_load_factor_value))
        , list(std::move(other.list))
        , buckets(std::move(other.buckets)) {}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(std::initializer_list<NodeType> init
        , size_t bucket_count
        , const Hash& hash
        , const Equal& equal
        , const Alloc& alloc)
        : UnorderedMap(bucket_count, hash, equal, alloc) {
    for (auto val : init) {
        insert(std::move(val));
    }
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
size_t UnorderedMap<Key, Value, Hash, Equal, Alloc>::size() const {
    return list.size();
}

struct UnorderedMapAtKeyNotFoundException : std::out_of_range {
    explicit UnorderedMapAtKeyNotFoundException()
            : std::out_of_range("UnorderedMapAtKeyNotFoundException") {}
};

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::begin() {
    return iterator(list.begin());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::cbegin() const {
    return const_iterator(list.cbegin());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::begin() const {
    return cbegin();
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::end() {
    return iterator(list.end());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::cend() const {
    return const_iterator(list.cend());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::end() const {
    return cend();
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
std::pair<typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator, bool>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::insert(const NodeType& element) {
    insert_impl(*reinterpret_cast<LowSecurityNodeType*>(&element));
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
std::pair<typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator, bool>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::insert(NodeType&& element) {
    return insert_impl(std::move(*reinterpret_cast<LowSecurityNodeType*>(&element)));
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
template<typename U>
std::pair<typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator, bool>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::insert_impl(U&& element) {
    if (buckets.empty()) {
        rehash(default_bucket_count);
    }
    size_t key_hash = hash(element.first);
    size_t shrinked_hash = bucketId(key_hash);
    if (buckets[shrinked_hash] != nullptr) {
        for (ListIt it = ListIt(buckets[shrinked_hash]); it != list.end(); ++it) {
            if (it->hash != key_hash) break;
            if (equal(it->element.first, element.first)) return {iterator(it), false};
        }
        buckets[shrinked_hash] = list.insert(ListIt(buckets[shrinked_hash]),
                                             HashedNode{std::forward<U>(element), key_hash}).return_base_node();
    } else {
        buckets[shrinked_hash] = list.insert(list.end(), HashedNode{std::forward<U>(element), key_hash}).return_base_node();
    }
    if (load_factor() > max_load_factor()) {
        rehash(2 * buckets.size());
    }
    return {iterator(ListIt(buckets[shrinked_hash])), true};
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
template<typename InputIt>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::insert(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
        insert_impl(*it);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
template<typename... Args>
std::pair<typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator, bool>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::emplace(Args&&... args) {
    typename std::aligned_storage<sizeof(NodeType), alignof(NodeType)>::type data;
    auto new_node = reinterpret_cast<NodeType*>(&data);
    AllocTraits::construct(alloc, new_node, std::forward<Args>(args)...);
    return insert_impl(std::move(*reinterpret_cast<LowSecurityNodeType*>(new_node)));
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::erase(
        typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator pos) {
    auto list_const_it = ListConstIt(pos.return_base_node());
    size_t key_hash = list_const_it->hash;
    size_t shrinked_hash = bucketId(key_hash);
    if (buckets[shrinked_hash] == list_const_it.return_base_node()) {
        auto next_it = list.erase(list_const_it);
        if (next_it != list.end() && next_it->hash == key_hash) {
            buckets[shrinked_hash] = next_it.return_base_node();
        } else {
            buckets[shrinked_hash] = nullptr;
        }
        return iterator(next_it);
    } else {
        return iterator(list.erase(list_const_it));
    }
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::erase(
        UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator first,
        UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator last) {
    auto it = first;
    while (it != last) {
        it = erase(it);
    }
    return iterator(last.return_base_node());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::find(const Key& key) {
    auto it = static_cast<const UnorderedMap<Key, Value, Hash, Equal, Alloc>&>(*this).find(key);
    return iterator(it.return_base_node());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::find(const Key& key) const {
    if (buckets.empty()) {
        return iterator(list.end());
    }
    size_t key_hash = hash(key);
    size_t shrinked_hash = bucketId(key_hash);
    if (buckets[shrinked_hash] != nullptr) {
        for (auto it = ListConstIt(buckets[shrinked_hash]); it != list.end(); ++it) {
            if (bucketId(it->hash) != shrinked_hash) break;
            if (equal(it->element.first, key)) return const_iterator(it);
        }
    }
    return const_iterator(list.cend());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
const Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::at(const Key& key) const {
    auto it = find(key);
    if (it == const_iterator(list.end())) {
        throw UnorderedMapAtKeyNotFoundException();
    }
    return it->second;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::at(const Key& key) {
    return const_cast<Value&>(static_cast<const UnorderedMap<Key, Value, Hash, Equal, Alloc>&>
                    (*this).at(key));
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator[](const Key& key) {
    auto it = find(key);
    if (it == iterator(list.end())) {
        std::pair<iterator, bool> result = insert({key, Value()});
        return (*result.first).second;
    }
    return it->second;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator[](Key&& key) {
    auto it = find(key);
    if (it == iterator(list.end())) {
        std::pair<iterator, bool> result = insert({std::move(key), Value()});
        return (*result.first).second;
    }
    return it->second;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::reserve(size_t count) {
    rehash(std::ceil(count / max_load_factor()));
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
size_t UnorderedMap<Key, Value, Hash, Equal, Alloc>::max_size() const noexcept {
    return std::floor(buckets.size() * max_load_factor());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
float UnorderedMap<Key, Value, Hash, Equal, Alloc>::load_factor() const noexcept {
    return static_cast<float>(size()) / static_cast<float>(buckets.size());
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::rehash(size_t new_bucket_count) {
    decltype(list) moved_list = std::move(list);
    buckets.assign(new_bucket_count, nullptr);
    for (auto it = moved_list.begin(); it != moved_list.end(); ++it) {
        insert_impl(std::move(it->element));
    }
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::swap(
        UnorderedMap<Key, Value, Hash, Equal, Alloc>& other) {
    std::swap(equal, other.equal);
    std::swap(hash, other.hash);
    std::swap(alloc, other.alloc);
    std::swap(max_load_factor_value, other.max_load_factor_value);
    std::swap(buckets, other.buckets);
    std::swap(list, other.list);
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>&
UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator=(const UnorderedMap& other) {
    if (this == &other) {
        return *this;
    }
    auto deprecated_alloc = alloc;
    std::vector<BaseNode*> buckets_copy = buckets;
    try {
        if (AllocTraits::propagate_on_container_copy_assignment::value == true) {
            alloc = other.alloc;
        }
        list = other.list;
        update_buckets(other.buckets.size());
    } catch (...) {
        alloc = deprecated_alloc;
        buckets = buckets_copy;
        throw;
    }
    max_load_factor_value = other.max_load_factor_value;
    hash = other.hash;
    equal = other.equal;
    return *this;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>&
UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator=(UnorderedMap&& other) {
    if (this == &other) {
        return *this;
    }
    max_load_factor_value = std::move(other.max_load_factor_value);
    hash = std::move(other.hash);
    equal = std::move(other.equal);
    std::vector<BaseNode*> buckets_copy = buckets;
    try {
        list = std::move(other.list);
        update_buckets(other.buckets.size());
    } catch (...) {
        buckets = buckets_copy;
        throw;
    }
    return *this;
}

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::update_buckets(size_t new_bucket_count) {
    buckets = std::vector<BaseNode*>(new_bucket_count);
    if (size() == 0) return;
    size_t shrinked_hash = bucketId(list.begin()->hash);
    buckets[shrinked_hash] = list.begin().return_base_node();
    for (auto it = ++list.begin(); it != list.end(); ++it) {
        size_t new_hash = bucketId(it->hash);
        if (new_hash != shrinked_hash) {
            shrinked_hash = new_hash;
            buckets[shrinked_hash] = it.return_base_node();
        }
    }
}

