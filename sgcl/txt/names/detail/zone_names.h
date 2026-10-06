//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lookup.h"
#include "zones.h"

#include <cstdint>
#include <string_view>

// The time zones of the display names: a zone's canonical index by any of its
// names, the metazone it uses today. Read by the dates of sgcl/time.
namespace sgcl::txt::detail::names {
    // The canonical index of a zone by any of its names (the IANA one or
    // CLDR's), or UINT32_MAX
    inline uint32_t zone_index(std::string_view id) noexcept {
        using namespace cldr;
        size_t lo = 0, n = std::size(ZoneAliasNames);
        while (n > 0) {
            size_t half = n / 2;
            if (ZoneAliasTexts[ZoneAliasNames[lo + half]] < id) {
                lo += half + 1;
                n -= half + 1;
            } else {
                n = half;
            }
        }
        if (lo < std::size(ZoneAliasNames) && ZoneAliasTexts[ZoneAliasNames[lo]] == id) {
            return ZoneAliasZones[lo];
        }
        return UINT32_MAX;
    }

    // The metazone a zone uses today, or UINT32_MAX
    inline uint32_t metazone_of(uint32_t zone) noexcept {
        if (zone >= std::size(cldr::ZoneMetazones) || !cldr::ZoneMetazones[zone]) {
            return UINT32_MAX;
        }
        return cldr::ZoneMetazones[zone] - 1u;
    }

}
