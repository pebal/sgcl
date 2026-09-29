//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Project Wycheproof's AEAD vectors, read from
// ~/Programming/oracles/wycheproof/testvectors_v1/ when they are there
// (aes_gcm_test.json, chacha20_poly1305_test.json,
// xchacha20_poly1305_test.json); a test whose file is missing is skipped.
// Their download waits for the user's consent. The reader itself is tested
// here on a document in their format, so that it is known to work before
// the files arrive.
//
// Each case: "valid" must seal to ct || tag and open to msg; "invalid"
// must not open; "acceptable" is not asked either way. Groups the module
// does not have (GCM with an IV other than 96 bits or a tag other than 128)
// are counted and skipped.
#include "cipher_test.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace cipher_test;

namespace {
    struct AeadCase {
        int64_t id;
        bytes key, iv, aad, msg, ct, tag;
        std::string result;
    };

    std::string field(const encoding::json& j, const char* name) {
        auto s = j[string(name)].as_string();
        return s ? std::string(s->data(), s->size()) : std::string();
    }

    // The cases of a document of type AeadTest, those of groups with an
    // IV of iv_bits and a tag of 128 bits; the others counted in skipped
    std::vector<AeadCase> load_aead(const std::string& text, int64_t iv_bits, size_t& skipped) {
        std::vector<AeadCase> cases;
        auto doc = encoding::json::parse(string(text));
        if (!doc) {
            ADD_FAILURE() << "not JSON: " << std::string(doc.error().message().data(), doc.error().message().size());
            return cases;
        }
        const auto& groups = (*doc)["testGroups"];
        for (size_t g = 0; g < groups.size(); ++g) {
            const auto& group = groups[g];
            const auto& tests = group["tests"];
            if (group["ivSize"].as_int().value_or(-1) != iv_bits || group["tagSize"].as_int().value_or(-1) != 128) {
                skipped += tests.size();
                continue;
            }
            for (size_t t = 0; t < tests.size(); ++t) {
                const auto& c = tests[t];
                cases.push_back({c["tcId"].as_int().value_or(-1), hex(field(c, "key")), hex(field(c, "iv")), hex(field(c, "aad")), hex(field(c, "msg")), hex(field(c, "ct")), hex(field(c, "tag")), field(c, "result")});
            }
        }
        return cases;
    }

    // The failures as a count, each case named in the report
    template<class Aead>
    size_t run_aead(const std::vector<AeadCase>& cases, size_t key_size_filter = 0) {
        size_t failures = 0;
        for (auto& c : cases) {
            if (key_size_filter && c.key.size() != key_size_filter) {
                continue;
            }
            auto key = Aead::from_key(c.key);
            if (!key) {
                // a key of a size the cipher does not take: only an invalid case may have one
                if (c.result == "valid") {
                    ADD_FAILURE() << "tcId " << c.id << ": the key was refused";
                    ++failures;
                }
                continue;
            }
            if (c.iv.size() != Aead::nonce_size) {
                continue;
            }
            bytes sealed = concat(c.ct, c.tag);
            auto opened = key->open(c.iv, sealed, c.aad);
            if (c.result == "valid") {
                auto s = key->seal(c.iv, c.msg, c.aad);
                bool ok = opened.has_value() && to_hex(*opened) == to_hex(c.msg) && to_hex(s) == to_hex(sealed);
                if (!ok) {
                    ADD_FAILURE() << "tcId " << c.id << ": a valid case failed";
                    ++failures;
                }
            } else if (c.result == "invalid") {
                if (opened.has_value()) {
                    ADD_FAILURE() << "tcId " << c.id << ": an invalid case opened";
                    ++failures;
                }
            }
        }
        return failures;
    }

    std::string read_vectors(const char* name) {
        const char* home = std::getenv("HOME");
        if (!home) {
            return {};
        }
        std::ifstream in(std::string(home) + "/Programming/oracles/wycheproof/testvectors_v1/" + name, std::ios::binary);
        if (!in) {
            return {};
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
}

// The reader on a document of Wycheproof's shape: RFC 8439's AEAD vector as
// valid, the same with its tag damaged as invalid, and a group of another
// IV size skipped
TEST(CryptoWycheproof_Tests, ReaderOnADocumentOfTheirFormat) {
    const std::string doc = R"({
      "algorithm": "CHACHA20-POLY1305",
      "schema": "aead_test_schema_v1.json",
      "numberOfTests": 3,
      "testGroups": [
        {
          "ivSize": 96, "keySize": 256, "tagSize": 128, "type": "AeadTest",
          "tests": [
            {
              "tcId": 1, "comment": "RFC 8439", "flags": ["Ktv"],
              "key": "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f",
              "iv": "070000004041424344454647",
              "aad": "50515253c0c1c2c3c4c5c6c7",
              "msg": "4c616469657320616e642047656e746c656d656e206f662074686520636c617373206f66202739393a204966204920636f756c64206f6666657220796f75206f6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73637265656e20776f756c642062652069742e",
              "ct": "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116",
              "tag": "1ae10b594f09e26a7e902ecbd0600691",
              "result": "valid"
            },
            {
              "tcId": 2, "comment": "modified tag", "flags": ["ModifiedTag"],
              "key": "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f",
              "iv": "070000004041424344454647",
              "aad": "50515253c0c1c2c3c4c5c6c7",
              "msg": "4c616469657320616e642047656e746c656d656e206f662074686520636c617373206f66202739393a204966204920636f756c64206f6666657220796f75206f6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73637265656e20776f756c642062652069742e",
              "ct": "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116",
              "tag": "1ae10b594f09e26a7e902ecbd0600690",
              "result": "invalid"
            }
          ]
        },
        {
          "ivSize": 64, "keySize": 256, "tagSize": 128, "type": "AeadTest",
          "tests": [ { "tcId": 3, "comment": "", "flags": [], "key": "", "iv": "", "aad": "", "msg": "", "ct": "", "tag": "", "result": "invalid" } ]
        }
      ]
    })";
    size_t skipped = 0;
    auto cases = load_aead(doc, 96, skipped);
    ASSERT_EQ(cases.size(), 2u);
    EXPECT_EQ(skipped, 1u);
    EXPECT_EQ(cases[0].id, 1);
    EXPECT_EQ(cases[1].result, "invalid");
    EXPECT_EQ(run_aead<crypto::chacha20_poly1305>(cases), 0u);
}

TEST(CryptoWycheproof_Tests, AesGcm) {
    auto text = read_vectors("aes_gcm_test.json");
    if (text.empty()) {
        GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/aes_gcm_test.json";
    }
    size_t skipped = 0;
    auto cases = load_aead(text, 96, skipped);
    EXPECT_GT(cases.size(), 0u);
    EXPECT_EQ(run_aead<crypto::aes_gcm>(cases), 0u);
    std::printf("[ wycheproof ] aes_gcm: %zu cases, %zu skipped (IV or tag size)\n", cases.size(), skipped);
}

TEST(CryptoWycheproof_Tests, ChachaPoly) {
    auto text = read_vectors("chacha20_poly1305_test.json");
    if (text.empty()) {
        GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/chacha20_poly1305_test.json";
    }
    size_t skipped = 0;
    auto cases = load_aead(text, 96, skipped);
    EXPECT_GT(cases.size(), 0u);
    EXPECT_EQ(run_aead<crypto::chacha20_poly1305>(cases), 0u);
}

TEST(CryptoWycheproof_Tests, XChachaPoly) {
    auto text = read_vectors("xchacha20_poly1305_test.json");
    if (text.empty()) {
        GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/xchacha20_poly1305_test.json";
    }
    size_t skipped = 0;
    auto cases = load_aead(text, 192, skipped);
    EXPECT_GT(cases.size(), 0u);
    EXPECT_EQ(run_aead<crypto::xchacha20_poly1305>(cases), 0u);
}
