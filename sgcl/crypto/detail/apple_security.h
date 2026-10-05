//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(__APPLE__)

#include "../../core/detail/os.h"

#include <cstddef>
#include <cstdint>

// The few C functions of Security.framework and CoreFoundation that the
// system's root certificates are read with on macOS, declared here under
// names of their own and bound to the system's symbols by asm labels,
// rather than through the SDK's headers (the rule for platform headers:
// CoreFoundation's would bring MacTypes' Point, Rect, Byte and the rest into
// every program that includes sgcl/crypto). The objects are opaque pointers;
// no structure crosses the ABI. tests/crypto/x509_keychain_platform.cpp holds
// the declarations, the constants and the symbols against the SDK's headers
// (checked against the macOS 26 SDK). The functions CoreFoundation shares
// with codec's declarations (sgcl/codec/detail/apple_imageio.h) are declared
// with the same names and types: a function of C linkage is one function
// whatever namespace declares it. The frameworks are linked with the library
// on Apple's systems (CMake).
namespace sgcl::crypto::detail::apple {
    using Ref = const void*;      // any CoreFoundation object: CFArrayRef, SecCertificateRef…
    using Index = long;           // CFIndex
    using Status = int32_t;       // OSStatus

    // SecTrustSettingsDomain
    inline constexpr uint32_t DomainUser = 0;
    inline constexpr uint32_t DomainAdmin = 1;
    inline constexpr uint32_t DomainSystem = 2;

    // SecTrustSettingsResult
    inline constexpr int32_t ResultTrustRoot = 1;
    inline constexpr int32_t ResultTrustAsRoot = 2;
    inline constexpr int32_t ResultDeny = 3;
    inline constexpr int32_t ResultUnspecified = 4;

    // CFNumberType, CFStringEncoding
    inline constexpr Index NumberSInt32 = 3;
    inline constexpr uint32_t Utf8 = 0x08000100;

    extern "C" {
        // CoreFoundation
        void release(Ref object) __asm__("_CFRelease");
        const uint8_t* data_bytes(Ref data) __asm__("_CFDataGetBytePtr");
        Index data_length(Ref data) __asm__("_CFDataGetLength");
        Ref dictionary_value(Ref dictionary, Ref key) __asm__("_CFDictionaryGetValue");
        bool number_value(Ref number, Index type, void* value) __asm__("_CFNumberGetValue");
        Ref string_create(Ref allocator, const char* text, uint32_t encoding) __asm__("_CFStringCreateWithCString");
        bool equal(Ref a, Ref b) __asm__("_CFEqual");
        Index array_count(Ref array) __asm__("_CFArrayGetCount");
        Ref array_value(Ref array, Index index) __asm__("_CFArrayGetValueAtIndex");

        // Security
        Status trust_copy_anchor_certificates(Ref* anchors) __asm__("_SecTrustCopyAnchorCertificates");
        Status trust_settings_copy_certificates(uint32_t domain, Ref* certificates) __asm__("_SecTrustSettingsCopyCertificates");
        Status trust_settings_copy_trust_settings(Ref certificate, uint32_t domain, Ref* settings) __asm__("_SecTrustSettingsCopyTrustSettings");
        Ref certificate_copy_data(Ref certificate) __asm__("_SecCertificateCopyData");
        Ref policy_copy_properties(Ref policy) __asm__("_SecPolicyCopyProperties");
        extern const Ref policy_oid __asm__("_kSecPolicyOid");
        extern const Ref policy_apple_ssl __asm__("_kSecPolicyAppleSSL");
    }

    // The calls the reading of the roots makes, as pointers: the system's
    // by default, a test's own to make them fail
    struct KeychainCalls {
        Status (*copy_anchors)(Ref*) = &trust_copy_anchor_certificates;
        Status (*copy_certificates)(uint32_t, Ref*) = &trust_settings_copy_certificates;
        Status (*copy_trust_settings)(Ref, uint32_t, Ref*) = &trust_settings_copy_trust_settings;
        Ref (*copy_data)(Ref) = &certificate_copy_data;
    };

    // A CoreFoundation object released when it goes
    class Owned {
    public:
        SGCL_INLINE_HOT explicit Owned(Ref r = nullptr) noexcept
        : _r(r) {
        }

        Owned(const Owned&) = delete;
        Owned& operator=(const Owned&) = delete;

        SGCL_INLINE_HOT ~Owned() {
            if (_r) {
                release(_r);
            }
        }

        SGCL_INLINE_HOT Ref get() const noexcept {
            return _r;
        }

        SGCL_INLINE_HOT Ref* out() noexcept {
            return &_r;
        }

    private:
        Ref _r;
    };

    // What the trust settings of a certificate in a domain say for TLS
    // servers: 1 trusted (as a root, or as an anchor below one), -1 denied,
    // 0 nothing (no setting for SSL, or Unspecified). An empty list is
    // "trusted as a root for everything" (SecTrustSettings.h); a setting of
    // another policy, of an application or of a policy string (a host name)
    // says nothing here, as it says nothing of every TLS connection
    inline int trust_decision(const KeychainCalls& calls, Ref certificate, uint32_t domain) noexcept {
        Owned settings;
        if (calls.copy_trust_settings(certificate, domain, settings.out()) != 0 || !settings.get()) {
            return 0;
        }
        const Index n = array_count(settings.get());
        if (n == 0) {
            return 1;
        }
        Owned k_policy(string_create(nullptr, "kSecTrustSettingsPolicy", Utf8));
        Owned k_application(string_create(nullptr, "kSecTrustSettingsApplication", Utf8));
        Owned k_policy_string(string_create(nullptr, "kSecTrustSettingsPolicyString", Utf8));
        Owned k_result(string_create(nullptr, "kSecTrustSettingsResult", Utf8));
        for (Index i = 0; i < n; ++i) {
            Ref d = array_value(settings.get(), i);
            if (!d || dictionary_value(d, k_application.get()) || dictionary_value(d, k_policy_string.get())) {
                continue;
            }
            if (Ref policy = dictionary_value(d, k_policy.get())) {
                Owned properties(policy_copy_properties(policy));
                Ref oid = properties.get() ? dictionary_value(properties.get(), policy_oid) : nullptr;
                if (!oid || !equal(oid, policy_apple_ssl)) {
                    continue;
                }
            }
            int32_t result = ResultTrustRoot;
            if (Ref r = dictionary_value(d, k_result.get())) {
                if (!number_value(r, NumberSInt32, &result)) {
                    continue;
                }
            }
            if (result == ResultTrustRoot || result == ResultTrustAsRoot) {
                return 1;
            }
            if (result == ResultDeny) {
                return -1;
            }
        }
        return 0;
    }

    // The DER of a SecCertificateRef, to f(bytes, size)
    template<class F>
    bool with_der(const KeychainCalls& calls, Ref certificate, F&& f) {
        Owned data(calls.copy_data(certificate));
        if (!data.get()) {
            return false;
        }
        f(data_bytes(data.get()), size_t(data_length(data.get())));
        return true;
    }
}

#endif
