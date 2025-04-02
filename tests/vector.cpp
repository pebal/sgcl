//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
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
    auto it = vec.emplace_back(5);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
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

TEST(Vector_Test, Emplace) {
    sgcl::vector<Int> vec({2, 3, 5});
    auto it = vec.emplace(vec.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(*it, 1);
    it = vec.emplace(vec.begin() + 3, 4);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(*it, 4);
    it = vec.emplace(vec.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(*it, 6);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(vec.size(), 6u);
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertValue) {
    sgcl::vector<Int> vec({2, 3, 5});
    auto it = vec.insert(vec.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(*it, 1);
    it = vec.insert(vec.begin() + 3, 4);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(*it, 4);
    it = vec.insert(vec.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(vec.size(), 6u);
    EXPECT_EQ(*it, 6);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertNValues) {
    sgcl::vector<Int> vec({2, 3, 5});
    auto it = vec.insert(vec.begin(), 0, 1);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(it, vec.begin());
    it = vec.insert(vec.begin(), 2, 1);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*++it, 1);
    it = vec.insert(vec.begin() + 4, 2, 4);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 4);
    it = vec.insert(vec.end(), 2, 6);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(*++it, 6);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {1, 1, 2, 3, 4, 4, 5, 6, 6};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertRange) {
    sgcl::vector<Int> vec({2, 3, 5});
    std::vector<int> other = {4, 5};
    auto it = vec.insert(vec.end(), other.begin(), other.begin());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(it, vec.end());
    it = vec.insert(vec.begin(), other.begin(), other.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 5);
    other = std::vector<int>{7, 8};
    it = vec.insert(vec.begin() + 4, other.begin(), other.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    EXPECT_EQ(*it, 7);
    EXPECT_EQ(*++it, 8);
    other = std::vector<int>{2, 3};
    it = vec.insert(vec.end(), other.begin(), other.end());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*++it, 3);
    std::vector<int> result(vec.begin(), vec.end());
    std::vector<int> expected = {4, 5, 2, 3, 7, 8, 5, 2, 3};
    EXPECT_EQ(result, expected);
}

TEST(Vector_Test, InsertList) {
    sgcl::vector<Int> vec({2, 3, 5});
    auto it = vec.insert(vec.end(), std::initializer_list<Int>());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(it, vec.end());
    it = vec.insert(vec.begin(), {5, 4});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(*++it, 4);
    it = vec.insert(vec.begin() + 4, {8, 7});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(vec.size(), 7u);
    EXPECT_EQ(*it, 8);
    EXPECT_EQ(*++it, 7);
    it = vec.insert(vec.end(), {3, 2});
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(vec.size(), 9u);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(*++it, 2);
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
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.begin(), vec.end());
    EXPECT_EQ(vec.cbegin(), vec.cend());
    EXPECT_EQ(vec.rbegin(), vec.rend());
    EXPECT_EQ(vec.crbegin(), vec.crend());
}
