//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../containers/detail/transparent.h"
#include "../../containers/vector.h"
#include "../../core/make_tracked.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The hash array mapped trie of Bagwell (Ideal Hash Trees, 2001) as
    // Clojure's PersistentHashMap uses it, persistent: every insert and
    // erase makes a new trie that shares all but the path it changed with
    // the old one, and nothing is ever modified. A node has 32 slots, one
    // per value of the five bits of the hash its level consumes, and
    // stores only the slots in use, packed in slot order: a bitmap says
    // which, and the entry of a slot is at the population count of the
    // bits below it. An entry holds an element, or the subtrie of the
    // elements whose hashes agree with it so far (a second bitmap says
    // which); two elements with the same hash to the last bit hang off
    // one entry as a chain. The nodes come in six sizes, with room for
    // 1, 2, 4, 8, 16 or 32 entries, each a managed type of its own, so
    // that a node is a managed object with a destructor for its elements
    // and a pointer map of its own: the collector destroys the elements
    // and frees the node once no version of the trie reaches it.

    // What every node begins with: the layout of its entries. Two words,
    // laid out so that neither can pass for an address: the collector
    // classifies the words of a type by elimination and, in a debug
    // build, reports a word it took for data that holds an address
    // (collector.h, _check_removed_offsets); two bitmaps packed in one
    // word would trip that now and then (a small one in the high half
    // over any low half is an address on macOS), the bitmap alone in a
    // word is below 4 GB, and the counts in the high half of the second
    // word put it beyond a terabyte.
    struct HamtHead {
        uint32_t bitmap = 0;     // the slots in use, one bit each; the entries are in slot order
        uint32_t reserved = 0;
        uint32_t subtries = 0;   // of those, the slots whose entry is a subtrie (the others hold an element)
        uint8_t built = 0;       // the entries filled so far: a node is filled in order, and the destructor destroys these
        uint8_t capacity = 0;    // the entries the node has room for: 1, 2, 4, 8, 16 or 32
        uint16_t reserved2 = 0;
    };

    // One entry: a subtrie, or an element with the chain of the other
    // elements that have exactly its hash (null for none); the link is
    // the one or the other. Constructed in place with its contents when
    // the node is filled, one store of the link with its barrier, and
    // destroyed by the node's destructor. The element lives in a union
    // with itself as the sole member, which keeps its pointers at fixed
    // offsets for the node's map; an entry holding a subtrie leaves null
    // words there (a fresh page is zero, a destroyed tracked_ptr null).
    template<class V>
    struct HamtEntry {
        tracked_ptr<HamtHead> link;
        union {
            V value;
        };

        // A subtrie
        explicit HamtEntry(const tracked_ptr<HamtHead>& subtrie) noexcept
        : link(subtrie) {
        }

        // An element built from a..., with its chain
        template<class... A>
        HamtEntry(const tracked_ptr<HamtHead>& chain, std::in_place_t, A&&... a)
        : link(chain)
        , value(std::forward<A>(a)...) {
        }

        HamtEntry(const HamtEntry&) = delete;
        HamtEntry& operator=(const HamtEntry&) = delete;

        // The link only: the value, if any, is destroyed by the node
        ~HamtEntry() {
        }
    };

    // A node with room for N entries, in raw storage: the entries are
    // constructed in slot order as the node is filled, `built` of them,
    // and those are what the destructor destroys (the element, when the
    // entry holds one, then the link); the rest of the storage holds
    // null words. The head comes first, as the sole base, so that a link
    // to the head addresses the node.
    template<class V, unsigned N>
    struct HamtNode : HamtHead {
        union {
            HamtEntry<V> entries[N];
        };

        HamtNode() noexcept {
            capacity = N;
        }

        HamtNode(const HamtNode&) = delete;
        HamtNode& operator=(const HamtNode&) = delete;

        ~HamtNode() {
            auto bits = bitmap;
            for (unsigned i = 0; i < built; ++i) {
                auto bit = bits & (0u - bits);
                bits &= bits - 1;
                if (!(subtries & bit)) {
                    entries[i].value.~V();
                }
                entries[i].~HamtEntry();
            }
        }
    };

    // One element of a chain: the elements whose hashes are equal to the
    // last bit, after the one in the entry. A head like a node's so that
    // an entry's link is one type; its fields unused.
    template<class V>
    struct HamtChain : HamtHead {
        V value;
        tracked_ptr<HamtChain> next;

        template<class... A>
        explicit HamtChain(tracked_ptr<HamtChain> next, A&&... a)
        : value(std::forward<A>(a)...)
        , next(std::move(next)) {
        }
    };

    // The entries of a node, whatever its size: the array begins at the
    // same offset in every size, but the type is asked, not assumed
    template<class V>
    inline HamtEntry<V>* hamt_entries(HamtHead* node) noexcept {
        switch (node->capacity) {
            case 1: return static_cast<HamtNode<V, 1>*>(node)->entries;
            case 2: return static_cast<HamtNode<V, 2>*>(node)->entries;
            case 4: return static_cast<HamtNode<V, 4>*>(node)->entries;
            case 8: return static_cast<HamtNode<V, 8>*>(node)->entries;
            case 16: return static_cast<HamtNode<V, 16>*>(node)->entries;
            default: return static_cast<HamtNode<V, 32>*>(node)->entries;
        }
    }

    template<class V>
    inline const HamtEntry<V>* hamt_entries(const HamtHead* node) noexcept {
        return hamt_entries<V>(const_cast<HamtHead*>(node));
    }

    // A forward iterator over the elements: the path from the root, a
    // node and the slots of it still to visit per level, and the chain
    // node when inside a chain. Raw pointers only: the trie is held by
    // the container the iterator came from, and the iterator is valid
    // while that object exists. Copyable; two iterators are equal when
    // they address the same element.
    template<class V>
    class HamtIterator {
        using Entry = HamtEntry<V>;
        using Chain = HamtChain<V>;

    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = V;
        using reference = const V&;
        using pointer = const V*;
        using difference_type = ptrdiff_t;

        HamtIterator() noexcept = default;

        // At the first element under `root`; the end for null
        explicit HamtIterator(const HamtHead* root) noexcept {
            if (root) {
                _nodes[0] = root;
                _remaining[0] = root->bitmap;
                _depth = 0;
                _settle();
            }
        }

        // A dead iterator keeps no word the conservative scan of the
        // stack could take for a root
        ~HamtIterator() noexcept {   // volatile, as tracked_ptr's: a store the compiler may not drop as dead
            for (auto& n : _nodes) {
                *(void* volatile*)&n = nullptr;
            }
            *(void* volatile*)&_entry = nullptr;
            *(void* volatile*)&_chain = nullptr;
        }

        reference operator*() const noexcept {
            return _chain ? _chain->value : _entry->value;
        }

        pointer operator->() const noexcept {
            return std::addressof(**this);
        }

        HamtIterator& operator++() noexcept {
            if (_chain) {
                _chain = _chain->next.get();
                if (_chain) {
                    return *this;
                }
            } else if (auto chain = _entry->link.get()) {   // the element's chain comes after it
                _chain = static_cast<const Chain*>(chain);
                return *this;
            }
            _remaining[_depth] &= _remaining[_depth] - 1;   // the element's slot done
            _settle();
            return *this;
        }

        HamtIterator operator++(int) noexcept {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        friend bool operator==(const HamtIterator& a, const HamtIterator& b) noexcept {
            return a._entry == b._entry && a._chain == b._chain;
        }

    private:
        static constexpr unsigned MaxDepth = (sizeof(size_t) * 8 + 4) / 5;   // levels of five bits in a hash

        // To the next element entry: the lowest slot still to visit at
        // this depth, down into it when it is a subtrie (its slot done
        // for when the walk comes back up), up when the node is done
        void _settle() noexcept {
            for (;;) {
                auto node = _nodes[_depth];
                auto remaining = _remaining[_depth];
                if (remaining) {
                    auto bit = remaining & (0u - remaining);
                    auto& entry = hamt_entries<V>(node)[std::popcount(node->bitmap) - std::popcount(remaining)];
                    if (node->subtries & bit) {
                        _remaining[_depth] = remaining & (remaining - 1);
                        ++_depth;
                        _nodes[_depth] = entry.link.get();
                        _remaining[_depth] = _nodes[_depth]->bitmap;
                        continue;
                    }
                    _entry = &entry;
                    return;
                }
                if (_depth == 0) {
                    _entry = nullptr;   // the end
                    return;
                }
                --_depth;
            }
        }

        const HamtHead* _nodes[MaxDepth] = {};
        uint32_t _remaining[MaxDepth] = {};   // per level, the slots not visited yet, the current one first
        unsigned _depth = 0;
        const Entry* _entry = nullptr;   // the element's entry; null at the end
        const Chain* _chain = nullptr;   // the chain node, when the element is in a chain
    };

    // The trie with the operations of a persistent map or set: Traits
    // gives the key type, the element type, the hash, the equality and
    // how the key is read from an element. The container holds the size
    // and the root; every operation that changes something returns the
    // new root and leaves the old trie as it was.
    template<class Traits>
    class Hamt {
    public:
        using key_type = typename Traits::key_type;
        using value_type = typename Traits::value_type;
        using hasher = typename Traits::hasher;
        using key_equal = typename Traits::key_equal;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using const_iterator = HamtIterator<value_type>;
        using iterator = const_iterator;

        Hamt() = default;

        Hamt(const hasher& hash, const key_equal& equal)
        : _hash(hash)
        , _equal(equal) {
        }

        // The trie of a range, built at once (the constructors from a
        // range and a list) rather than by an insert per element, each
        // of which copies the path to its element: the elements are
        // taken into a buffer, their order of the trie computed (a sort
        // by the hash with its chunks reversed, since the trie consumes
        // the low chunk first), a key that occurs twice keeping its last
        // occurrence, as the later insert would replace the earlier, and
        // the trie made from the root down, every node allocated once at
        // its size and every entry constructed once, in slot order.
        template<class It>
        Hamt(It first, It last, const hasher& hash, const key_equal& equal)
        : _hash(hash)
        , _equal(equal) {
            vector<value_type> items;   // the elements may hold tracked pointers: a managed buffer
            for (; first != last; ++first) {
                items.emplace_back(*first);
            }
            vector<Placed> order;
            order.reserve(items.size());
            for (size_t i = 0; i < items.size(); ++i) {
                auto h = _hash(Traits::key(items[i]));
                order.push_back(Placed{_trie_order(h), h, i});
            }
            std::sort(order.begin(), order.end(), [](const Placed& a, const Placed& b) noexcept {
                return a.order != b.order ? a.order < b.order : a.index < b.index;
            });
            size_t kept = 0;   // the elements of one hash are adjacent, in their order: of two with one key the later stays
            for (size_t i = 0; i < order.size();) {
                size_t j = i;
                while (j < order.size() && order[j].hash == order[i].hash) {
                    ++j;
                }
                for (size_t a = i; a < j; ++a) {
                    bool later = false;
                    for (size_t b = a + 1; b < j && !later; ++b) {
                        later = _equal(Traits::key(items[order[a].index]), Traits::key(items[order[b].index]));
                    }
                    if (!later) {
                        order[kept++] = order[a];
                    }
                }
                i = j;
            }
            _size = kept;
            if (kept) {
                _root = _build(items, order.begin(), order.begin() + kept, 0);
            }
        }

        Hamt(const Hamt&) noexcept = default;
        Hamt(Hamt&&) noexcept = default;
        Hamt& operator=(const Hamt&) noexcept = default;
        Hamt& operator=(Hamt&&) noexcept = default;

        const_iterator begin() const noexcept {
            return const_iterator(_root.get());
        }

        const_iterator end() const noexcept {
            return const_iterator();
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        size_type size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        hasher hash_function() const {
            return _hash;
        }

        key_equal key_eq() const {
            return _equal;
        }

        // The element under the key, null when there is none: the bits of
        // the hash walked down, the key compared at the entry they lead to
        template<class K>
        const value_type* find(const K& key) const {
            const HamtHead* node = _root.get();
            if (!node) {
                return nullptr;
            }
            auto hash = _hash(key);
            for (unsigned shift = 0;; shift += Bits) {
                auto bit = _bit(hash, shift);
                if (!(node->bitmap & bit)) {
                    return nullptr;
                }
                auto& entry = hamt_entries<value_type>(node)[_position(node->bitmap, bit)];
                if (node->subtries & bit) {
                    node = entry.link.get();
                    continue;
                }
                if (_equal(key, Traits::key(entry.value))) {
                    return &entry.value;
                }
                for (auto chain = _chain(entry.link.get()); chain; chain = chain->next.get()) {
                    if (_equal(key, Traits::key(chain->value))) {
                        return &chain->value;
                    }
                }
                return nullptr;
            }
        }

        template<class K>
        bool contains(const K& key) const {
            return find(key) != nullptr;
        }

        // The trie with an element built from a... under `key` (the key
        // the element will have), in place of the one there if any:
        // the path to it copied, the rest shared
        template<class K, class... A>
        Hamt insert(const K& key, A&&... a) const {
            auto hash = _hash(key);
            Hamt next(*this);
            bool added = false;
            if (_root) {
                next._root = _insert(*_root, 0, hash, key, added, std::forward<A>(a)...);
            } else {
                added = true;
                next._root = _make(_bit(hash, 0), 0, [&](Entry* at, uint32_t) {
                    _element(at, nullptr, std::forward<A>(a)...);
                });
            }
            next._size += added;
            return next;
        }

        // The trie without the element under the key: the path copied,
        // a node emptied dropped, a subtrie left with one element folded
        // into its parent; the same trie when the key is absent
        template<class K>
        Hamt erase(const K& key) const {
            if (!_root) {
                return *this;
            }
            bool removed = false;
            Hamt next(*this);
            next._root = _erase(_root, 0, _hash(key), key, removed);
            next._size -= removed;
            return next;
        }

        // The same elements under the same keys, whatever the two tries look like
        template<class Eq>
        bool equals(const Hamt& other, Eq&& eq) const {
            if (_root == other._root) {   // the same trie: a version and its copy, two snapshots of one value
                return true;
            }
            if (_size != other._size) {
                return false;
            }
            for (auto& v : *this) {
                auto o = other.find(Traits::key(v));
                if (!o || !eq(v, *o)) {
                    return false;
                }
            }
            return true;
        }

    private:
        using Entry = HamtEntry<value_type>;
        using Chain = HamtChain<value_type>;
        using Link = tracked_ptr<HamtHead>;
        using Node = unique_ptr<HamtHead>;   // a node being built: deterministic until it is linked

        static constexpr unsigned Bits = 5;
        static constexpr unsigned MaxShift = sizeof(size_t) * 8 - 1;

        static uint32_t _bit(size_t hash, unsigned shift) noexcept {
            assert(shift <= MaxShift);
            return uint32_t(1) << ((hash >> shift) & 31);
        }

        static unsigned _position(uint32_t bitmap, uint32_t bit) noexcept {
            return std::popcount(bitmap & (bit - 1));
        }

        static const Chain* _chain(const HamtHead* link) noexcept {
            return static_cast<const Chain*>(link);
        }

        // An element placed for the build from a range: its position in
        // the trie's order, its hash and its index in the buffer
        struct Placed {
            size_t order;
            size_t hash;
            size_t index;
        };

        // The hash with its chunks in the trie's order: the low chunk,
        // which the root consumes, highest, so that hashes sorted by this
        // are elements in the order of the trie's slots at every level
        static size_t _trie_order(size_t hash) noexcept {
            constexpr unsigned Width = sizeof(size_t) * 8;
            size_t order = 0;
            unsigned shift = 0;
            for (; shift + Bits <= Width; shift += Bits) {
                order = (order << Bits) | ((hash >> shift) & 31);
            }
            return (order << (Width - shift)) | (hash >> shift);   // the last chunk, the bits left
        }

        // The node at `shift` over the elements [first, last), sorted in
        // the trie's order: a slot per chunk value, holding the element
        // when the chunk is one element's, the element with the chain of
        // the others when they share the whole hash, a subtrie built
        // below otherwise. The elements are moved out of the buffer.
        template<class P>
        static Node _build(vector<value_type>& items, P first, P last, unsigned shift) {
            uint32_t bitmap = 0, subtries = 0;
            for (auto p = first; p != last;) {
                auto bit = _bit(p->hash, shift);
                auto q = p + 1;
                while (q != last && _bit(q->hash, shift) == bit) {
                    ++q;
                }
                bitmap |= bit;
                if ((q - 1)->hash != p->hash) {   // sorted: the first and the last apart means not all one hash
                    subtries |= bit;
                }
                p = q;
            }
            auto p = first;
            return _make(bitmap, subtries, [&](Entry* at, uint32_t bit) {
                auto q = p + 1;
                while (q != last && _bit(q->hash, shift) == bit) {
                    ++q;
                }
                if (subtries & bit) {
                    Link sub = _build(items, p, q, shift + Bits);
                    _subtrie(at, sub);
                } else {
                    Link chain;
                    for (auto r = p + 1; r != q; ++r) {
                        chain = make_tracked<Chain>(static_pointer_cast<Chain>(chain), std::move(items[r->index]));
                    }
                    _element(at, chain, std::move(items[p->index]));
                }
                p = q;
            });
        }

        // A node with room for `count` entries: the smallest size that holds them
        template<unsigned N>
        static Node _allocate() {
            return make_tracked<HamtNode<value_type, N>>();
        }

        static Node _allocate(unsigned count) {
            if (count <= 1) return _allocate<1>();
            if (count <= 2) return _allocate<2>();
            if (count <= 4) return _allocate<4>();
            if (count <= 8) return _allocate<8>();
            if (count <= 16) return _allocate<16>();
            return _allocate<32>();
        }

        // A node of the given layout, its entries constructed in slot order
        // by fill(entry storage, bit); `built` follows each, so that an
        // exception leaves a node whose destructor knows what to destroy
        template<class F>
        static Node _make(uint32_t bitmap, uint32_t subtries, F&& fill) {
            Node node = _allocate(std::popcount(bitmap));
            node->bitmap = bitmap;
            node->subtries = subtries;
            auto entries = hamt_entries<value_type>(node.get());
            auto bits = bitmap;
            for (unsigned i = 0; bits; ++i) {
                auto bit = bits & (0u - bits);
                bits &= bits - 1;
                fill(entries + i, bit);
                ++node->built;
            }
            return node;
        }

        // An entry constructed in its storage: a subtrie, or an element
        // built from a... with its chain
        static void _subtrie(Entry* at, const Link& sub) {
            ::new (static_cast<void*>(at)) Entry(sub);
        }

        template<class... A>
        static void _element(Entry* at, const Link& chain, A&&... a) {
            ::new (static_cast<void*>(at)) Entry(chain, std::in_place, std::forward<A>(a)...);
        }

        // The entry of slot `bit` of `node`, whose entries are `from`,
        // copied into `at`
        static void _copy(Entry* at, const HamtHead& node, const Entry* from, uint32_t bit) {
            auto& entry = from[_position(node.bitmap, bit)];
            if (node.subtries & bit) {
                _subtrie(at, entry.link);
            } else {
                _element(at, entry.link, entry.value);
            }
        }

        // The node with an element added in the free slot `bit`
        template<class... A>
        static Node _copy_adding(const HamtHead& node, uint32_t bit, A&&... a) {
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap | bit, node.subtries, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _element(at, nullptr, std::forward<A>(a)...);
                } else {
                    _copy(at, node, from, b);
                }
            });
        }

        // The node with the element of slot `bit` replaced by one built
        // from a..., its chain being `chain`
        template<class... A>
        static Node _copy_replacing(const HamtHead& node, uint32_t bit, const Link& chain, A&&... a) {
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap, node.subtries & ~bit, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _element(at, chain, std::forward<A>(a)...);
                } else {
                    _copy(at, node, from, b);
                }
            });
        }

        // The node with slot `bit` holding the subtrie `sub`
        static Node _copy_linking(const HamtHead& node, uint32_t bit, const Link& sub) {
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap, node.subtries | bit, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _subtrie(at, sub);
                } else {
                    _copy(at, node, from, b);
                }
            });
        }

        // The node with the element of slot `bit` kept and its chain
        // replaced by `chain`
        static Node _copy_chaining(const HamtHead& node, uint32_t bit, const Link& chain) {
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap, node.subtries, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _element(at, chain, from[_position(node.bitmap, bit)].value);
                } else {
                    _copy(at, node, from, b);
                }
            });
        }

        // The node without slot `bit`; null when it was the last one
        static Node _copy_removing(const HamtHead& node, uint32_t bit) {
            if (node.bitmap == bit) {
                return nullptr;
            }
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap & ~bit, node.subtries & ~bit, [&](Entry* at, uint32_t b) {
                _copy(at, node, from, b);
            });
        }

        // The node with slot `bit` holding the element (and chain) of
        // `entry`, an element pulled up out of a subtrie
        static Node _copy_hoisting(const HamtHead& node, uint32_t bit, const Entry& entry) {
            auto from = hamt_entries<value_type>(&node);
            return _make(node.bitmap, node.subtries & ~bit, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _element(at, entry.link, entry.value);
                } else {
                    _copy(at, node, from, b);
                }
            });
        }

        // A subtrie at `shift` holding the element of `entry` (hash
        // `ehash`, its chain kept) and a new element built from a...
        // (hash `hash`): one node when their bits at this level differ,
        // a node over another when they agree
        template<class... A>
        static Node _split(unsigned shift, const Entry& entry, size_t ehash, size_t hash, A&&... a) {
            auto ebit = _bit(ehash, shift);
            auto bit = _bit(hash, shift);
            if (ebit == bit) {
                Link sub = _split(shift + Bits, entry, ehash, hash, std::forward<A>(a)...);
                return _make(bit, bit, [&](Entry* at, uint32_t) {
                    _subtrie(at, sub);
                });
            }
            return _make(ebit | bit, 0, [&](Entry* at, uint32_t b) {
                if (b == bit) {
                    _element(at, nullptr, std::forward<A>(a)...);
                } else {
                    _element(at, entry.link, entry.value);
                }
            });
        }

        // The chain with the element under `key` replaced by one built
        // from a...: the nodes before it copied, the rest shared; null
        // when the key is not in it
        template<class K, class... A>
        Link _chain_replacing(const Chain* chain, const K& key, A&&... a) const {
            if (!chain) {
                return nullptr;
            }
            if (_equal(key, Traits::key(chain->value))) {
                return make_tracked<Chain>(chain->next, std::forward<A>(a)...);
            }
            Link rest = _chain_replacing(chain->next.get(), key, std::forward<A>(a)...);
            if (!rest) {
                return nullptr;
            }
            return make_tracked<Chain>(static_pointer_cast<Chain>(rest), chain->value);
        }

        // The chain without the element under `key`; `removed` whether
        // it was there (the chain may become empty: null)
        template<class K>
        Link _chain_removing(const Chain* chain, const K& key, bool& removed) const {
            if (!chain) {
                removed = false;
                return nullptr;
            }
            if (_equal(key, Traits::key(chain->value))) {
                removed = true;
                return chain->next;
            }
            Link rest = _chain_removing(chain->next.get(), key, removed);
            if (!removed) {
                return nullptr;
            }
            return make_tracked<Chain>(static_pointer_cast<Chain>(rest), chain->value);
        }

        // The node with an element built from a... under `key`: added
        // (`added`) or replacing the one under the key
        template<class K, class... A>
        Node _insert(const HamtHead& node, unsigned shift, size_t hash, const K& key, bool& added, A&&... a) const {
            auto bit = _bit(hash, shift);
            if (!(node.bitmap & bit)) {
                added = true;
                return _copy_adding(node, bit, std::forward<A>(a)...);
            }
            auto& entry = hamt_entries<value_type>(&node)[_position(node.bitmap, bit)];
            if (node.subtries & bit) {
                Link sub = _insert(*entry.link, shift + Bits, hash, key, added, std::forward<A>(a)...);
                return _copy_linking(node, bit, sub);
            }
            if (_equal(key, Traits::key(entry.value))) {
                added = false;
                return _copy_replacing(node, bit, entry.link, std::forward<A>(a)...);
            }
            auto ehash = _hash(Traits::key(entry.value));
            if (ehash != hash) {   // apart from here on: a subtrie in place of the element
                added = true;
                Link sub = _split(shift + Bits, entry, ehash, hash, std::forward<A>(a)...);
                return _copy_linking(node, bit, sub);
            }
            // the same hash to the last bit: the chain of the entry
            if (Link chain = _chain_replacing(_chain(entry.link.get()), key, std::forward<A>(a)...)) {
                added = false;
                return _copy_chaining(node, bit, chain);
            }
            added = true;
            Link chain = make_tracked<Chain>(static_pointer_cast<Chain>(entry.link), std::forward<A>(a)...);
            return _copy_chaining(node, bit, chain);
        }

        // The node without the element under `key` (`removed` whether it
        // was there): the same node when it was not, null when the node
        // is emptied
        template<class K>
        Link _erase(const Link& self, unsigned shift, size_t hash, const K& key, bool& removed) const {
            auto& node = *self;
            auto bit = _bit(hash, shift);
            if (!(node.bitmap & bit)) {
                removed = false;
                return self;
            }
            auto& entry = hamt_entries<value_type>(&node)[_position(node.bitmap, bit)];
            if (node.subtries & bit) {
                Link sub = _erase(entry.link, shift + Bits, hash, key, removed);
                if (!removed) {
                    return self;
                }
                if (!sub) {
                    return _copy_removing(node, bit);
                }
                if (std::popcount(sub->bitmap) == 1 && !sub->subtries) {   // one element left below: it takes the slot
                    return _copy_hoisting(node, bit, hamt_entries<value_type>(sub.get())[0]);
                }
                return _copy_linking(node, bit, sub);
            }
            if (_equal(key, Traits::key(entry.value))) {
                removed = true;
                if (auto chain = _chain(entry.link.get())) {   // the chain's first element takes the entry
                    return _copy_replacing(node, bit, chain->next, chain->value);
                }
                return _copy_removing(node, bit);
            }
            Link chain = _chain_removing(_chain(entry.link.get()), key, removed);
            if (!removed) {
                return self;
            }
            return _copy_chaining(node, bit, chain);
        }

        size_t _size = 0;
        Link _root;   // null when empty
        [[no_unique_address]] hasher _hash;
        [[no_unique_address]] key_equal _equal;
    };
}
