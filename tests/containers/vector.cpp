//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <vector>

// Running out of memory ends the program: an insertion throws only what
// the element's construction, copy or move throws (DESIGN 356)
namespace {
    struct ThrowingCopy {
        ThrowingCopy() = default;
        ThrowingCopy(const ThrowingCopy&) {}
        ThrowingCopy& operator=(const ThrowingCopy&) { return *this; }
    };
}
static_assert(noexcept(std::declval<sgcl::vector<int>&>().push_back(1)));
static_assert(noexcept(std::declval<sgcl::vector<int>&>().emplace_back()));
static_assert(noexcept(std::declval<sgcl::vector<int>&>().insert(std::declval<sgcl::vector<int>&>().cbegin(), 1)));
static_assert(noexcept(std::declval<sgcl::vector<int>&>().erase(std::declval<sgcl::vector<int>&>().cbegin())));
static_assert(noexcept(std::declval<sgcl::vector<int>&>().shrink_to_fit()));
static_assert(noexcept(std::declval<sgcl::vector<std::string>&>().push_back(std::string())));
static_assert(!noexcept(std::declval<sgcl::vector<std::string>&>().push_back(std::declval<const std::string&>())));
static_assert(!noexcept(std::declval<sgcl::vector<ThrowingCopy>&>().push_back(ThrowingCopy())));   // moved by its throwing copy
static_assert(!noexcept(std::declval<sgcl::vector<int>&>().reserve(1)));   // length_error for a size past max_size

TEST(Vector_Test, DefaultConstructorEmpty) {
    sgcl::vector<Int> vec;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.cbegin(), vec.cend());
    EXPECT_EQ(vec.rbegin(), vec.rend());
    EXPECT_EQ(vec.crbegin(), vec.crend());
}

TEST(Vector_Test, ConstructorNDefault) {
    sgcl::vector<Int> vec(3);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {0, 0, 0};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, ConstructorNValues) {
    sgcl::vector<Int> vec(4, 3);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 4u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {3, 3, 3, 3};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, List) {
    sgcl::vector<Int> vec({4, 5, 6});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, ConstructorRange) {
    std::vector<int> expected = {1, 2, 3};
    sgcl::vector<Int> vec(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, CopyConstructor) {
    sgcl::vector<Int> other({1, 2, 3});
    sgcl::vector<Int> vec(other);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, MoveConstructor) {
    sgcl::vector<Int> other({4, 5, 6});
    sgcl::vector<Int> vec(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, CopyAssignment) {
    sgcl::vector<Int> other({1, 2, 3});
    sgcl::vector<Int> vec;
    vec = other;
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({3, 2, 1});
    EXPECT_EQ(result, expected);

    other = sgcl::vector<Int>({2, 3});
    vec = other;
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 2u);
    result = std::vector<int>(vec.begin(), vec.end());
    expected = std::vector<int>({2, 3});
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({3, 2});
    EXPECT_EQ(result, expected);

    other = sgcl::vector<Int>();
    vec = other;
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.rbegin(), vec.rend());
}

TEST(Vector_Test, MoveAssignment) {
    sgcl::vector<Int> other({4, 5, 6});
    sgcl::vector<Int> vec;
    vec = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, ListAssignment) {
    sgcl::vector<Int> vec;
    vec = {7, 8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {7, 8, 9};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({9, 8, 7});
    EXPECT_EQ(result, expected);

    vec = {8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    result = std::vector<int>(vec.begin(), vec.end());
    expected = std::vector<int>({8, 9});
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({9, 8});
    EXPECT_EQ(result, expected);

    vec = std::initializer_list<Int>();
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.rbegin(), vec.rend());
}

TEST(Vector_Test, IndexOperator) {
    sgcl::vector<Int> vec({1, 2, 3, 4, 5, 6, 7});
    for (int i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i], i + 1);
    }
}

TEST(Vector_Test, at) {
    sgcl::vector<Int> vec({0, 1, 2, 3, 4});
    for (int i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.at(i), i);
    }
    EXPECT_THROW(vec.at(10), std::out_of_range);
}

TEST(Vector_Test, AssignNValues) {
    sgcl::vector<Int> vec({1, 2});
    vec.assign(4, 5);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {5, 5, 5, 5};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    EXPECT_EQ(result, expected);

    vec.assign(2, 3);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    result = std::vector<int>(vec.begin(), vec.end());
    expected = {3, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    EXPECT_EQ(result, expected);

    vec.assign(0, 0);
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.rbegin(), vec.rend());
}

TEST(Vector_Test, AssignRange) {
    sgcl::vector<Int> vec;
    std::vector<int> expected = {3, 4, 5};
    vec.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.begin(), vec.end());
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({5, 4, 3});
    EXPECT_EQ(result, expected);

    expected = {2, 3, 4, 5};
    vec.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    result = std::vector<int>(vec.begin(), vec.end());
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.rbegin(), vec.rend());
    expected = std::vector<int>({5, 4, 3, 2});
    EXPECT_EQ(result, expected);

    vec.assign(expected.begin(), expected.begin());
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.rbegin(), vec.rend());
}

TEST(Vector_Test, AssignList) {
    sgcl::vector<Int> vec({1, 2});
    vec.assign({3, 4, 5});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    std::vector<int> result(vec.cbegin(), vec.cend());
    std::vector<int> expected = {3, 4, 5};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.crbegin(), vec.crend());
    expected = std::vector<int>({5, 4, 3});
    EXPECT_EQ(result, expected);

    vec.assign({2, 3});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    result = std::vector<int>(vec.cbegin(), vec.cend());
    expected = {2, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec.crbegin(), vec.crend());
    expected = std::vector<int>({3, 2});
    EXPECT_EQ(result, expected);

    vec.assign(std::initializer_list<Int>());
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.cbegin(), vec.cend());
    EXPECT_EQ(vec.crbegin(), vec.crend());
}

TEST(Vector_Test, Front) {
    sgcl::vector<int> vec({5, 6, 7});
    EXPECT_EQ(vec.front(), 5);
}

TEST(Vector_Test, Back) {
    sgcl::vector<int> vec({5, 6, 7});
    EXPECT_EQ(vec.back(), 7);
}

TEST(Vector_Test, PushAndEmplaceBack) {
    sgcl::vector<Int> vec({1, 2, 3});
    vec.push_back(4);
    auto v = vec.emplace_back(5);
    EXPECT_EQ(v, 5);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, PopBack) {
    sgcl::vector<Int> vec({2, 3, 4});
    vec.pop_back();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {2, 3};
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(result, expected);
}

// A reallocating insert leaves references to the old buffer in the frame
// that performed it (arguments, element references for the checks), and the
// stack is scanned conservatively: the insert and the element checks run in
// a frame of their own, the counts are taken from the test's frame.
TEST(Vector_Test, Emplace) {
    sgcl::vector<Int> vec({2, 3, 5});
    sgcl::vector<Int>::iterator it;
    off_frame([&] {
        it = vec.emplace(vec.begin(), 1);
        EXPECT_EQ(*it, 1);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    off_frame([&] {
        it = vec.emplace(vec.begin() + 3, 4);
        EXPECT_EQ(*it, 4);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    off_frame([&] {
        it = vec.emplace(vec.end(), 6);
        EXPECT_EQ(*it, 6);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(vec.size(), 6u);
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertValue) {
    sgcl::vector<Int> vec({2, 3, 5});
    sgcl::vector<Int>::iterator it;
    off_frame([&] {
        it = vec.insert(vec.begin(), 1);
        EXPECT_EQ(*it, 1);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    off_frame([&] {
        it = vec.insert(vec.begin() + 3, 4);
        EXPECT_EQ(*it, 4);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    off_frame([&] {
        it = vec.insert(vec.end(), 6);
        EXPECT_EQ(*it, 6);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(vec.size(), 6u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertNValues) {
    sgcl::vector<Int> vec({2, 3, 5});
    sgcl::vector<Int>::iterator it;
    off_frame([&] {
        it = vec.insert(vec.begin(), 0, 1);
        EXPECT_EQ(it, vec.begin());
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    off_frame([&] {
        it = vec.insert(vec.begin(), 2, 1);
        EXPECT_EQ(*it, 1);
        EXPECT_EQ(*++it, 1);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    off_frame([&] {
        it = vec.insert(vec.begin() + 4, 2, 4);
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(*++it, 4);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    off_frame([&] {
        it = vec.insert(vec.end(), 2, 6);
        EXPECT_EQ(*it, 6);
        EXPECT_EQ(*++it, 6);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 1, 2, 3, 4, 4, 5, 6, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertRange) {
    sgcl::vector<Int> vec({2, 3, 5});
    sgcl::vector<Int>::iterator it;
    std::vector<int> other = {4, 5};
    off_frame([&] {
        it = vec.insert(vec.end(), other.begin(), other.begin());
        EXPECT_EQ(it, vec.end());
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    off_frame([&] {
        it = vec.insert(vec.begin(), other.begin(), other.end());
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(*++it, 5);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    other = std::vector<int>{7, 8};
    off_frame([&] {
        it = vec.insert(vec.begin() + 4, other.begin(), other.end());
        EXPECT_EQ(*it, 7);
        EXPECT_EQ(*++it, 8);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    other = std::vector<int>{2, 3};
    off_frame([&] {
        it = vec.insert(vec.end(), other.begin(), other.end());
        EXPECT_EQ(*it, 2);
        EXPECT_EQ(*++it, 3);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {4, 5, 2, 3, 7, 8, 5, 2, 3};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertList) {
    sgcl::vector<Int> vec({2, 3, 5});
    sgcl::vector<Int>::iterator it;
    off_frame([&] {
        it = vec.insert(vec.end(), std::initializer_list<Int>());
        EXPECT_EQ(it, vec.end());
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    off_frame([&] {
        it = vec.insert(vec.begin(), {5, 4});
        EXPECT_EQ(*it, 5);
        EXPECT_EQ(*++it, 4);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    off_frame([&] {
        it = vec.insert(vec.begin() + 4, {8, 7});
        EXPECT_EQ(*it, 8);
        EXPECT_EQ(*++it, 7);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    off_frame([&] {
        it = vec.insert(vec.end(), {3, 2});
        EXPECT_EQ(*it, 3);
        EXPECT_EQ(*++it, 2);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {5, 4, 2, 3, 8, 7, 5, 3, 2};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, Erase) {
    sgcl::vector<Int> vec({1, 2, 3, 4, 5});
    auto it = vec.erase(vec.begin());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(*it, 2);
    it = vec.erase(++vec.begin());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(*it, 4);
    it = vec.erase(--vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(it, vec.end());
    it = vec.erase(vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(it, vec.end());
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, EraseRange) {
    sgcl::vector<Int> vec({1, 2, 3, 4, 5, 6, 7, 8});
    auto it = vec.erase(vec.begin(), vec.begin() + 2);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(vec.size(), 6u);
    EXPECT_EQ(*it, 3);
    it = vec.erase(vec.begin() + 1, vec.begin() + 3);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(*it, 6);
    it = vec.erase(vec.end() - 2, vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(it, vec.end());
    it = vec.erase(vec.end(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(it, vec.end());
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {3, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, Resize) {
    sgcl::vector<Int> vec({1, 2, 3});
    vec.resize(5);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 0, 0};
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(result, expected);
    vec.resize(2);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(vec.size(), 2u);
    result = std::vector<int>(vec.begin(), vec.end());
    expected = std::vector<int>({1, 2});
    EXPECT_EQ(result, expected);
    vec.resize(4, 8);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    result = std::vector<int>(vec.begin(), vec.end());
    expected = std::vector<int>({1, 2, 8, 8});
    EXPECT_EQ(result, expected);
    vec.resize(0);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, Swap) {
    sgcl::vector<Int> vec1({1, 2});
    sgcl::vector<Int> vec2({4, 5, 6, 7});
    vec1.swap(vec2);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(vec1.size(), 4);
    EXPECT_EQ(vec2.size(), 2);
    std::vector<int> result(vec1.begin(), vec1.end());
    std::vector<int> expected = {4, 5, 6, 7};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(vec2.begin(), vec2.end());
    expected = std::vector<int>({1, 2});
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, Clear) {
    sgcl::vector<Int> vec({5, 6, 7});
    vec.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the buffer stays, like std::vector's capacity
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.cbegin(), vec.cend());
    EXPECT_EQ(vec.rbegin(), vec.rend());
    EXPECT_EQ(vec.crbegin(), vec.crend());
}

TEST(Vector_Test, ShrinkToFit) {
    sgcl::vector<Int> vec(20);
    EXPECT_EQ(Int::counter, 20u);
    vec.assign({5, 6, 7});
    vec.shrink_to_fit();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.capacity(), 4u);
}

// The cases the review found: element types with real semantics.
namespace {
    struct Counted {
        static inline int alive = 0;
        static inline int throw_at = -1;   // the n-th construction throws
        static inline int constructed = 0;
        std::string s;
        bool moved = false;   // a moved-from object left in an old buffer is not counted
        Counted(std::string v = "") : s(std::move(v)) { if (constructed++ == throw_at) { throw std::runtime_error("ctor"); } ++alive; }
        Counted(const Counted& o) : Counted(o.s) {}
        Counted(Counted&& o) noexcept : s(std::move(o.s)) { ++alive; o.give_up(); }
        Counted& operator=(const Counted& o) { s = o.s; revive(); return *this; }
        Counted& operator=(Counted&& o) noexcept { s = std::move(o.s); revive(); o.give_up(); return *this; }
        ~Counted() { give_up(); }
        void give_up() { if (!moved) { moved = true; --alive; } }   // a moved-from object no longer counts
        void revive() { if (moved) { moved = false; ++alive; } }
        bool operator==(const Counted& o) const { return s == o.s; }
    };

    struct MoveOnly {
        std::unique_ptr<int> p;
        MoveOnly() : p(std::make_unique<int>(0)) {}
        explicit MoveOnly(int v) : p(std::make_unique<int>(v)) {}
        MoveOnly(MoveOnly&&) noexcept = default;
        MoveOnly& operator=(MoveOnly&&) noexcept = default;
    };

    struct alignas(16) Wide {
        float x[4];
    };
}

static_assert(std::contiguous_iterator<sgcl::vector<int>::iterator>);
static_assert(std::contiguous_iterator<sgcl::vector<int>::const_iterator>);
static_assert(std::ranges::contiguous_range<sgcl::vector<int>>);

TEST(Vector_Test, FillInsertInPlaceLongerThanTail) {
    sgcl::vector<std::string> v;
    v.reserve(16);
    v = {"a", "b", "c"};
    v.insert(v.begin() + 1, 5, "x");
    std::vector<std::string> expected = {"a", "x", "x", "x", "x", "x", "b", "c"};
    EXPECT_EQ(std::vector<std::string>(v.begin(), v.end()), expected);
    v.insert(v.begin() + 7, 2, "y");   // tail of one, two copies
    expected = {"a", "x", "x", "x", "x", "x", "b", "y", "y", "c"};
    EXPECT_EQ(std::vector<std::string>(v.begin(), v.end()), expected);
    v.insert(v.begin() + 2, 1, "z");   // tail longer than the count
    expected = {"a", "x", "z", "x", "x", "x", "x", "b", "y", "y", "c"};
    EXPECT_EQ(std::vector<std::string>(v.begin(), v.end()), expected);
}

TEST(Vector_Test, RangeInsertInPlaceAndReallocating) {
    sgcl::vector<std::string> v = {"a", "b", "c"};
    std::vector<std::string> src = {"1", "2", "3", "4"};
    v.insert(v.begin() + 1, src.begin(), src.end());   // reallocates
    std::vector<std::string> expected = {"a", "1", "2", "3", "4", "b", "c"};
    EXPECT_EQ(std::vector<std::string>(v.begin(), v.end()), expected);
    v.reserve(32);
    v.insert(v.end() - 1, src.begin(), src.end());     // in place, tail of one
    expected = {"a", "1", "2", "3", "4", "b", "1", "2", "3", "4", "c"};
    EXPECT_EQ(std::vector<std::string>(v.begin(), v.end()), expected);
    v.insert(v.begin(), {"p", "q"});
    EXPECT_EQ(v[0], "p");
    EXPECT_EQ(v[1], "q");
    EXPECT_EQ(v.size(), 13u);
}

// An append of plain elements from contiguous memory within the capacity
// (one copy, the count raised once): every length across the copy's
// thresholds, from a pointer and from std::vector's iterators, the
// vector's own elements appended to itself, and the position returned
TEST(Vector_Test, PlainAppendWithinCapacity) {
    for (size_t n : {size_t(1), size_t(7), size_t(31), size_t(32), size_t(64), size_t(65), size_t(4095), size_t(4096), size_t(5000)}) {
        std::vector<std::byte> src(n);
        for (size_t i = 0; i < n; ++i) {
            src[i] = std::byte(i * 131 + 7);
        }
        sgcl::vector<std::byte> v;
        v.reserve(3 * n + 3);
        v.push_back(std::byte(1));
        auto capacity = v.capacity();
        auto it = v.insert(v.end(), src.data(), src.data() + n);
        EXPECT_EQ(it, v.begin() + 1);
        it = v.insert(v.end(), src.begin(), src.end());
        EXPECT_EQ(it, v.begin() + 1 + n);
        ASSERT_EQ(v.size(), 1 + 2 * n);
        EXPECT_EQ(v.capacity(), capacity);   // in place
        EXPECT_EQ(v[0], std::byte(1));
        EXPECT_EQ(std::memcmp(v.data() + 1, src.data(), n), 0);
        EXPECT_EQ(std::memcmp(v.data() + 1 + n, src.data(), n), 0);
        v.resize(1 + n);
        v.insert(v.end(), v.data() + 1, v.data() + 1 + n);   // its own elements
        ASSERT_EQ(v.size(), 1 + 2 * n);
        EXPECT_EQ(std::memcmp(v.data() + 1 + n, src.data(), n), 0);
        EXPECT_EQ(v.capacity(), capacity);
    }
    sgcl::vector<int> w{1, 2};
    w.reserve(8);
    std::vector<int> more{3, 4, 5};
    EXPECT_EQ(w.insert(w.end(), more.begin(), more.end()), w.begin() + 2);
    EXPECT_EQ(std::vector<int>(w.begin(), w.end()), (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(Vector_Test, ArgumentsAliasingAnElement) {
    sgcl::vector<std::string> v{"long enough to defeat the small string optimization"};
    const auto original = v[0];
    for (int i = 0; i < 6; ++i) {
        v.push_back(v[0]);   // reallocates several times
    }
    for (auto& s : v) {
        EXPECT_EQ(s, original);
    }
    sgcl::vector<std::string> w{"first string that is long enough", "second string that is long enough"};
    w.insert(w.begin(), w.back());   // in place or not: the value is the old back()
    EXPECT_EQ(w[0], "second string that is long enough");
    w.reserve(16);
    w.insert(w.begin(), w.back());
    EXPECT_EQ(w[0], "second string that is long enough");
    w.emplace(w.begin() + 1, w[0]);
    EXPECT_EQ(w[1], "second string that is long enough");
    w.assign(3, w[0]);
    EXPECT_EQ(w.size(), 3u);
    EXPECT_EQ(w[2], "second string that is long enough");
    w.insert(w.begin(), 4, w[1]);
    EXPECT_EQ(w.size(), 7u);
    EXPECT_EQ(w[3], "second string that is long enough");
}

TEST(Vector_Test, ComparisonsCheckSize) {
    sgcl::vector<int> a = {1, 2};
    sgcl::vector<int> b = {1, 2, 3};
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    sgcl::vector<double> c = {1.0, 2.0};
    sgcl::vector<double> d = {1.0, 2.5};
    EXPECT_TRUE(c < d);
    EXPECT_TRUE((c <=> d) == std::partial_ordering::less);
    sgcl::vector<Counted> e = {Counted("x")};
    sgcl::vector<Counted> f = {Counted("x")};
    EXPECT_TRUE(e == f);
}

TEST(Vector_Test, EraseEmptyRangeIsANoOp) {
    sgcl::vector<std::vector<int>> v;
    v.push_back({1, 2, 3});
    v.push_back({4});
    auto it = v.erase(v.begin(), v.begin());
    EXPECT_EQ(it, v.begin());
    EXPECT_EQ(v[0].size(), 3u);
    EXPECT_EQ(v[1].size(), 1u);
    it = v.erase(v.end(), v.end());
    EXPECT_EQ(it, v.end());
    EXPECT_EQ(v.size(), 2u);
}

TEST(Vector_Test, SinglePassInputIterators) {
    std::istringstream in("1 2 3 4 5");
    sgcl::vector<int> v{std::istream_iterator<int>(in), std::istream_iterator<int>()};
    EXPECT_EQ(std::vector<int>(v.begin(), v.end()), (std::vector<int>{1, 2, 3, 4, 5}));
    std::istringstream in2("7 8");
    v.insert(v.begin() + 1, std::istream_iterator<int>(in2), std::istream_iterator<int>());
    EXPECT_EQ(std::vector<int>(v.begin(), v.end()), (std::vector<int>{1, 7, 8, 2, 3, 4, 5}));
    std::istringstream in3("9");
    v.assign(std::istream_iterator<int>(in3), std::istream_iterator<int>());
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 9);
}

TEST(Vector_Test, OveralignedElements) {
    sgcl::vector<Wide> v(3);
    EXPECT_EQ((uintptr_t)v.data() % 16, 0u);
    for (int i = 0; i < 100; ++i) {
        v.push_back(Wide{{1, 2, 3, float(i)}});
        EXPECT_EQ((uintptr_t)v.data() % 16, 0u);
    }
    EXPECT_EQ(v[102].x[3], 99.0f);
}

TEST(Vector_Test, ThrowingConstructorLeavesTheVectorConsistent) {
    collector::force_collect(true);   // buffers of earlier tests die now, not during the counts below
    Counted::alive = 0;
    Counted::constructed = 0;
    Counted::throw_at = 2;
    EXPECT_THROW(sgcl::vector<Counted> v(5, Counted("a")), std::runtime_error);
    Counted::throw_at = -1;
    collector::force_collect(true);   // the half-built buffer dies with exactly its constructed elements
    EXPECT_EQ(Counted::alive, 0);
    sgcl::vector<Counted> v;
    v.emplace_back("a");
    v.emplace_back("b");
    Counted::constructed = 0;
    Counted::throw_at = 0;
    EXPECT_THROW(v.emplace_back("c"), std::runtime_error);   // reallocation: the vector is unchanged
    Counted::throw_at = -1;
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].s, "a");
    EXPECT_EQ(v[1].s, "b");
    v.reserve(16);
    Counted::constructed = 0;
    Counted::throw_at = 0;
    EXPECT_THROW(v.insert(v.begin(), 2, Counted("d")), std::runtime_error);   // the temporary throws first
    Counted::throw_at = -1;
    EXPECT_EQ(v.size(), 2u);
}

TEST(Vector_Test, ResizeWithoutCopyingAndMoveOnlyElements) {
    sgcl::vector<MoveOnly> v;
    v.resize(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(*v[2].p, 0);
    v.emplace_back(7);
    v.push_back(MoveOnly(8));
    EXPECT_EQ(*v[4].p, 8);
    v.insert(v.begin(), MoveOnly(9));
    EXPECT_EQ(*v[0].p, 9);
    EXPECT_EQ(*v[5].p, 8);
    v.erase(v.begin() + 1, v.begin() + 3);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(*v[3].p, 8);
    sgcl::vector<MoveOnly> w = std::move(v);
    EXPECT_EQ(w.size(), 4u);
    EXPECT_TRUE(v.empty());
    v.resize(2);
    EXPECT_EQ(v.size(), 2u);
}

TEST(Vector_Test, LengthErrorAndCapacityRules) {
    sgcl::vector<int> v;
    EXPECT_THROW(v.reserve(v.max_size() + 1), std::length_error);
    EXPECT_THROW(v.resize(v.max_size() + 1), std::length_error);
    EXPECT_THROW(sgcl::vector<int> w(v.max_size() + 1), std::length_error);
    v = {1, 2, 3};
    auto cap = v.capacity();
    v.clear();
    EXPECT_EQ(v.capacity(), cap);   // clear keeps the buffer
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), 0u);    // shrink_to_fit of an empty vector drops it
    v = {1, 2, 3, 4, 5};
    v.pop_back();
    v.shrink_to_fit();
    EXPECT_GE(v.capacity(), 4u);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[3], 4);
}

TEST(Vector_Test, EagerDestructionOnRemovalAndOnDrop) {
    collector::force_collect(true);
    Counted::alive = 0;
    Counted::constructed = 0;
    {
        sgcl::vector<Counted> v;
        for (int i = 0; i < 5; ++i) {
            v.emplace_back(std::to_string(i));
        }
        EXPECT_EQ(Counted::alive, 5);
        v.pop_back();
        EXPECT_EQ(Counted::alive, 4);
        v.erase(v.begin());
        EXPECT_EQ(Counted::alive, 3);
        v.resize(1);
        EXPECT_EQ(Counted::alive, 1);
        v.assign(3, Counted("q"));
        EXPECT_EQ(Counted::alive, 3);
        v.clear();
        EXPECT_EQ(Counted::alive, 0);
        v = {Counted("a"), Counted("b")};
        EXPECT_EQ(Counted::alive, 2);
    }
    // the vector is gone and so are its elements, at once: the buffer waits
    // for the collector, empty
    EXPECT_EQ(Counted::alive, 0);
}

// Growth moves the elements and destroys the moved-from ones at once, as
// std::vector does; a vector inside a managed object destroys its elements
// when the object dies in a sweep, exactly once.
TEST(Vector_Test, ElementsDestroyedOnGrowthAndInASweep) {
    collector::force_collect(true);
    Counted::alive = 0;
    Counted::constructed = 0;
    {
        sgcl::vector<Counted> v;
        for (int i = 0; i < 100; ++i) {
            v.emplace_back(std::to_string(i));
            EXPECT_EQ(Counted::alive, i + 1);
        }
        EXPECT_EQ(v[99].s, "99");
    }
    EXPECT_EQ(Counted::alive, 0);
    struct Holder {
        sgcl::vector<Counted> values;
    };
    off_frame([&] {
        tracked_ptr<Holder> h = make_tracked<Holder>();
        for (int i = 0; i < 1000; ++i) {
            h->values.emplace_back(std::to_string(i));
        }
        EXPECT_EQ(Counted::alive, 1000);
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Counted::alive, 0);
}

TEST(Vector_Test, StdEraseAndEraseIf) {
    sgcl::vector<int> v = {1, 2, 3, 2, 4, 2};
    EXPECT_EQ(std::erase(v, 2), 3u);
    EXPECT_EQ(std::vector<int>(v.begin(), v.end()), (std::vector<int>{1, 3, 4}));
    EXPECT_EQ(std::erase_if(v, [](int x) { return x > 2; }), 2u);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 1);
    // unqualified, found by argument-dependent lookup in sgcl as for the
    // other containers (they were declared in namespace std before)
    sgcl::vector<int> w = {1, 2, 3};
    EXPECT_EQ(erase_if(w, [](int x) { return x != 2; }), 2u);
    EXPECT_EQ(erase(w, 2), 1u);
    EXPECT_TRUE(w.empty());
}

TEST(Vector_Test, RangesAndDeduction) {
    sgcl::vector<int> v = {5, 3, 1, 4};
    std::ranges::sort(v);
    EXPECT_EQ(std::vector<int>(v.begin(), v.end()), (std::vector<int>{1, 3, 4, 5}));
    auto it = std::ranges::find(v, 4);
    EXPECT_EQ(it - v.begin(), 2);
    std::span<int> s(v);
    EXPECT_EQ(s.size(), 4u);
    std::vector<int> src = {9, 8};
    sgcl::vector w(src.begin(), src.end());
    static_assert(std::is_same_v<decltype(w), sgcl::vector<int>>);
    EXPECT_EQ(w.size(), 2u);
    EXPECT_EQ(std::distance(v.crbegin(), v.crend()), 4);
    EXPECT_EQ(*v.crbegin(), 5);
}

TEST(Vector_Test, ElementsHoldingTrackedPointersAreTraced) {
    struct Node { int v; sgcl::tracked_ptr<Node> next; };
    sgcl::vector<sgcl::tracked_ptr<Node>> v;
    for (int i = 0; i < 1000; ++i) {
        auto n = sgcl::make_tracked<Node>();
        n->v = i;
        if (!v.empty()) {
            n->next = v.back();
        }
        v.push_back(std::move(n));
    }
    v.insert(v.begin() + 10, 3, sgcl::tracked_ptr<Node>());
    v.erase(v.begin() + 500, v.begin() + 600);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    int count = 0;
    for (auto& p : v) {
        if (p) {
            EXPECT_EQ(p->v, count < 497 ? count : count + 100);   // positions 500..599 held the values 497..596
            ++count;
        }
    }
    EXPECT_EQ(count, 900);
    EXPECT_EQ(v.back()->next->v, 998);
}

// A vector of tracked pointers growing on one thread while the collector
// runs on another: the elements move to the new buffer as words, without
// a barrier each, and the old buffer holds them through the cycle by its
// state. That state has to be set after the new buffer's allocation:
// set only by the copy of the handle, before the flip of the epoch, with
// the buffer allocated after it (a growth that takes a fresh page wakes
// the collector, which lands the flip right there), it left the targets
// held by nothing the cycle could see (vector.h: _relocate). Threads
// push into vectors of their own with garbage between the pushes, one
// thread forces a cycle before every push, and every element must be
// alive afterwards. mine.reserve() up front made it pass before the fix.
namespace {
    struct GrowthNode {
        static inline sgcl::atomic<int> alive = 0;
        explicit GrowthNode(int v) : value(v) { ++alive; }
        ~GrowthNode() { value = -1; --alive; }
        int value;
    };
}

TEST(Vector_Test, GrowthDuringACycle) {
    using Node = GrowthNode;
    const int threads = 8;
    const int per_thread = 4000;
    const int stride = 500;
    for (int round = 0; round < 40; ++round) {
        collector::force_collect(true);
        Node::alive = 0;
        sgcl::vector<tracked_ptr<Node>> kept;
        off_frame([&] {
            std::vector<std::thread> workers;
            sgcl::concurrent::queue<tracked_ptr<Node>> handed;
            for (int t = 0; t < threads; ++t) {
                workers.emplace_back([&, t] {
                    sgcl::vector<tracked_ptr<Node>> mine;   // grows on this thread's stack
                    for (int i = 0; i < per_thread; ++i) {
                        tracked_ptr node = make_tracked<Node>(i * threads + t);
                        if (i % stride == 0) {
                            mine.push_back(node);
                        }
                        if (i % stride == stride - 1 && t == 0) {
                            collector::force_collect(true);
                        }
                    }
                    for (auto& p : mine) {
                        handed.push(p);
                    }
                });
            }
            for (auto& w : workers) {
                w.join();
            }
            while (auto p = handed.try_pop()) {
                kept.push_back(*p);
            }
        });
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        ASSERT_EQ(kept.size(), size_t(threads * (per_thread / stride)));
        for (auto& p : kept) {
            ASSERT_GE(p->value, 0) << "round " << round;
        }
        ASSERT_EQ(Node::alive, (int)kept.size()) << "round " << round;
    }
}

// The review's fixes: an insertion within the capacity that throws, the
// growth of resize, the arguments of a growth, a count that wraps.
namespace {
    // Copies only, no move: every shift of the tail is a copy, and the
    // k-th copy (a construction or an assignment) throws. `alive` is the
    // constructions less the destructions.
    struct CopyCounted {
        static inline int alive = 0;
        static inline int copies = 0;
        static inline int throw_at = -1;
        int v;
        explicit CopyCounted(int x) : v(x) { ++alive; }
        CopyCounted(const CopyCounted& o) : v(o.v) { if (copies++ == throw_at) { throw std::runtime_error("copy"); } ++alive; }
        CopyCounted& operator=(const CopyCounted& o) { if (copies++ == throw_at) { throw std::runtime_error("copy"); } v = o.v; return *this; }
        ~CopyCounted() { --alive; }
    };

    std::vector<int> values(const sgcl::vector<CopyCounted>& v) {
        std::vector<int> out;
        for (auto& e : v) {
            out.push_back(e.v);
        }
        return out;
    }

    // A default construction that throws at the k-th one
    struct DefaultThrower {
        static inline int alive = 0;
        static inline int made = 0;
        static inline int throw_at = -1;
        DefaultThrower() { if (made++ == throw_at) { throw std::runtime_error("default"); } ++alive; }
        DefaultThrower(DefaultThrower&&) noexcept { ++alive; }
        ~DefaultThrower() { --alive; }
    };

    struct Owner {
        std::string name;
        Owner() = default;
        Owner(const Owner&) = delete;
    };

    struct View {
        std::string name;   // not trivially copyable: the growth takes the argument itself, not a copy
        View(const Owner& o) : name(o.name) {}
    };
}

TEST(Vector_Test, InPlaceInsertThatThrowsLeavesNothingUnaccounted) {
    CopyCounted::alive = 0;
    {
        CopyCounted x(9);
        sgcl::vector<CopyCounted> v;
        v.reserve(16);
        for (int i = 1; i <= 5; ++i) {
            v.emplace_back(i);
        }
        // the tail longer than the count: the top of the tail is copied
        // into raw storage first; the second copy throws, the vector is
        // as it was
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 1;
        EXPECT_THROW(v.insert(v.begin() + 1, 2, x), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 3, 4, 5}));
        EXPECT_EQ(CopyCounted::alive, 6);
        // the tail shorter than the count: the surplus copies of x go
        // above the old end first (the second throws), then the tail
        // (its copy throws), then the assignment over the tail (throws:
        // the size stays grown, the values as far as they got)
        v.erase(v.begin() + 3, v.end());
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 1;
        EXPECT_THROW(v.insert(v.begin() + 2, 3, x), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 3}));
        EXPECT_EQ(CopyCounted::alive, 4);
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 2;
        EXPECT_THROW(v.insert(v.begin() + 2, 3, x), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 3}));
        EXPECT_EQ(CopyCounted::alive, 4);
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 3;
        EXPECT_THROW(v.insert(v.begin() + 2, 3, x), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 3, 9, 9, 3}));
        EXPECT_EQ(CopyCounted::alive, 7);
        EXPECT_EQ(v.size(), 6u);
        // the range overload, the tail shorter than the range: the
        // elements past the old end are constructed first, in order
        CopyCounted::throw_at = -1;
        v.erase(v.begin() + 3, v.end());
        std::vector<CopyCounted> src = {CopyCounted(7), CopyCounted(8), CopyCounted(6)};
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 1;
        EXPECT_THROW(v.insert(v.begin() + 2, src.begin(), src.end()), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 3}));
        CopyCounted::throw_at = -1;
        v.insert(v.begin() + 2, src.begin(), src.end());
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 7, 8, 6, 3}));
        // a single element before the last: the temporary is the first
        // copy, the tail's copy into raw storage the second, which throws
        CopyCounted::copies = 0;
        CopyCounted::throw_at = 1;
        EXPECT_THROW(v.insert(v.end() - 1, x), std::runtime_error);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 7, 8, 6, 3}));
        CopyCounted::throw_at = -1;
        v.insert(v.end() - 1, x);
        EXPECT_EQ(values(v), (std::vector<int>{1, 2, 7, 8, 6, 9, 3}));
        EXPECT_EQ(CopyCounted::alive, 11);
    }
    EXPECT_EQ(CopyCounted::alive, 0);   // everything built was destroyed
}

TEST(Vector_Test, ResizeGrowsGeometricallyAndUndoesAThrowingConstruction) {
    sgcl::vector<int> v(20000);
    v.resize(v.capacity());   // full: a buffer past a page holds what its pages hold, more than the 20 000
    const size_t full = v.size();
    int reallocations = 0;
    auto data = v.data();
    for (int i = 0; i < 2000; ++i) {
        v.resize(v.size() + 1);
        if (v.data() != data) {
            ++reallocations;
            data = v.data();
        }
    }
    EXPECT_EQ(reallocations, 1);   // as a push_back: the capacity doubled
    EXPECT_GE(v.capacity(), 2 * full);
    EXPECT_EQ(v.size(), full + 2000);
    EXPECT_EQ(v[full + 1999], 0);
    DefaultThrower::alive = 0;
    {
        sgcl::vector<DefaultThrower> w(2);
        DefaultThrower::made = 0;
        DefaultThrower::throw_at = 1;   // the second appended
        EXPECT_THROW(w.resize(5), std::runtime_error);
        EXPECT_EQ(w.size(), 2u);
        EXPECT_EQ(DefaultThrower::alive, 2);
        w.shrink_to_fit();
        DefaultThrower::made = 0;
        DefaultThrower::throw_at = 2;   // above the capacity: the buffer grows, then the third throws
        EXPECT_THROW(w.resize(w.capacity() + 3), std::runtime_error);
        EXPECT_EQ(w.size(), 2u);
        EXPECT_EQ(DefaultThrower::alive, 2);
        DefaultThrower::throw_at = -1;
        w.resize(4);
        EXPECT_EQ(DefaultThrower::alive, 4);
    }
    EXPECT_EQ(DefaultThrower::alive, 0);
}

TEST(Vector_Test, GrowthTakesTheArgumentsAsGiven) {
    Owner owner;
    owner.name = "o";
    sgcl::vector<View> views;
    views.emplace_back(owner);   // an lvalue of a non-copyable type, on the growth from empty
    views.emplace_back(owner);
    EXPECT_EQ(views[1].name, "o");
    Counted c("a");
    sgcl::vector<Counted> v;
    Counted::constructed = 0;
    v.push_back(c);   // the growth from empty: one copy, into the new buffer
    EXPECT_EQ(Counted::constructed, 1);
    while (v.size() < v.capacity()) {
        v.push_back(c);
    }
    Counted::constructed = 0;
    v.push_back(c);   // a growth with elements to move: still one copy
    EXPECT_EQ(Counted::constructed, 1);
}

TEST(Vector_Test, InsertOfACountNearTheRangeThrowsLengthError) {
    sgcl::vector<int> v = {1};
    EXPECT_THROW(v.insert(v.begin(), SIZE_MAX, 2), std::length_error);   // size() + count wraps
    EXPECT_THROW(v.insert(v.begin(), v.max_size(), 2), std::length_error);
    EXPECT_THROW(v.resize(v.max_size() + 1, 2), std::length_error);
    EXPECT_EQ(v.size(), 1u);
}

// stable_sort of elements that hold tracked pointers: the order is stable
// and no element passes through memory the collector does not see (the
// standard's stable_sort moves them into a buffer of operator new, which a
// debug build's registration check refuses). The heap audit of 2026-09-26.
TEST(Vector_Test, StableSortOfTrackedPointers) {
    struct Item {
        int key;
        int order;
    };
    vector<tracked_ptr<Item>> v;
    for (int i = 0; i < 200; ++i) {
        v.push_back(make_tracked<Item>(Item{(i * 7) % 13, i}));
    }
    v.stable_sort([](const tracked_ptr<Item>& a, const tracked_ptr<Item>& b) { return a->key < b->key; });
    for (size_t i = 1; i < v.size(); ++i) {
        ASSERT_LE(v[i - 1]->key, v[i]->key);
        if (v[i - 1]->key == v[i]->key) {
            ASSERT_LT(v[i - 1]->order, v[i]->order);   // stable
        }
    }
    vector<pair<int, tracked_ptr<Item>>> pairs;
    for (int i = 0; i < 100; ++i) {
        pairs.push_back({i % 5, make_tracked<Item>(Item{i % 5, i})});
    }
    pairs.stable_sort();
    for (size_t i = 1; i < pairs.size(); ++i) {
        ASSERT_LE(pairs[i - 1].first, pairs[i].first);
    }
}

// The in-place stable sort against std::stable_sort on plain pairs: the
// same order for random keys of every length up to 1000, through vector,
// deque and a slice of a vector
TEST(Vector_Test, StableSortOfTrackedPointersAgreesWithTheStandard) {
    struct Item {
        int key;
        int order;
    };
    std::mt19937 rng(7);
    for (int n : {0, 1, 2, 15, 16, 17, 31, 64, 65, 100, 333, 1000}) {
        std::vector<std::pair<int, int>> oracle;
        vector<tracked_ptr<Item>> v;
        deque<tracked_ptr<Item>> d;
        for (int i = 0; i < n; ++i) {
            int key = int(rng() % 10);
            oracle.push_back({key, i});
            v.push_back(make_tracked<Item>(Item{key, i}));
            d.push_back(make_tracked<Item>(Item{key, i}));
        }
        std::stable_sort(oracle.begin(), oracle.end(), [](auto& a, auto& b) { return a.first < b.first; });
        auto by_key = [](const tracked_ptr<Item>& a, const tracked_ptr<Item>& b) { return a->key < b->key; };
        v.stable_sort(by_key);
        d.stable_sort(by_key);
        for (int i = 0; i < n; ++i) {
            ASSERT_EQ(v[i]->order, oracle[i].second) << "vector n=" << n << " i=" << i;
            ASSERT_EQ(d[i]->order, oracle[i].second) << "deque n=" << n << " i=" << i;
        }
        // a slice sorts the part of its vector it spans
        if (n >= 4) {
            vector<tracked_ptr<Item>> w;
            for (int i = 0; i < n; ++i) {
                w.push_back(make_tracked<Item>(Item{n - i, i}));
            }
            slice<tracked_ptr<Item>> part = w.as_slice(1, size_t(n) - 2);
            part.stable_sort(by_key);
            ASSERT_EQ(w[0]->order, 0);
            ASSERT_EQ(w[n - 1]->order, n - 1);
            for (int i = 2; i < n - 1; ++i) {
                ASSERT_LE(w[i - 1]->key, w[i]->key);
            }
        }
    }
}

// The cycles of the permutation: already sorted (cycles of one), reversed
// (cycles of two) and one long cycle (a rotation by one)
TEST(Vector_Test, StableSortOfTrackedPointersCycles) {
    struct Item {
        int key;
    };
    auto keys = [](const vector<tracked_ptr<Item>>& v) {
        std::vector<int> k;
        for (auto& p : v) {
            k.push_back(p->key);
        }
        return k;
    };
    auto by_key = [](const tracked_ptr<Item>& a, const tracked_ptr<Item>& b) { return a->key < b->key; };
    for (int n : {1, 2, 3, 100, 1001}) {
        std::vector<int> want(n);
        std::iota(want.begin(), want.end(), 0);
        vector<tracked_ptr<Item>> sorted, reversed, rotated;
        for (int i = 0; i < n; ++i) {
            sorted.push_back(make_tracked<Item>(Item{i}));
            reversed.push_back(make_tracked<Item>(Item{n - 1 - i}));
            rotated.push_back(make_tracked<Item>(Item{(i + 1) % n}));
        }
        sorted.stable_sort(by_key);
        reversed.stable_sort(by_key);
        rotated.stable_sort(by_key);
        EXPECT_EQ(keys(sorted), want) << n;
        EXPECT_EQ(keys(reversed), want) << n;
        EXPECT_EQ(keys(rotated), want) << n;
    }
}

// A buffer past a page takes the pages its bytes need and no more:
// 20 000 ints and the header, 80 016 bytes, are two pages (the alias
// array.h gives Array<> was not the one TypeInfo reads, and the inherited
// allocator of Array<PageDataSize> added 65 520 bytes: three)
TEST(Vector_Test, ABufferPastAPageTakesItsPagesOnly) {
    const size_t before = live_buffer_bytes<int>();
    sgcl::vector<int> odd;
    off_frame([&] {
        odd = sgcl::vector<int>(20000);
    });
    EXPECT_EQ(live_buffer_bytes<int>() - before, 2 * config::page_size);
}

// A vector past a page holds what its pages hold: 16384 ints and the
// header take two pages, and the capacity is all of them (it was the
// 16384 asked for), so that the growth doubles into four pages where it
// went to three and then five
TEST(Vector_Test, AVectorPastAPageFillsItsPages) {
    const size_t before = live_buffer_bytes<int>();
    sgcl::vector<int> v;
    off_frame([&] {
        v = sgcl::vector<int>(16384);
    });
    EXPECT_EQ(v.capacity(), (2 * config::page_size - sizeof(detail::ArrayBase)) / sizeof(int));
    EXPECT_EQ(live_buffer_bytes<int>() - before, 2 * config::page_size);
    off_frame([&] {
        while (v.size() < 40000) {
            v.push_back(int(v.size()));
        }
    });
    EXPECT_EQ(v.capacity(), (4 * config::page_size - sizeof(detail::ArrayBase)) / sizeof(int));
    EXPECT_EQ(live_buffer_bytes<int>() - before, 4 * config::page_size);
    EXPECT_EQ(v[16384], 16384);
    EXPECT_EQ(v[39999], 39999);
    sgcl::vector<int> odd(20000);
    EXPECT_EQ(odd.capacity(), (2 * config::page_size - sizeof(detail::ArrayBase)) / sizeof(int));
    sgcl::vector<tracked_ptr<int>> pointers(9000);   // 72 KB: two pages, the rest of them zeroed and traced too
    EXPECT_EQ(pointers.capacity(), (2 * config::page_size - sizeof(detail::ArrayBase)) / sizeof(tracked_ptr<int>));
    pointers.resize(pointers.capacity());
    for (auto& p : pointers) {
        EXPECT_FALSE(p);
    }
    pointers.back() = make_tracked<int>(7);
    collector::force_collect(true);
    EXPECT_EQ(*pointers.back(), 7);
}

namespace {
    // Buffers of `n` elements filled with 0xAB and dropped, the collector
    // run, so that the next buffers of that size reuse their slots or
    // their ranges of pages
    template<class Make>
    void leave_garbage(size_t n, Make make) {
        for (int round = 0; round < 20; ++round) {
            auto a = make(n);
            std::memset(static_cast<void*>(a.data()), 0xAB, n * sizeof(a[0]));
        }
        collector::force_collect(true);
        collector::force_collect(true);
    }

    template<class C>
    size_t nonzero_bytes(const C& c, size_t from = 0) {
        auto p = reinterpret_cast<const unsigned char*>(c.data());
        size_t bad = 0;
        for (size_t i = from * sizeof(c[0]); i < c.size() * sizeof(c[0]); ++i) {
            bad += p[i] != 0;
        }
        return bad;
    }

    // Not trivial, with a trivial member: T() zeroes the member, `new T`
    // would leave it as the slot was
    struct Mixed {
        std::string name;
        int count;
    };
}

// vector(count), resize(count) and emplace_back() value-initialize, as
// std's do: zeros for a trivial type, also where the buffer reuses the
// slot or the pages of an earlier one that held other bytes (a buffer of
// a type without tracked pointers is not zeroed when it is issued); the
// sizes go past a page, where a range of pages comes back as it was
TEST(Vector_Tests, CountResizeAndEmplaceValueInitializeAfterReuse) {
    for (size_t n : {size_t(64), size_t(1000), size_t(100000), size_t(3000000)}) {
        leave_garbage(n, [](size_t k) { return vector<int>(k); });
        for (int round = 0; round < 5; ++round) {
            vector<int> v(n);
            EXPECT_EQ(nonzero_bytes(v), 0u) << "vector(" << n << ")";
        }
        leave_garbage(n, [](size_t k) { return vector<std::byte>(k); });
        for (int round = 0; round < 5; ++round) {
            vector<std::byte> v;
            v.resize(n);
            EXPECT_EQ(nonzero_bytes(v), 0u) << "resize(" << n << ")";
        }
        leave_garbage(n, [](size_t k) { return vector<double>(k); });
        for (int round = 0; round < 5; ++round) {
            vector<double> v(n / 2, 1.5);
            v.resize(n);   // the tail past the copies: zeros
            EXPECT_EQ(nonzero_bytes(v, n / 2), 0u) << "resize past " << n / 2 << " to " << n;
        }
    }
    // emplace_back() of nothing into a slot a popped element left
    vector<uint64_t> v;
    v.reserve(8);
    v.push_back(0xABABABABABABABABull);
    v.pop_back();
    v.emplace_back();
    EXPECT_EQ(v[0], 0u);
    // a class with a trivial member: T() zeroes it
    leave_garbage(100000, [](size_t k) { return vector<std::array<int, 8>>(k); });
    vector<Mixed> m(3000);
    for (auto& e : m) {
        EXPECT_EQ(e.count, 0);
        EXPECT_TRUE(e.name.empty());
    }
}

// Boundaries (DESIGN 408)

namespace {
    struct Three {
        char bytes[3] = {};
    };
}

// max_size() is the most elements whose bytes a difference_type holds; a
// count past it is length_error before anything changes (SIZE_MAX, one
// past, through every member that takes a count), and a count within it
// that no memory holds ends the program as a refused managed allocation
TEST(Vector_Test, CountsAtTheLimits) {
    EXPECT_EQ(sgcl::vector<char>().max_size(), size_t(PTRDIFF_MAX));
    EXPECT_EQ(sgcl::vector<int>().max_size(), size_t(PTRDIFF_MAX) / 4);
    EXPECT_EQ(sgcl::vector<Three>().max_size(), size_t(PTRDIFF_MAX) / 3);
    sgcl::vector<int> v = {1, 2, 3};
    const auto data = v.data();
    for (size_t count : {v.max_size() + 1, SIZE_MAX / 4 + 1, SIZE_MAX}) {
        EXPECT_THROW(v.reserve(count), std::length_error);
        EXPECT_THROW(v.resize(count), std::length_error);
        EXPECT_THROW(v.resize(count, 7), std::length_error);
        EXPECT_THROW(v.assign(count, 7), std::length_error);
        EXPECT_THROW(v.insert(v.begin(), count, 7), std::length_error);
        EXPECT_THROW((void)sgcl::vector<int>(count), std::length_error);
        EXPECT_THROW((void)sgcl::vector<int>(count, 7), std::length_error);
    }
    EXPECT_THROW(v.insert(v.end(), v.max_size() - 2, 7), std::length_error);   // one past with the three there
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v.data(), data);
    EXPECT_EQ(v[2], 3);
    EXPECT_THROW(v.at(SIZE_MAX), std::out_of_range);
    EXPECT_THROW(v.at(3), std::out_of_range);
    EXPECT_THROW(v.as_slice(4), std::out_of_range);
    EXPECT_EQ(v.as_slice(3).size(), 0u);
    EXPECT_EQ(v.as_slice(1, SIZE_MAX).size(), 2u);
    EXPECT_EQ(v.as_slice(3, SIZE_MAX).size(), 0u);
    EXPECT_EQ(v.as_slice(0, 0).size(), 0u);
}

TEST(Vector_Test, ACountNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    auto reserve = [] {
        sgcl::vector<char> v;
        v.reserve(v.max_size());
    };
    auto construct = [] {
        sgcl::vector<int> v(sgcl::vector<int>().max_size());
    };
    auto resize = [] {
        sgcl::vector<Three> v(1);
        v.resize(v.max_size());
    };
    EXPECT_DEATH(reserve(), "sgcl: out of managed memory");
    EXPECT_DEATH(construct(), "sgcl: out of managed memory");
    EXPECT_DEATH(resize(), "sgcl: out of managed memory");
}

namespace {
    // Every member on an empty vector without a buffer (default or moved
    // from): it works as an empty one, and takes elements again after
    template<class T>
    void expect_vector_works_empty(sgcl::vector<T>& v, const T& value) {
        EXPECT_TRUE(v.empty());
        EXPECT_EQ(v.size(), 0u);
        EXPECT_EQ(v.capacity(), 0u);
        EXPECT_EQ(v.data(), nullptr);
        EXPECT_EQ(v.begin(), v.end());
        EXPECT_EQ(v.rbegin(), v.rend());
        EXPECT_EQ(v.as_slice().size(), 0u);
        EXPECT_EQ(v.as_slice(0).size(), 0u);
        EXPECT_THROW(v.at(0), std::out_of_range);
        EXPECT_EQ(v.erase(v.begin(), v.end()), v.end());
        EXPECT_EQ(sgcl::erase(v, value), 0u);
        EXPECT_EQ(sgcl::erase_if(v, [](const T&) { return true; }), 0u);
        EXPECT_TRUE(v == sgcl::vector<T>());
        EXPECT_FALSE(v < sgcl::vector<T>());
        v.clear();
        v.shrink_to_fit();
        v.resize(0);
        v.reserve(0);
        v.assign(0, value);
        v.insert(v.begin(), 0, value);
        EXPECT_EQ(v.capacity(), 0u);
        sgcl::vector<T> copy(v);
        EXPECT_TRUE(copy.empty());
        sgcl::vector<T> other = {value};
        v.swap(other);
        v.swap(other);
        EXPECT_TRUE(v.empty());
        v.push_back(value);
        EXPECT_EQ(v.size(), 1u);
        EXPECT_EQ(v.front(), value);
        v.pop_back();
        EXPECT_TRUE(v.empty());
    }
}

TEST(Vector_Test, MovedFromAndDefaultWorkAsEmpty) {
    sgcl::vector<std::string> v = {"a", "b"};
    sgcl::vector<std::string> to(std::move(v));
    EXPECT_EQ(to.size(), 2u);
    expect_vector_works_empty(v, std::string("x"));
    sgcl::vector<std::string> assigned = {"c"};
    assigned = std::move(to);
    EXPECT_EQ(assigned.size(), 2u);
    expect_vector_works_empty(to, std::string("y"));
    sgcl::vector<int> d;
    expect_vector_works_empty(d, 1);
    sgcl::vector<int> empty_list = {};
    expect_vector_works_empty(empty_list, 1);
    sgcl::vector<int> none(0);
    expect_vector_works_empty(none, 1);
    sgcl::vector<int> none_of(0, 5);
    expect_vector_works_empty(none_of, 1);
}

// The vector on both sides: a copy and a move assignment to itself, swap
// with itself keep the elements and the buffer
TEST(Vector_Test, AVectorOnBothSidesKeepsItself) {
    sgcl::vector<std::string> v = {"one long enough for the heap", "two", "three"};
    const auto before = v;
    const auto data = v.data();
    auto& self = v;
    v = self;
    EXPECT_EQ(v, before);
    EXPECT_EQ(v.data(), data);
    v = std::move(self);
    EXPECT_EQ(v, before);
    EXPECT_EQ(v.data(), data);
    v.swap(self);
    swap(v, self);
    EXPECT_EQ(v, before);
    EXPECT_EQ(v.data(), data);
    EXPECT_TRUE(v == self);
    EXPECT_FALSE(v < self);
    EXPECT_TRUE((v <=> self) == 0);
}

// erase(v, value) with an element of v as the value: std::remove moved the
// elements down over the one the value referred to, and compared the rest
// with what had been moved there (erase(v, v[0]) of {1, 2, 1, 3, 1} took one
// element and left 2, 1, 3, 1). The element is now taken out of the
// comparison first; a move-only element too
TEST(Vector_Test, EraseOfAValueThatIsAnElement) {
    sgcl::vector<int> v = {1, 2, 1, 3, 1};
    EXPECT_EQ(sgcl::erase(v, v[0]), 3u);
    EXPECT_EQ(v, (sgcl::vector<int>{2, 3}));
    sgcl::vector<std::string> s = {"x", "long enough for the heap", "y", "long enough for the heap", "z"};
    EXPECT_EQ(std::erase(s, s[3]), 2u);   // the second of two, after the first was erased
    EXPECT_EQ(s, (sgcl::vector<std::string>{"x", "y", "z"}));
    EXPECT_EQ(sgcl::erase(s, s.back()), 1u);
    EXPECT_EQ(s, (sgcl::vector<std::string>{"x", "y"}));
    sgcl::vector<std::unique_ptr<int>> u;
    u.push_back(nullptr);
    u.push_back(std::make_unique<int>(1));
    u.push_back(nullptr);
    EXPECT_EQ(sgcl::erase(u, u[2]), 2u);
    ASSERT_EQ(u.size(), 1u);
    EXPECT_EQ(*u[0], 1);
    sgcl::vector<int> one = {4};
    EXPECT_EQ(sgcl::erase(one, one[0]), 1u);
    EXPECT_TRUE(one.empty());
}

// The vector's own element as the argument of what may reallocate: resize
// to more copies of an element, push_back of an element moved out of the
// vector itself (the element left moved from), at and within the capacity
TEST(Vector_Test, ItsOwnElementAcrossAGrowth) {
    sgcl::vector<std::string> v = {"long enough to live on the heap"};
    v.shrink_to_fit();
    v.resize(5, v[0]);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[4], "long enough to live on the heap");
    v.reserve(16);
    v.resize(8, v[4]);
    EXPECT_EQ(v[7], "long enough to live on the heap");
    sgcl::vector<std::string> m = {"moved along"};
    m.shrink_to_fit();
    m.push_back(std::move(m[0]));   // growth: the new element first, from the old buffer
    EXPECT_EQ(m[1], "moved along");
    m.push_back(std::move(m[1]));   // within or past the capacity, the same
    EXPECT_EQ(m[2], "moved along");
    m.emplace(m.begin(), std::move(m[2]));
    EXPECT_EQ(m[0], "moved along");
}

// One element and empty ranges: an insertion of nothing at either end
// returns its position, emplace at end() of an empty vector, an erasure
// of the one element, and pop_back to empty keeping the buffer
TEST(Vector_Test, OneElementAndEmptyRanges) {
    sgcl::vector<int> v;
    EXPECT_EQ(v.emplace(v.end(), 9), v.begin());
    EXPECT_EQ(v.size(), 1u);
    std::vector<int> none;
    EXPECT_EQ(v.insert(v.begin(), none.begin(), none.end()), v.begin());
    EXPECT_EQ(v.insert(v.end(), none.begin(), none.end()), v.end());
    EXPECT_EQ(v.insert(v.end(), 0, 1), v.end());
    EXPECT_EQ(v.insert(v.begin(), std::initializer_list<int>{}), v.begin());
    EXPECT_EQ(v.erase(v.end()), v.end());   // end() is no element: nothing erased
    EXPECT_EQ(v.size(), 1u);
    auto capacity = v.capacity();
    EXPECT_EQ(v.erase(v.begin()), v.end());
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), capacity);
    v.push_back(1);
    v.pop_back();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), capacity);
}

// The iterators as the page states them: within the capacity a push_back
// and an insertion keep those before the point; reserve that does not
// reallocate keeps all; swap keeps them, now into the other
// vector; a slice keeps reading the old elements after a reallocation
TEST(Vector_Test, IteratorsAsThePageStatesThem) {
    sgcl::vector<int> v;
    v.reserve(8);
    v = {1, 2, 3};
    ASSERT_GE(v.capacity(), 8u);
    auto first = v.begin();
    auto second = v.begin() + 1;
    v.push_back(4);
    v.insert(v.begin() + 2, 9);
    EXPECT_EQ(v.begin(), first);
    EXPECT_EQ(*second, 2);
    v.reserve(v.capacity());
    v.reserve(0);
    EXPECT_EQ(v.begin(), first);
    auto slice = v.as_slice();
    v.resize(v.capacity() + 1);
    EXPECT_NE(v.begin(), first);   // reallocated
    EXPECT_EQ(slice.size(), 5u);
    EXPECT_EQ(slice[4], 4);
    sgcl::vector<int> exact = {1, 2};
    sgcl::vector<int> other;
    auto it = exact.begin();
    exact.swap(other);
    EXPECT_EQ(it, other.begin());
    EXPECT_TRUE(exact.empty());
}
