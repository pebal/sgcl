//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The system's roots on macOS, from the Keychain's trust settings
// (Security.framework): the declarations of sgcl/crypto/detail/apple_security.h
// against the SDK's headers (a test may include them: the rule for platform
// headers binds the library), the pool the system's anchors make, the
// decisions of an administrator's or a user's trust settings (made here with
// CoreFoundation's own objects behind the calls), and the files as the
// fallback when Security cannot be read.
#if defined(__APPLE__)

#include <gtest/gtest.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "sgcl/crypto/x509.h"

#include <dlfcn.h>
#include <string>

namespace apple = sgcl::crypto::detail::apple;
namespace x509 = sgcl::crypto::x509;

// the types and the constants
static_assert(sizeof(apple::Index) == sizeof(CFIndex));
static_assert(sizeof(apple::Status) == sizeof(OSStatus));
static_assert(apple::DomainUser == kSecTrustSettingsDomainUser);
static_assert(apple::DomainAdmin == kSecTrustSettingsDomainAdmin);
static_assert(apple::DomainSystem == kSecTrustSettingsDomainSystem);
static_assert(apple::ResultTrustRoot == kSecTrustSettingsResultTrustRoot);
static_assert(apple::ResultTrustAsRoot == kSecTrustSettingsResultTrustAsRoot);
static_assert(apple::ResultDeny == kSecTrustSettingsResultDeny);
static_assert(apple::ResultUnspecified == kSecTrustSettingsResultUnspecified);
static_assert(apple::NumberSInt32 == kCFNumberSInt32Type);
static_assert(apple::Utf8 == kCFStringEncodingUTF8);

namespace {
    // the addresses compared as the loader resolved the symbol (dlsym): two
    // declarations are two functions to the optimizer, which may fold an ==
    // between them to false, so each goes through a volatile first
    template<class A, class B>
    bool same_symbol(A* ours, B* theirs, const char* name) {
        const void* symbol = dlsym(RTLD_DEFAULT, name);
        const void* volatile a = reinterpret_cast<const void*>(ours);
        const void* volatile b = reinterpret_cast<const void*>(theirs);
        return symbol != nullptr && a == symbol && b == symbol;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST(Crypto_X509_Keychain, DeclarationsAreTheSdks) {
    EXPECT_TRUE(same_symbol(&apple::release, &CFRelease, "CFRelease"));
    EXPECT_TRUE(same_symbol(&apple::data_bytes, &CFDataGetBytePtr, "CFDataGetBytePtr"));
    EXPECT_TRUE(same_symbol(&apple::data_length, &CFDataGetLength, "CFDataGetLength"));
    EXPECT_TRUE(same_symbol(&apple::dictionary_value, &CFDictionaryGetValue, "CFDictionaryGetValue"));
    EXPECT_TRUE(same_symbol(&apple::number_value, &CFNumberGetValue, "CFNumberGetValue"));
    EXPECT_TRUE(same_symbol(&apple::string_create, &CFStringCreateWithCString, "CFStringCreateWithCString"));
    EXPECT_TRUE(same_symbol(&apple::equal, &CFEqual, "CFEqual"));
    EXPECT_TRUE(same_symbol(&apple::array_count, &CFArrayGetCount, "CFArrayGetCount"));
    EXPECT_TRUE(same_symbol(&apple::array_value, &CFArrayGetValueAtIndex, "CFArrayGetValueAtIndex"));
    EXPECT_TRUE(same_symbol(&apple::trust_copy_anchor_certificates, &SecTrustCopyAnchorCertificates, "SecTrustCopyAnchorCertificates"));
    EXPECT_TRUE(same_symbol(&apple::trust_settings_copy_certificates, &SecTrustSettingsCopyCertificates, "SecTrustSettingsCopyCertificates"));
    EXPECT_TRUE(same_symbol(&apple::trust_settings_copy_trust_settings, &SecTrustSettingsCopyTrustSettings, "SecTrustSettingsCopyTrustSettings"));
    EXPECT_TRUE(same_symbol(&apple::certificate_copy_data, &SecCertificateCopyData, "SecCertificateCopyData"));
    EXPECT_TRUE(same_symbol(&apple::policy_copy_properties, &SecPolicyCopyProperties, "SecPolicyCopyProperties"));
    EXPECT_EQ(apple::policy_oid, (const void*)kSecPolicyOid);
    EXPECT_EQ(apple::policy_apple_ssl, (const void*)kSecPolicyAppleSSL);
    // the keys made at run time are the SDK's CFSTR constants
    CFStringRef result = CFStringCreateWithCString(nullptr, "kSecTrustSettingsResult", kCFStringEncodingUTF8);
    EXPECT_TRUE(CFEqual(result, kSecTrustSettingsResult));
    CFRelease(result);
}
#pragma clang diagnostic pop

namespace {
    bool has_subject(const x509::certificate_pool& pool, std::string_view subject) {
        for (const auto& c : pool.certificates()) {
            if (c.subject().to_string().view() == subject) {
                return true;
            }
        }
        return false;
    }

    // The calls the fakes stand for: the system's anchors as they are, an
    // administrator's settings made here
    CFArrayRef fake_admin_certs = nullptr;
    CFArrayRef fake_settings = nullptr;

    apple::Status fail_anchors(apple::Ref* out) {
        *out = nullptr;
        return -25300;   // errSecItemNotFound
    }

    apple::Status admin_certificates(uint32_t domain, apple::Ref* out) {
        if (domain != apple::DomainAdmin || !fake_admin_certs) {
            *out = nullptr;
            return -25263;   // errSecNoTrustSettings
        }
        CFRetain(fake_admin_certs);
        *out = fake_admin_certs;
        return 0;
    }

    apple::Status admin_settings(apple::Ref, uint32_t domain, apple::Ref* out) {
        if (domain != apple::DomainAdmin || !fake_settings) {
            *out = nullptr;
            return -25300;
        }
        CFRetain(fake_settings);
        *out = fake_settings;
        return 0;
    }

    // A trust settings array of one dictionary: the result given, the SSL
    // policy or another, or no result key (TrustRoot by default)
    CFArrayRef settings_of(int result, CFStringRef policy_oid = nullptr) {
        CFMutableDictionaryRef d = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (result) {
            SInt32 r = result;
            CFNumberRef n = CFNumberCreate(nullptr, kCFNumberSInt32Type, &r);
            CFDictionarySetValue(d, kSecTrustSettingsResult, n);
            CFRelease(n);
        }
        if (policy_oid) {
            SecPolicyRef p = policy_oid == kSecPolicyAppleSSL ? SecPolicyCreateSSL(true, nullptr) : SecPolicyCreateBasicX509();
            CFDictionarySetValue(d, kSecTrustSettingsPolicy, p);
            CFRelease(p);
        }
        const void* one = d;
        CFArrayRef a = CFArrayCreate(nullptr, &one, 1, &kCFTypeArrayCallBacks);
        CFRelease(d);
        return a;
    }

    // The first of the system's anchors, as a SecCertificateRef, and its
    // subject
    SecCertificateRef first_anchor(std::string& subject) {
        CFArrayRef anchors = nullptr;
        if (SecTrustCopyAnchorCertificates(&anchors) != 0 || !anchors || CFArrayGetCount(anchors) == 0) {
            return nullptr;
        }
        auto c = (SecCertificateRef)CFArrayGetValueAtIndex(anchors, 0);
        CFRetain(c);
        CFDataRef der = SecCertificateCopyData(c);
        auto parsed = x509::certificate::parse(sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(CFDataGetBytePtr(der)), size_t(CFDataGetLength(der))));
        subject = parsed ? std::string(parsed->subject().to_string().view()) : std::string();
        CFRelease(der);
        CFRelease(anchors);
        return c;
    }
}

// The system's pool on macOS is the Keychain's: not empty, the roots of the
// system there (ISRG's, which the bundle of /etc/ssl/cert.pem has too)
TEST(Crypto_X509_Keychain, SystemPoolFromTheKeychain) {
    x509::certificate_pool pool;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(pool));
    EXPECT_GT(pool.size(), 50u);
    EXPECT_TRUE(has_subject(pool, "CN=ISRG Root X1,O=Internet Security Research Group,C=US"));
    // the files the system keeps hold most of the same roots (the bundle
    // is older than the Keychain's: 95 of its 128 on macOS 26)
    x509::certificate_pool files;
    sgcl::crypto::x509::detail::load_root_files(files, false);
    size_t shared = 0;
    for (const auto& c : files.certificates()) {
        shared += pool.contains(c);
    }
    EXPECT_GT(shared * 2, files.size()) << shared << " of " << files.size();
    // and certificate_pool::system() is it, unless SSL_CERT_FILE or SSL_CERT_DIR says otherwise
    if (!getenv("SSL_CERT_FILE") && !getenv("SSL_CERT_DIR")) {
        auto sys = x509::certificate_pool::system();
        ASSERT_TRUE(sys.has_value());
        EXPECT_EQ(sys->size(), pool.size());
        EXPECT_TRUE(has_subject(*sys, "CN=ISRG Root X1,O=Internet Security Research Group,C=US"));
    }
}

// Security that cannot be read: no pool from the Keychain, the files instead
TEST(Crypto_X509_Keychain, FallbackWhenSecurityFails) {
    apple::KeychainCalls calls;
    calls.copy_anchors = &fail_anchors;
    x509::certificate_pool pool;
    EXPECT_FALSE(sgcl::crypto::x509::detail::load_keychain_roots(pool, calls));
    EXPECT_TRUE(pool.empty());
    sgcl::crypto::x509::detail::load_root_files(pool, false);
    EXPECT_GT(pool.size(), 50u);   // /etc/ssl/cert.pem
    EXPECT_TRUE(has_subject(pool, "CN=ISRG Root X1,O=Internet Security Research Group,C=US"));
}

// An administrator's settings: a root distrusted leaves the pool, a policy
// of another kind says nothing, trusted for SSL keeps it
TEST(Crypto_X509_Keychain, TrustSettingsDecide) {
    std::string subject;
    SecCertificateRef anchor = first_anchor(subject);
    ASSERT_NE(anchor, nullptr);
    ASSERT_FALSE(subject.empty());
    const void* one = anchor;
    fake_admin_certs = CFArrayCreate(nullptr, &one, 1, &kCFTypeArrayCallBacks);
    apple::KeychainCalls calls;
    calls.copy_certificates = &admin_certificates;
    calls.copy_trust_settings = &admin_settings;
    x509::certificate_pool all;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(all));

    fake_settings = settings_of(kSecTrustSettingsResultDeny);
    x509::certificate_pool denied;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(denied, calls));
    EXPECT_EQ(denied.size() + 1, all.size());
    EXPECT_FALSE(has_subject(denied, subject)) << subject;
    CFRelease(fake_settings);

    fake_settings = settings_of(kSecTrustSettingsResultDeny, kSecPolicyAppleX509Basic);   // another policy: nothing for TLS
    x509::certificate_pool other;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(other, calls));
    EXPECT_EQ(other.size(), all.size());
    CFRelease(fake_settings);

    fake_settings = settings_of(kSecTrustSettingsResultDeny, kSecPolicyAppleSSL);
    x509::certificate_pool ssl_denied;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(ssl_denied, calls));
    EXPECT_EQ(ssl_denied.size() + 1, all.size());
    CFRelease(fake_settings);

    fake_settings = settings_of(0);   // no result: TrustRoot
    x509::certificate_pool trusted;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(trusted, calls));
    EXPECT_EQ(trusted.size(), all.size());
    CFRelease(fake_settings);

    fake_settings = settings_of(kSecTrustSettingsResultUnspecified);
    x509::certificate_pool unspecified;
    ASSERT_TRUE(sgcl::crypto::x509::detail::load_keychain_roots(unspecified, calls));
    EXPECT_EQ(unspecified.size(), all.size());
    CFRelease(fake_settings);
    fake_settings = nullptr;
    CFRelease(fake_admin_certs);
    fake_admin_certs = nullptr;
    CFRelease(anchor);
}

// A root added by the user to a keychain of its own with `security
// add-trusted-cert` changes the user's trust settings, which macOS confirms
// with a dialog (an authorization prompt): never in a test run
TEST(Crypto_X509_Keychain, UserAddedRootNeedsADialog) {
    GTEST_SKIP() << "security add-trusted-cert asks for the user's password in a dialog; the decisions are tested with fakes (TrustSettingsDecide)";
}

#endif
