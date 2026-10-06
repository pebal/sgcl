//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of mail authentication share: the DNS server of the
// loopback (tests/net/dns_server.h) with a zone of the test's, the records
// of DKIM keys, SPF and DMARC made into TXT records of it.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/dns_server.h"
#include "sgcl/net/smtp.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace mail_test {
    using namespace dns_test;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    inline std::string run_command(const std::string& cmd, int* status = nullptr) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        const int st = pclose(p);
        if (status) {
            *status = st;
        }
        return out;
    }

    inline bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    // A directory of the test's own under the system's temporary one
    inline std::filesystem::path temp_dir(const std::string& name) {
        auto p = std::filesystem::temp_directory_path() / ("sgcl-mail-" + name + "-" + std::to_string(::getpid()));
        std::filesystem::remove_all(p);
        std::filesystem::create_directories(p);
        return p;
    }

    inline void write_file(const std::filesystem::path& p, const std::string& text) {
        std::ofstream(p, std::ios::binary | std::ios::trunc) << text;
    }

    // A TXT record of a long text, cut into strings of 255 bytes as a zone
    // file has it
    inline Rr txt_rr(const std::string& owner, const std::string& text) {
        std::vector<std::string> parts;
        for (size_t at = 0; at < text.size(); at += 255) {
            parts.push_back(text.substr(at, 255));
        }
        if (parts.empty()) {
            parts.push_back(std::string());
        }
        return rr_txt(owner, parts);
    }

    // RFC 8463 Appendix A: the message signed with both algorithms, and
    // the keys' records
    inline const char* rfc8463_message =
        "DKIM-Signature: v=1; a=ed25519-sha256; c=relaxed/relaxed;\r\n"
        " d=football.example.com; i=@football.example.com;\r\n"
        " q=dns/txt; s=brisbane; t=1528637909; h=from : to :\r\n"
        " subject : date : message-id : from : subject : date;\r\n"
        " bh=2jUSOH9NhtVGCQWNr9BrIAPreKQjO6Sn7XIkfJVOzv8=;\r\n"
        " b=/gCrinpcQOoIfuHNQIbq4pgh9kyIK3AQUdt9OdqQehSwhEIug4D11Bus\r\n"
        " Fa3bT3FY5OsU7ZbnKELq+eXdp1Q1Dw==\r\n"
        "DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed;\r\n"
        " d=football.example.com; i=@football.example.com;\r\n"
        " q=dns/txt; s=test; t=1528637909; h=from : to : subject :\r\n"
        " date : message-id : from : subject : date;\r\n"
        " bh=2jUSOH9NhtVGCQWNr9BrIAPreKQjO6Sn7XIkfJVOzv8=;\r\n"
        " b=F45dVWDfMbQDGHJFlXUNB2HKfbCeLRyhDXgFpEL8GwpsRe0IeIixNTe3\r\n"
        " DhCVlUrSjV4BwcVcOF6+FF3Zo9Rpo1tFOeS9mPYQTnGdaSGsgeefOsk2Jz\r\n"
        " dA+L10TeYt9BgDfQNZtKdN1WO//KgIqXP7OdEFE4LjFYNcUxZQ4FADY+8=\r\n"
        "From: Joe SixPack <joe@football.example.com>\r\n"
        "To: Suzie Q <suzie@shopping.example.net>\r\n"
        "Subject: Is dinner ready?\r\n"
        "Date: Fri, 11 Jul 2003 21:00:37 -0700 (PDT)\r\n"
        "Message-ID: <20030712040037.46341.5F8J@football.example.com>\r\n"
        "\r\n"
        "Hi.\r\n"
        "\r\n"
        "We lost the game.  Are you hungry yet?\r\n"
        "\r\n"
        "Joe.\r\n";

    inline const char* rfc8463_ed25519_record = "v=DKIM1; k=ed25519; p=11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";
    inline const char* rfc8463_rsa_record =
        "v=DKIM1; k=rsa; p=MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDkHlOQoBTzWRiGs5V6NpP3idY6Wk08a5qhdR6wy5bdOKb2jLQ"
        "iY/J16JYi0Qvx/byYzCNb3W91y3FutACDfzwQ/BC/e/8uBsCR+yz1Lxj+PL6lHvqMKrM3rG4hstT5QjvHO9PzoxZyVYLzBfO2EeC3Ip3G+"
        "2kryOTIKT+l/K4w3QIDAQAB";
    // the Ed25519 secret key of RFC 8463 A.1 (its seed)
    inline const char* rfc8463_ed25519_seed = "nWGxne/9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A=";

    inline const char* plain_message =
        "From: Alice <alice@example.com>\r\n"
        "To: Bob <bob@example.org>\r\n"
        "Subject: Lunch\r\n"
        "Date: Tue, 06 Oct 2026 12:00:00 +0000\r\n"
        "Message-ID: <1@example.com>\r\n"
        "\r\n"
        "Shall we meet at noon?\r\n"
        "\r\n"
        "Alice\r\n";
}
