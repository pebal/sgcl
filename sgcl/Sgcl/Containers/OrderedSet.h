//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// OrderedSet<T>: a HashSet iterated in the order the values were added
// (Java's LinkedHashSet). First is the oldest value and Last the newest,
// a copy keeps the order, an Add of a present value leaves it where it
// was, MoveToLast and MoveToFirst move a value in the order.
#pragma once

#include "../../containers/ordered_set.h"
#include "detail/Associative.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class OrderedSet : public detail::Set<sgcl::ordered_set<T, Hash, Equal>> {
        using Base = detail::Set<sgcl::ordered_set<T, Hash, Equal>>;

    public:
        using typename Base::InnerType;
        using typename Base::SizeType;
        using typename Base::ValueType;
        using typename Base::ConstIterator;

        OrderedSet() = default;

        template<std::input_iterator It>
        OrderedSet(It first, It last)
        : Base(InnerType(first, last)) {
        }

        OrderedSet(std::initializer_list<typename InnerType::value_type> il)
        : Base(InnerType(il)) {
        }

        explicit OrderedSet(InnerType c) noexcept
        : Base(std::move(c)) {
        }

        explicit OrderedSet(SizeType buckets)
        : Base(InnerType(buckets)) {
        }

        OrderedSet(const OrderedSet&) = default;
        OrderedSet(OrderedSet&&) noexcept = default;
        OrderedSet& operator=(const OrderedSet&) = default;
        OrderedSet& operator=(OrderedSet&&) noexcept = default;

        // The oldest and the newest value (the set not empty)
        const ValueType& First() const noexcept {
            return this->_c.front();
        }

        const ValueType& Last() const noexcept {
            return this->_c.back();
        }

        // The oldest (the newest) value removed: whether there was one
        bool RemoveFirst() {
            if (this->_c.empty()) {
                return false;
            }
            this->_c.erase(this->_c.begin());
            return true;
        }

        bool RemoveLast() {
            if (this->_c.empty()) {
                return false;
            }
            this->_c.erase(std::prev(this->_c.end()));
            return true;
        }

        // The value moved to the end (the start) of the order: as if added
        // last (first) from now on; by its iterator (FindEntry) or by the
        // value, then whether it was there
        void MoveToLast(ConstIterator pos) noexcept {
            this->_c.to_back(pos);
        }

        void MoveToFirst(ConstIterator pos) noexcept {
            this->_c.to_front(pos);
        }

        template<class K = T> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool MoveToLast(const K& value) noexcept {
            auto it = this->_c.find(value);
            if (it == this->_c.end()) {
                return false;
            }
            this->_c.to_back(it);
            return true;
        }

        template<class K = T> requires (!std::is_convertible_v<const K&, ConstIterator>)
        bool MoveToFirst(const K& value) noexcept {
            auto it = this->_c.find(value);
            if (it == this->_c.end()) {
                return false;
            }
            this->_c.to_front(it);
            return true;
        }

        // Room for `n` values without a rehash
        void Reserve(SizeType n) {
            this->_c.reserve(n);
        }

        // The buckets: a power of two, doubled when the load reaches the maximum
        SizeType BucketCount() const noexcept {
            return this->_c.bucket_count();
        }

        float LoadFactor() const noexcept {
            return this->_c.load_factor();
        }

        float MaxLoadFactor() const noexcept {
            return this->_c.max_load_factor();
        }

        void MaxLoadFactor(float z) {
            this->_c.max_load_factor(z);
        }

        void Rehash(SizeType buckets) {
            this->_c.rehash(buckets);
        }

        Hash HashFunction() const {
            return this->_c.hash_function();
        }

        Equal KeyEqual() const {
            return this->_c.key_eq();
        }
    };

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    OrderedSet(std::initializer_list<T>, size_t = 0, Hash = Hash(), Equal = Equal()) -> OrderedSet<T, Hash, Equal>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
