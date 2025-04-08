//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(UnorderedMap_Test, InsertAndFind) {
    sgcl::unordered_map<std::string, int> map;
    auto [it, inserted] = map.insert({"a", 1});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, "a");
    EXPECT_EQ(it->second, 1);
    EXPECT_NE(map.find("a"), map.end());
    EXPECT_EQ(map.find("b"), map.end());
}

TEST(UnorderedMap_Test, OperatorAccess) {
    sgcl::unordered_map<std::string, int> map;
    map["x"] = 42;
    EXPECT_EQ(map["x"], 42);
    EXPECT_EQ(map.size(), 1);

    map["x"] = 100;
    EXPECT_EQ(map["x"], 100);
    EXPECT_EQ(map.size(), 1);
}

TEST(UnorderedMap_Test, AtMethod) {
    sgcl::unordered_map<std::string, int> map;
    map["k"] = 5;
    EXPECT_EQ(map.at("k"), 5);
    EXPECT_THROW(map.at("missing"), std::out_of_range);
}

TEST(UnorderedMap_Test, CountAndContains) {
    sgcl::unordered_map<std::string, int> map;
    map["one"] = 1;
    EXPECT_TRUE(map.contains("one"));
    EXPECT_EQ(map.count("one"), 1);
    EXPECT_FALSE(map.contains("none"));
    EXPECT_EQ(map.count("none"), 0);
}

TEST(UnorderedMap_Test, EraseByKey) {
    sgcl::unordered_map<std::string, int> map;
    map["one"] = 1;
    EXPECT_EQ(map.erase("one"), 1);
    EXPECT_FALSE(map.contains("one"));
    EXPECT_EQ(map.erase("not-there"), 0);
}

TEST(UnorderedMap_Test, Clear) {
    sgcl::unordered_map<std::string, int> map = {{"a", 1}, {"b", 2}};
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);
}

TEST(UnorderedMap_Test, CopyConstructor) {
    sgcl::unordered_map<std::string, int> original = {{"x", 42}};
    sgcl::unordered_map<std::string, int> copy(original);
    EXPECT_EQ(copy.size(), 1);
    EXPECT_TRUE(copy.contains("x"));
    EXPECT_EQ(copy["x"], 42);
}

TEST(UnorderedMap_Test, MoveConstructor) {
    sgcl::unordered_map<std::string, int> original = {{"z", 99}};
    sgcl::unordered_map<std::string, int> moved(std::move(original));
    EXPECT_TRUE(moved.contains("z"));
    EXPECT_EQ(moved["z"], 99);
    EXPECT_TRUE(original.empty());
}

TEST(UnorderedMap_Test, InsertOrAssign) {
    sgcl::unordered_map<std::string, int> map;
    map.insert_or_assign("a", 1);
    EXPECT_EQ(map["a"], 1);
    map.insert_or_assign("a", 2);
    EXPECT_EQ(map["a"], 2);
}

TEST(UnorderedMap_Test, TryEmplace) {
    sgcl::unordered_map<std::string, int> map;
    map.try_emplace("a", 123);
    EXPECT_EQ(map["a"], 123);
    map.try_emplace("a", 456);
    EXPECT_EQ(map["a"], 123);
}

TEST(UnorderedMap_Test, EqualRange) {
    sgcl::unordered_map<std::string, int> map = {{"a", 1}};
    auto [first, second] = map.equal_range("a");
    EXPECT_NE(first, map.end());
    EXPECT_EQ(first->first, "a");
    EXPECT_EQ(std::distance(first, second), 1);
}

TEST(UnorderedMap_Test, Swap) {
    sgcl::unordered_map<std::string, int> a = {{"a", 1}};
    sgcl::unordered_map<std::string, int> b = {{"b", 2}};
    a.swap(b);
    EXPECT_TRUE(a.contains("b"));
    EXPECT_TRUE(b.contains("a"));
}

TEST(UnorderedMap_Test, RehashAndReserve) {
    sgcl::unordered_map<std::string, int> map;
    map.reserve(100);
    EXPECT_GE(map.bucket_count(), 100 / map.max_load_factor());
    map.rehash(200);
    EXPECT_GE(map.bucket_count(), 200);
}

TEST(UnorderedMap_Test, IteratorTraversal) {
    sgcl::unordered_map<std::string, int> map = {{"a", 1}, {"b", 2}};
    sgcl::vector<std::string> keys;
    for (const auto& [k, v] : map) {
        keys.push_back(k);
    }
    EXPECT_EQ(keys.size(), map.size());
}

TEST(UnorderedMap_Test, MergeLvalue) {
    sgcl::unordered_map<std::string, int> a = {{"a", 1}};
    sgcl::unordered_map<std::string, int> b = {{"b", 2}};
    a.merge(b);
    EXPECT_TRUE(a.contains("b"));
    EXPECT_FALSE(b.contains("b"));
}

TEST(UnorderedMap_Test, MergeRvalue) {
    sgcl::unordered_map<std::string, int> a = {{"a", 1}};
    sgcl::unordered_map<std::string, int> b = {{"b", 2}};
    a.merge(std::move(b));
    EXPECT_TRUE(a.contains("b"));
    EXPECT_TRUE(b.empty());
}

TEST(UnorderedMap_Test, ExtractByKey) {
    sgcl::unordered_map<std::string, int> map = {{"x", 10}};
    auto node = map.extract("x");
    EXPECT_TRUE(node.key() == "x");
    EXPECT_EQ(node.value(), 10);
    EXPECT_TRUE(map.empty());
}

TEST(UnorderedMap_Test, ExtractByIterator) {
    sgcl::unordered_map<std::string, int> map = {{"y", 20}};
    auto it = map.find("y");
    auto node = map.extract(it);
    EXPECT_EQ(node.key(), "y");
    EXPECT_EQ(node.value(), 20);
    EXPECT_TRUE(map.empty());
}

TEST(UnorderedMap_Test, NodeTypeReuse) {
    sgcl::unordered_map<std::string, int> map;
    map.insert({"z", 30});
    auto node = map.extract("z");
    EXPECT_TRUE(node.key() == "z");
    EXPECT_EQ(node.value(), 30);
    auto result = map.insert(std::move(node));
    EXPECT_TRUE(result.inserted);
    EXPECT_TRUE(map.contains("z"));
}

TEST(UnorderedMap_Test, EraseInvalidIterator) {
    sgcl::unordered_map<std::string, int> map = { {"one", 1}, {"two", 2} };
    auto endIt = map.end();
    auto result = map.erase(endIt);
    EXPECT_EQ(result, endIt);
    EXPECT_EQ(map.size(), 2);
}

TEST(UnorderedMap_Test, TryEmplaceOnCollision) {
    sgcl::unordered_map<int, std::string> map;
    map.try_emplace(1, "first");
    auto [it, inserted] = map.try_emplace(1, "second");
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->second, "first");
}

TEST(UnorderedMap_Test, RehashEmptyMap) {
    sgcl::unordered_map<std::string, int> map;
    map.rehash(50);
    EXPECT_GE(map.bucket_count(), 50);
    EXPECT_TRUE(map.empty());
}

TEST(UnorderedMap_Test, ShrinkToFit) {
    sgcl::unordered_map<std::string, int> map = { {"a", 1}, {"b", 2}, {"c", 3} };
    map.rehash(64);
    ASSERT_GT(map.bucket_count(), map.size());
    map.shrink_to_fit();
    EXPECT_EQ(map.bucket_count(), 8);
}
