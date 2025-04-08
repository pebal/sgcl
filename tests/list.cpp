//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <vector>

TEST(List_Test, DefaultConstructorEmpty) {
    sgcl::list<int> lst;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.rbegin(), lst.rend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
}

TEST(List_Test, ConstructorNDefault) {
    sgcl::list<int> lst(3);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {0, 0, 0};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, ConstructorNValues) {
    sgcl::list<int> lst(4, 3);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 4u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {3, 3, 3, 3};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, List) {
    sgcl::list<int> lst({4, 5, 6});
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, ConstructorRange) {
    std::vector<int> expected = {1, 2, 3};
    sgcl::list<int> lst(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    EXPECT_EQ(result, expected);
}

TEST(List_Test, CopyConstructor) {
    sgcl::list<int> other({1, 2, 3});
    sgcl::list<int> lst(other);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, MoveConstructor) {
    sgcl::list<int> other({4, 5, 6});
    sgcl::list<int> lst(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_TRUE(other.empty());
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, CopyAssignment) {
    sgcl::list<int> other({1, 2, 3});
    sgcl::list<int> lst;
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({3, 2, 1});
    EXPECT_EQ(result, expected);

    other = sgcl::list<int>({2, 3});
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.begin(), lst.end());
    expected = std::vector<int>({2, 3});
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({3, 2});
    EXPECT_EQ(result, expected);

    other = sgcl::list<int>();
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, MoveAssignment) {
    sgcl::list<int> other({4, 5, 6});
    sgcl::list<int> lst;
    lst = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_TRUE(other.empty());
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, ListAssignment) {
    sgcl::list<int> lst;
    lst = {7, 8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {7, 8, 9};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({9, 8, 7});
    EXPECT_EQ(result, expected);

    lst = {8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.begin(), lst.end());
    expected = std::vector<int>({8, 9});
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({9, 8});
    EXPECT_EQ(result, expected);

    lst = std::initializer_list<int>();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignNValues) {
    sgcl::list<int> lst({1, 2});
    lst.assign(4, 5);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {5, 5, 5, 5};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    EXPECT_EQ(result, expected);

    lst.assign(2, 3);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.begin(), lst.end());
    expected = {3, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    EXPECT_EQ(result, expected);

    lst.assign(0, 0);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignRange) {
    sgcl::list<int> lst;
    std::vector<int> expected = {3, 4, 5};
    lst.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({5, 4, 3});
    EXPECT_EQ(result, expected);

    expected = {2, 3};
    lst.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.begin(), lst.end());
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({3, 2});
    EXPECT_EQ(result, expected);

    lst.assign(expected.begin(), expected.begin());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignList) {
    sgcl::list<int> lst({1, 2});
    lst.assign({3, 4, 5});
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> result(lst.cbegin(), lst.cend());
    std::vector<int> expected = {3, 4, 5};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.crbegin(), lst.crend());
    expected = std::vector<int>({5, 4, 3});
    EXPECT_EQ(result, expected);

    lst.assign({2, 3});
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.cbegin(), lst.cend());
    expected = {2, 3};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.crbegin(), lst.crend());
    expected = std::vector<int>({3, 2});
    EXPECT_EQ(result, expected);

    lst.assign(std::initializer_list<int>());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
}

TEST(List_Test, Front) {
    sgcl::list<int> lst({5, 6, 7});
    EXPECT_EQ(lst.front(), 5);
}

TEST(List_Test, Back) {
    sgcl::list<int> lst({5, 6, 7});
    EXPECT_EQ(lst.back(), 7);
}

TEST(List_Test, PushAndEmplaceBack) {
    sgcl::list<int> lst({2, 3});
    lst.push_back(4);
    auto v = lst.emplace_back(5);
    EXPECT_EQ(v, 5);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {2, 3, 4, 5};
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(result, expected);
}

TEST(List_Test, PushAndEmplaceFront) {
    sgcl::list<int> lst({3, 4});
    lst.push_front(2);
    auto v = lst.emplace_front(1);
    EXPECT_EQ(v, 1);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 4};
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(result, expected);
}

TEST(List_Test, PopBack) {
    sgcl::list<int> lst({2, 3, 4});
    lst.pop_back();
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {2, 3};
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(result, expected);
}

TEST(List_Test, PopFront) {
    sgcl::list<int> lst({2, 3, 4});
    lst.pop_front();
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {3, 4};
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Emplace) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.emplace(lst.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(*it, 1);
    it = lst.emplace(++++++lst.begin(), 4);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(*it, 4);
    it = lst.emplace(lst.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(*it, 6);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(result, expected);
}

TEST(List_Test, InsertValue) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 1);
    it = lst.insert(++++++lst.begin(), 4);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 4);
    it = lst.insert(lst.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(*it, 6);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, InsertNValues) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.begin(), 0, 1);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.begin());
    it = lst.insert(lst.begin(), 2, 1);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*++it, 1);
    it = lst.insert(++++++++lst.begin(), 2, 4);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 4);
    it = lst.insert(lst.end(), 2, 6);
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(*++it, 6);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 1, 2, 3, 4, 4, 5, 6, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, InsertRange) {
    sgcl::list<int> lst({2, 3, 5});
    std::vector<int> vec = {4, 5};
    auto it = lst.insert(lst.end(), vec.begin(), vec.begin());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.end());
    it = lst.insert(lst.begin(), vec.begin(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 5);
    vec = std::vector<int>{7, 8};
    it = lst.insert(++++++++lst.begin(), vec.begin(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 7);
    EXPECT_EQ(*++it, 8);
    vec = std::vector<int>{2, 3};
    it = lst.insert(lst.end(), vec.begin(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*++it, 3);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {4, 5, 2, 3, 7, 8, 5, 2, 3};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, InsertList) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.end(), std::initializer_list<int>());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.end());
    it = lst.insert(lst.begin(), {5, 4});
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(*++it, 4);
    it = lst.insert(++++++++lst.begin(), {8, 7});
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 8);
    EXPECT_EQ(*++it, 7);
    it = lst.insert(lst.end(), {3, 2});
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(*++it, 2);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {5, 4, 2, 3, 8, 7, 5, 3, 2};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Erase) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    auto it = lst.erase(lst.begin());
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 2);
    it = lst.erase(++lst.begin());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(*it, 4);
    it = lst.erase(--lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(it, lst.end());
    it = lst.erase(lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(it, lst.end());
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, EraseRange) {
    sgcl::list<int> lst({1, 2, 3, 4, 5, 6, 7, 8});
    auto it = lst.erase(lst.begin(), ++++lst.begin());
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(*it, 3);
    it = lst.erase(++lst.begin(), ++++++lst.begin());
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 6);
    it = lst.erase(----lst.end(), lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(it, lst.end());
    it = lst.erase(lst.end(), lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(it, lst.end());
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {3, 6};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Resize) {
    sgcl::list<int> lst({1, 2, 3});
    lst.resize(5);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 0, 0};
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(result, expected);
    lst.resize(2);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    result = std::vector<int>(lst.begin(), lst.end());
    expected = std::vector<int>({1, 2});
    EXPECT_EQ(result, expected);
    lst.resize(4, 8);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    result = std::vector<int>(lst.begin(), lst.end());
    expected = std::vector<int>({1, 2, 8, 8});
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Swap) {
    sgcl::list<int> lst1({1, 2});
    sgcl::list<int> lst2({4, 5, 6, 7});
    lst1.swap(lst2);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst1.size(), 4);
    EXPECT_EQ(lst2.size(), 2);
    std::vector<int> result(lst1.begin(), lst1.end());
    std::vector<int> expected = {4, 5, 6, 7};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst2.begin(), lst2.end());
    expected = std::vector<int>({1, 2});
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Merge) {
    sgcl::list<int> lst1({1, 6, 8});
    sgcl::list<int> lst2({4, 5, 6, 7});
    lst1.merge(lst2);
    EXPECT_EQ(collector::get_live_object_count(), 9u);
    EXPECT_EQ(lst1.size(), 7);
    EXPECT_EQ(lst2.size(), 0);
    EXPECT_EQ(lst2.begin(), lst2.end());
    std::vector<int> result(lst1.begin(), lst1.end());
    std::vector<int> expected = {1, 4, 5, 6, 6, 7, 8};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Sort) {
    sgcl::list<int> lst({5, 1, 2, 7, 3});
    lst.sort();
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 5, 7};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({7, 5, 3, 2, 1});
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Remove) {
    sgcl::list<int> lst({5, 1, 5, 7, 5});
    auto count = lst.remove(5);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2);
    EXPECT_EQ(count, 3);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 7};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, RemoveIf) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    auto count = lst.remove_if([](int v){ return v % 2 == 1; });
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2);
    EXPECT_EQ(count, 3);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Reverse) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    lst.reverse();
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(result, expected);
    result = std::vector<int>(lst.rbegin(), lst.rend());
    expected = std::vector<int>({1, 2, 3, 4, 5});
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Unique) {
    sgcl::list<int> lst({1, 2, 2, 3, 3, 3, 2, 1, 1, 2});
    auto count = lst.unique();
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6);
    EXPECT_EQ(count, 4);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 2, 1, 2};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, UniquePredicate) {
    sgcl::list<int> lst({1, 2, 2, 3, 3, 3, 5, 1, 1, 2});
    auto count = lst.unique([](int a, int b){ return a % 2 == b % 2; });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4);
    EXPECT_EQ(count, 6);
    std::vector<int> result(lst.begin(), lst.end());
    std::vector<int> expected = {1, 2, 3, 2};
    EXPECT_EQ(result, expected);
}

TEST(List_Test, Clear) {
    sgcl::list<int> lst({5, 6, 7});
    lst.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.rbegin(), lst.rend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
}

TEST(List_Test, Compare) {
    sgcl::list<int> lst1({1, 2, 3});
    sgcl::list<int> lst2({1, 2, 3});
    EXPECT_EQ(lst1, lst2);
    lst2 = {3, 2, 1};
    EXPECT_NE(lst1, lst2);
}
