//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the OAuth 2.0 and OpenID Connect tests share: the authorization
// server of Go's standard library (go_oauth/main.go), built once a run and
// started for a test, and the browser's step of the authorization.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace oauth2_test {
    using namespace sgcl;

    inline std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The Go server, built once and run for a test; "" base when there is no go
    struct GoAs {
        FILE* p = nullptr;
        std::string base;

        GoAs() {
            static std::string bin = [] {
                if (std::system("command -v go > /dev/null 2>&1") != 0) {
                    return std::string();
                }
                auto src = source_root() / "tests/net/oauth2/go_oauth/main.go";
                auto out = std::filesystem::temp_directory_path() / "sgcl_go_oauth";
                std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
                return std::system(cmd.c_str()) == 0 ? out.string() : std::string();
            }();
            if (bin.empty()) {
                return;
            }
            p = popen(("'" + bin + "'").c_str(), "r");
            char line[64] = {};
            if (!p || !fgets(line, sizeof line, p)) {
                return;
            }
            base = "http://127.0.0.1:" + std::to_string(std::atoi(line + 5));
        }

        ~GoAs() {
            if (!base.empty()) {
                net::http::client c;
                (void)c.get(sgcl::string(base + "/quit"));
            }
            if (p) {
                pclose(p);
            }
        }

        net::oauth2::config config(const char* id, const char* secret) const {
            net::oauth2::config c;
            c.client_id = id;
            c.client_secret = secret;
            c.endpoints.authorization = sgcl::string(base + "/authorize");
            c.endpoints.token = sgcl::string(base + "/token");
            c.endpoints.device_authorization = sgcl::string(base + "/device_authorization");
            c.endpoints.revocation = sgcl::string(base + "/revoke");
            c.endpoints.introspection = sgcl::string(base + "/introspect");
            c.redirect_url = "http://127.0.0.1:1/callback";
            c.scopes = {sgcl::string("read"), sgcl::string("write")};
            return c;
        }

        // The authorization step a browser would make: the redirect's
        // Location read, its code and state (the client told not to follow it)
        std::pair<std::string, std::string> authorize(const sgcl::string& url) const {
            net::http::client c;
            c.follow_redirects = false;
            auto res = c.get(url);
            if (!res || res->status() != 302) {
                return {};
            }
            auto loc = net::url::parse(res->header("Location"));
            if (!loc) {
                return {};
            }
            return {text(loc->query_params().get("code")), text(loc->query_params().get("state"))};
        }
    };
}
