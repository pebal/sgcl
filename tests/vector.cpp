//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>

#include <vector>

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
