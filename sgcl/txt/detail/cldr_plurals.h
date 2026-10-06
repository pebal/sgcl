//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once
#include "cldr.h"

#include <cstdint>

// The plural rules of CLDR 46.0 (supplemental/plurals.xml and ordinals.xml),
// compiled into C++ by tools/cldr_tables.py: do not edit. Every distinct set
// of rules is one function of the operands of TR35 §5.1. Included by
// sgcl/txt/plural.h after txt::plural, PluralOperands and PluralRule.
namespace sgcl::txt::detail::cldr {
    inline txt::plural Cardinal0(const PluralOperands& o) noexcept {
        (void)o;
        return txt::plural::other;
    }

    inline txt::plural Cardinal1(const PluralOperands& o) noexcept {
        if (o.i == 0 || (!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal2(const PluralOperands& o) noexcept {
        if ((o.i == 0 || o.i == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal3(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal4(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 0 || o.i == 1)) || (o.i == 0 && o.f == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal5(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i <= 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal6(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i <= 1) || (!o.fraction && (o.i >= 11 && o.i <= 99))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal7(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal8(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1) || (!(o.t == 0) && (o.i == 0 || o.i == 1))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal9(const PluralOperands& o) noexcept {
        if ((o.t == 0 && o.i % 10 == 1 && !(o.i % 100 == 11)) || (o.t % 10 == 1 && !(o.t % 100 == 11))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal10(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 10 == 1 && !(o.i % 100 == 11)) || (o.f % 10 == 1 && !(o.f % 100 == 11))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal11(const PluralOperands& o) noexcept {
        if ((o.v == 0 && (o.i == 1 || o.i == 2 || o.i == 3)) || (o.v == 0 && !(o.i % 10 == 4 || o.i % 10 == 6 || o.i % 10 == 9)) || (!(o.v == 0) && !(o.f % 10 == 4 || o.f % 10 == 6 || o.f % 10 == 9))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal12(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i % 10 == 0) || (!o.fraction && (o.i % 100 >= 11 && o.i % 100 <= 19)) || (o.v == 2 && (o.f % 100 >= 11 && o.f % 100 <= 19))) {
            return txt::plural::zero;
        }
        if (((!o.fraction && o.i % 10 == 1) && (o.fraction || !(o.i % 100 == 11))) || (o.v == 2 && o.f % 10 == 1 && !(o.f % 100 == 11)) || (!(o.v == 2) && o.f % 10 == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal13(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 0)) {
            return txt::plural::zero;
        }
        if (((o.i == 0 || o.i == 1) && (o.fraction || !(o.i == 0)))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal14(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 0)) {
            return txt::plural::zero;
        }
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal15(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0) || (o.i == 0 && !(o.v == 0))) {
            return txt::plural::one;
        }
        if ((o.i == 2 && o.v == 0)) {
            return txt::plural::two;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal16(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal17(const PluralOperands& o) noexcept {
        if (o.i == 0 || (!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i >= 2 && o.i <= 10))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal18(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0)) {
            return txt::plural::one;
        }
        if (!(o.v == 0) || (!o.fraction && o.i == 0) || ((o.fraction || !(o.i == 1)) && (!o.fraction && (o.i % 100 >= 1 && o.i % 100 <= 19)))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal19(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 10 == 1 && !(o.i % 100 == 11)) || (o.f % 10 == 1 && !(o.f % 100 == 11))) {
            return txt::plural::one;
        }
        if ((o.v == 0 && (o.i % 10 >= 2 && o.i % 10 <= 4) && !((o.i % 100 >= 12 && o.i % 100 <= 14))) || ((o.f % 10 >= 2 && o.f % 10 <= 4) && !((o.f % 100 >= 12 && o.f % 100 <= 14)))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal20(const PluralOperands& o) noexcept {
        if ((o.i == 0 || o.i == 1)) {
            return txt::plural::one;
        }
        if ((o.e == 0 && !(o.i == 0) && o.i % 1000000 == 0 && o.v == 0) || !(o.e <= 5)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal21(const PluralOperands& o) noexcept {
        if (o.i <= 1) {
            return txt::plural::one;
        }
        if ((o.e == 0 && !(o.i == 0) && o.i % 1000000 == 0 && o.v == 0) || !(o.e <= 5)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal22(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0)) {
            return txt::plural::one;
        }
        if ((o.e == 0 && !(o.i == 0) && o.i % 1000000 == 0 && o.v == 0) || !(o.e <= 5)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal23(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((o.e == 0 && !(o.i == 0) && o.i % 1000000 == 0 && o.v == 0) || !(o.e <= 5)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal24(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 11))) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 12))) {
            return txt::plural::two;
        }
        if ((!o.fraction && ((o.i >= 3 && o.i <= 10) || (o.i >= 13 && o.i <= 19)))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal25(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 100 == 1)) {
            return txt::plural::one;
        }
        if ((o.v == 0 && o.i % 100 == 2)) {
            return txt::plural::two;
        }
        if ((o.v == 0 && (o.i % 100 >= 3 && o.i % 100 <= 4)) || !(o.v == 0)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal26(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 100 == 1) || o.f % 100 == 1) {
            return txt::plural::one;
        }
        if ((o.v == 0 && o.i % 100 == 2) || o.f % 100 == 2) {
            return txt::plural::two;
        }
        if ((o.v == 0 && (o.i % 100 >= 3 && o.i % 100 <= 4)) || (o.f % 100 >= 3 && o.f % 100 <= 4)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal27(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0)) {
            return txt::plural::one;
        }
        if (((o.i >= 2 && o.i <= 4) && o.v == 0)) {
            return txt::plural::few;
        }
        if (!(o.v == 0)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal28(const PluralOperands& o) noexcept {
        if ((o.i == 1 && o.v == 0)) {
            return txt::plural::one;
        }
        if ((o.v == 0 && (o.i % 10 >= 2 && o.i % 10 <= 4) && !((o.i % 100 >= 12 && o.i % 100 <= 14)))) {
            return txt::plural::few;
        }
        if ((o.v == 0 && !(o.i == 1) && o.i % 10 <= 1) || (o.v == 0 && (o.i % 10 >= 5 && o.i % 10 <= 9)) || (o.v == 0 && (o.i % 100 >= 12 && o.i % 100 <= 14))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal29(const PluralOperands& o) noexcept {
        if (((!o.fraction && o.i % 10 == 1) && (o.fraction || !(o.i % 100 == 11)))) {
            return txt::plural::one;
        }
        if (((!o.fraction && (o.i % 10 >= 2 && o.i % 10 <= 4)) && (o.fraction || !((o.i % 100 >= 12 && o.i % 100 <= 14))))) {
            return txt::plural::few;
        }
        if ((!o.fraction && o.i % 10 == 0) || (!o.fraction && (o.i % 10 >= 5 && o.i % 10 <= 9)) || (!o.fraction && (o.i % 100 >= 11 && o.i % 100 <= 14))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal30(const PluralOperands& o) noexcept {
        if (((!o.fraction && o.i % 10 == 1) && (o.fraction || !((o.i % 100 >= 11 && o.i % 100 <= 19))))) {
            return txt::plural::one;
        }
        if (((!o.fraction && (o.i % 10 >= 2 && o.i % 10 <= 9)) && (o.fraction || !((o.i % 100 >= 11 && o.i % 100 <= 19))))) {
            return txt::plural::few;
        }
        if (!(o.f == 0)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal31(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 10 == 1 && !(o.i % 100 == 11))) {
            return txt::plural::one;
        }
        if ((o.v == 0 && (o.i % 10 >= 2 && o.i % 10 <= 4) && !((o.i % 100 >= 12 && o.i % 100 <= 14)))) {
            return txt::plural::few;
        }
        if ((o.v == 0 && o.i % 10 == 0) || (o.v == 0 && (o.i % 10 >= 5 && o.i % 10 <= 9)) || (o.v == 0 && (o.i % 100 >= 11 && o.i % 100 <= 14))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal32(const PluralOperands& o) noexcept {
        if (((!o.fraction && o.i % 10 == 1) && (o.fraction || !((o.i % 100 == 11 || o.i % 100 == 71 || o.i % 100 == 91))))) {
            return txt::plural::one;
        }
        if (((!o.fraction && o.i % 10 == 2) && (o.fraction || !((o.i % 100 == 12 || o.i % 100 == 72 || o.i % 100 == 92))))) {
            return txt::plural::two;
        }
        if (((!o.fraction && ((o.i % 10 >= 3 && o.i % 10 <= 4) || o.i % 10 == 9)) && (o.fraction || !(((o.i % 100 >= 10 && o.i % 100 <= 19) || (o.i % 100 >= 70 && o.i % 100 <= 79) || (o.i % 100 >= 90 && o.i % 100 <= 99)))))) {
            return txt::plural::few;
        }
        if (((o.fraction || !(o.i == 0)) && (!o.fraction && o.i % 1000000 == 0))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal33(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 0) || (!o.fraction && (o.i % 100 >= 3 && o.i % 100 <= 10))) {
            return txt::plural::few;
        }
        if ((!o.fraction && (o.i % 100 >= 11 && o.i % 100 <= 19))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal34(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && (o.i >= 3 && o.i <= 6))) {
            return txt::plural::few;
        }
        if ((!o.fraction && (o.i >= 7 && o.i <= 10))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal35(const PluralOperands& o) noexcept {
        if ((o.v == 0 && o.i % 10 == 1)) {
            return txt::plural::one;
        }
        if ((o.v == 0 && o.i % 10 == 2)) {
            return txt::plural::two;
        }
        if ((o.v == 0 && (o.i % 100 == 0 || o.i % 100 == 20 || o.i % 100 == 40 || o.i % 100 == 60 || o.i % 100 == 80))) {
            return txt::plural::few;
        }
        if (!(o.v == 0)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal36(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 0)) {
            return txt::plural::zero;
        }
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i % 100 == 2 || o.i % 100 == 22 || o.i % 100 == 42 || o.i % 100 == 62 || o.i % 100 == 82)) || ((!o.fraction && o.i % 1000 == 0) && (!o.fraction && ((o.i % 100000 >= 1000 && o.i % 100000 <= 20000) || o.i % 100000 == 40000 || o.i % 100000 == 60000 || o.i % 100000 == 80000))) || ((o.fraction || !(o.i == 0)) && (!o.fraction && o.i % 1000000 == 100000))) {
            return txt::plural::two;
        }
        if ((!o.fraction && (o.i % 100 == 3 || o.i % 100 == 23 || o.i % 100 == 43 || o.i % 100 == 63 || o.i % 100 == 83))) {
            return txt::plural::few;
        }
        if (((o.fraction || !(o.i == 1)) && (!o.fraction && (o.i % 100 == 1 || o.i % 100 == 21 || o.i % 100 == 41 || o.i % 100 == 61 || o.i % 100 == 81)))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal37(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 0)) {
            return txt::plural::zero;
        }
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && (o.i % 100 >= 3 && o.i % 100 <= 10))) {
            return txt::plural::few;
        }
        if ((!o.fraction && (o.i % 100 >= 11 && o.i % 100 <= 99))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Cardinal38(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 0)) {
            return txt::plural::zero;
        }
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 3)) {
            return txt::plural::few;
        }
        if ((!o.fraction && o.i == 6)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline constexpr PluralRule Cardinals[] = {
        &Cardinal0, &Cardinal1, &Cardinal2, &Cardinal3, &Cardinal4, &Cardinal5, &Cardinal6, &Cardinal7, &Cardinal8,
        &Cardinal9, &Cardinal10, &Cardinal11, &Cardinal12, &Cardinal13, &Cardinal14, &Cardinal15, &Cardinal16,
        &Cardinal17, &Cardinal18, &Cardinal19, &Cardinal20, &Cardinal21, &Cardinal22, &Cardinal23, &Cardinal24,
        &Cardinal25, &Cardinal26, &Cardinal27, &Cardinal28, &Cardinal29, &Cardinal30, &Cardinal31, &Cardinal32,
        &Cardinal33, &Cardinal34, &Cardinal35, &Cardinal36, &Cardinal37, &Cardinal38,
    };

    inline txt::plural Ordinal0(const PluralOperands& o) noexcept {
        (void)o;
        return txt::plural::other;
    }

    inline txt::plural Ordinal1(const PluralOperands& o) noexcept {
        if (((!o.fraction && (o.i % 10 == 1 || o.i % 10 == 2)) && (o.fraction || !((o.i % 100 == 11 || o.i % 100 == 12))))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal2(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal3(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 5))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal4(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i >= 1 && o.i <= 4))) {
            return txt::plural::one;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal5(const PluralOperands& o) noexcept {
        if (((!o.fraction && (o.i % 10 == 2 || o.i % 10 == 3)) && (o.fraction || !((o.i % 100 == 12 || o.i % 100 == 13))))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal6(const PluralOperands& o) noexcept {
        if (((!o.fraction && o.i % 10 == 3) && (o.fraction || !(o.i % 100 == 13)))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal7(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i % 10 == 6 || o.i % 10 == 9)) || (!o.fraction && o.i == 10)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal8(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i % 10 == 6) || (!o.fraction && o.i % 10 == 9) || ((!o.fraction && o.i % 10 == 0) && (o.fraction || !(o.i == 0)))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal9(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 11 || o.i == 8 || o.i == 80 || o.i == 800))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal10(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 11 || o.i == 8 || (o.i >= 80 && o.i <= 89) || (o.i >= 800 && o.i <= 899)))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal11(const PluralOperands& o) noexcept {
        if (o.i == 1) {
            return txt::plural::one;
        }
        if (o.i == 0 || ((o.i % 100 >= 2 && o.i % 100 <= 20) || o.i % 100 == 40 || o.i % 100 == 60 || o.i % 100 == 80)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal12(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if (((!o.fraction && o.i % 10 == 4) && (o.fraction || !(o.i % 100 == 14)))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal13(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i >= 1 && o.i <= 4)) || (!o.fraction && ((o.i % 100 >= 1 && o.i % 100 <= 4) || (o.i % 100 >= 21 && o.i % 100 <= 24) || (o.i % 100 >= 41 && o.i % 100 <= 44) || (o.i % 100 >= 61 && o.i % 100 <= 64) || (o.i % 100 >= 81 && o.i % 100 <= 84)))) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 5) || (!o.fraction && o.i % 100 == 5)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal14(const PluralOperands& o) noexcept {
        if (o.i == 0) {
            return txt::plural::zero;
        }
        if (o.i == 1) {
            return txt::plural::one;
        }
        if ((o.i == 2 || o.i == 3 || o.i == 4 || o.i == 5 || o.i == 6)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal15(const PluralOperands& o) noexcept {
        if (((!o.fraction && o.i % 10 == 1) && (o.fraction || !(o.i % 100 == 11)))) {
            return txt::plural::one;
        }
        if (((!o.fraction && o.i % 10 == 2) && (o.fraction || !(o.i % 100 == 12)))) {
            return txt::plural::two;
        }
        if (((!o.fraction && o.i % 10 == 3) && (o.fraction || !(o.i % 100 == 13)))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal16(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 3))) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 4)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal17(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 11))) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 12))) {
            return txt::plural::two;
        }
        if ((!o.fraction && (o.i == 3 || o.i == 13))) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal18(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 3))) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 4)) {
            return txt::plural::few;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal19(const PluralOperands& o) noexcept {
        if ((o.i % 10 == 1 && !(o.i % 100 == 11))) {
            return txt::plural::one;
        }
        if ((o.i % 10 == 2 && !(o.i % 100 == 12))) {
            return txt::plural::two;
        }
        if (((o.i % 10 == 7 || o.i % 10 == 8) && !(o.i % 100 == 17 || o.i % 100 == 18))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal20(const PluralOperands& o) noexcept {
        if ((o.i % 10 == 1 || o.i % 10 == 2 || o.i % 10 == 5 || o.i % 10 == 7 || o.i % 10 == 8) || (o.i % 100 == 20 || o.i % 100 == 50 || o.i % 100 == 70 || o.i % 100 == 80)) {
            return txt::plural::one;
        }
        if ((o.i % 10 == 3 || o.i % 10 == 4) || (o.i % 1000 == 100 || o.i % 1000 == 200 || o.i % 1000 == 300 || o.i % 1000 == 400 || o.i % 1000 == 500 || o.i % 1000 == 600 || o.i % 1000 == 700 || o.i % 1000 == 800 || o.i % 1000 == 900)) {
            return txt::plural::few;
        }
        if (o.i == 0 || o.i % 10 == 6 || (o.i % 100 == 40 || o.i % 100 == 60 || o.i % 100 == 90)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal21(const PluralOperands& o) noexcept {
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 3))) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 4)) {
            return txt::plural::few;
        }
        if ((!o.fraction && o.i == 6)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal22(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 5 || o.i == 7 || o.i == 8 || o.i == 9 || o.i == 10))) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 3))) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 4)) {
            return txt::plural::few;
        }
        if ((!o.fraction && o.i == 6)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal23(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 1 || o.i == 5 || (o.i >= 7 && o.i <= 9)))) {
            return txt::plural::one;
        }
        if ((!o.fraction && (o.i == 2 || o.i == 3))) {
            return txt::plural::two;
        }
        if ((!o.fraction && o.i == 4)) {
            return txt::plural::few;
        }
        if ((!o.fraction && o.i == 6)) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline txt::plural Ordinal24(const PluralOperands& o) noexcept {
        if ((!o.fraction && (o.i == 0 || o.i == 7 || o.i == 8 || o.i == 9))) {
            return txt::plural::zero;
        }
        if ((!o.fraction && o.i == 1)) {
            return txt::plural::one;
        }
        if ((!o.fraction && o.i == 2)) {
            return txt::plural::two;
        }
        if ((!o.fraction && (o.i == 3 || o.i == 4))) {
            return txt::plural::few;
        }
        if ((!o.fraction && (o.i == 5 || o.i == 6))) {
            return txt::plural::many;
        }
        return txt::plural::other;
    }

    inline constexpr PluralRule Ordinals[] = {
        &Ordinal0, &Ordinal1, &Ordinal2, &Ordinal3, &Ordinal4, &Ordinal5, &Ordinal6, &Ordinal7, &Ordinal8,
        &Ordinal9, &Ordinal10, &Ordinal11, &Ordinal12, &Ordinal13, &Ordinal14, &Ordinal15, &Ordinal16, &Ordinal17,
        &Ordinal18, &Ordinal19, &Ordinal20, &Ordinal21, &Ordinal22, &Ordinal23, &Ordinal24,
    };

    // a language (a language and a region) and its rule sets, cardinal and ordinal

    inline constexpr uint64_t PluralKeys[] = {
        0x0, 0x616600000000, 0x616b00000000, 0x616d00000000, 0x616e00000000, 0x617200000000, 0x617300000000,
        0x617a00000000, 0x626500000000, 0x626700000000, 0x626d00000000, 0x626e00000000, 0x626f00000000,
        0x627200000000, 0x627300000000, 0x636100000000, 0x636500000000, 0x637300000000, 0x637900000000,
        0x646100000000, 0x646500000000, 0x647600000000, 0x647a00000000, 0x656500000000, 0x656c00000000,
        0x656e00000000, 0x656f00000000, 0x657300000000, 0x657400000000, 0x657500000000, 0x666100000000,
        0x666600000000, 0x666900000000, 0x666f00000000, 0x667200000000, 0x667900000000, 0x676100000000,
        0x676400000000, 0x676c00000000, 0x677500000000, 0x677600000000, 0x686100000000, 0x686500000000,
        0x686900000000, 0x687200000000, 0x687500000000, 0x687900000000, 0x696100000000, 0x696400000000,
        0x696700000000, 0x696900000000, 0x696e00000000, 0x696f00000000, 0x697300000000, 0x697400000000,
        0x697500000000, 0x697700000000, 0x6a6100000000, 0x6a6900000000, 0x6a7600000000, 0x6a7700000000,
        0x6b6100000000, 0x6b6b00000000, 0x6b6c00000000, 0x6b6d00000000, 0x6b6e00000000, 0x6b6f00000000,
        0x6b7300000000, 0x6b7500000000, 0x6b7700000000, 0x6b7900000000, 0x6c6200000000, 0x6c6700000000,
        0x6c6e00000000, 0x6c6f00000000, 0x6c7400000000, 0x6c7600000000, 0x6d6700000000, 0x6d6b00000000,
        0x6d6c00000000, 0x6d6e00000000, 0x6d6f00000000, 0x6d7200000000, 0x6d7300000000, 0x6d7400000000,
        0x6d7900000000, 0x6e6200000000, 0x6e6400000000, 0x6e6500000000, 0x6e6c00000000, 0x6e6e00000000,
        0x6e6f00000000, 0x6e7200000000, 0x6e7900000000, 0x6f6d00000000, 0x6f7200000000, 0x6f7300000000,
        0x706100000000, 0x706c00000000, 0x707300000000, 0x707400000000, 0x707400000388, 0x726d00000000,
        0x726f00000000, 0x727500000000, 0x736300000000, 0x736400000000, 0x736500000000, 0x736700000000,
        0x736800000000, 0x736900000000, 0x736b00000000, 0x736c00000000, 0x736e00000000, 0x736f00000000,
        0x737100000000, 0x737200000000, 0x737300000000, 0x737400000000, 0x737500000000, 0x737600000000,
        0x737700000000, 0x746100000000, 0x746500000000, 0x746800000000, 0x746900000000, 0x746b00000000,
        0x746c00000000, 0x746e00000000, 0x746f00000000, 0x747200000000, 0x747300000000, 0x756700000000,
        0x756b00000000, 0x757200000000, 0x757a00000000, 0x766500000000, 0x766900000000, 0x766f00000000,
        0x776100000000, 0x776f00000000, 0x786800000000, 0x796900000000, 0x796f00000000, 0x7a6800000000,
        0x7a7500000000, 0x61727300000000, 0x61736100000000, 0x61737400000000, 0x62616c00000000, 0x62656d00000000,
        0x62657a00000000, 0x62686f00000000, 0x626c6f00000000, 0x62727800000000, 0x63656200000000, 0x63676700000000,
        0x63687200000000, 0x636b6200000000, 0x63737700000000, 0x646f6900000000, 0x64736200000000, 0x66696c00000000,
        0x66757200000000, 0x67737700000000, 0x67757700000000, 0x68617700000000, 0x686e6a00000000, 0x68736200000000,
        0x6a626f00000000, 0x6a676f00000000, 0x6a6d6300000000, 0x6b616200000000, 0x6b616a00000000, 0x6b636700000000,
        0x6b646500000000, 0x6b656100000000, 0x6b6b6a00000000, 0x6b736200000000, 0x6b736800000000, 0x6c616700000000,
        0x6c696a00000000, 0x6c6b7400000000, 0x6c6c6400000000, 0x6d617300000000, 0x6d676f00000000, 0x6e616800000000,
        0x6e617100000000, 0x6e6e6800000000, 0x6e716f00000000, 0x6e736f00000000, 0x6e796e00000000, 0x6f736100000000,
        0x70617000000000, 0x70636d00000000, 0x70726700000000, 0x726f6600000000, 0x72776b00000000, 0x73616800000000,
        0x73617100000000, 0x73617400000000, 0x73636e00000000, 0x73646800000000, 0x73656800000000, 0x73657300000000,
        0x73686900000000, 0x736d6100000000, 0x736d6900000000, 0x736d6a00000000, 0x736d6e00000000, 0x736d7300000000,
        0x73737900000000, 0x73797200000000, 0x74656f00000000, 0x74696700000000, 0x74706900000000, 0x747a6d00000000,
        0x76656300000000, 0x76756e00000000, 0x77616500000000, 0x786f6700000000, 0x79756500000000,
    };

    inline constexpr uint16_t PluralSets[] = {
        0, 1792, 1280, 256, 1792, 9472, 278, 1812, 7429, 1792, 0, 278, 0, 8192, 4864, 5650, 1792, 6912, 9752, 2048,
        768, 1792, 0, 1792, 1792, 783, 1792, 5888, 768, 1792, 256, 512, 768, 1792, 5122, 768, 8706, 6161, 768, 277,
        8960, 1792, 3840, 277, 4864, 1795, 514, 768, 0, 0, 0, 0, 768, 2304, 5641, 4096, 3840, 0, 768, 0, 0, 1803,
        1800, 1792, 0, 256, 0, 1792, 1792, 9229, 1792, 1792, 1792, 1280, 2, 7680, 3072, 1280, 2579, 1792, 1792,
        4610, 1808, 2, 8448, 0, 1792, 1792, 1796, 768, 1792, 1792, 1792, 1792, 1792, 1815, 1792, 1280, 7168, 1792,
        5376, 5632, 1792, 4610, 7936, 777, 1792, 4096, 0, 4864, 1024, 6912, 6400, 1792, 1792, 1804, 4864, 1792,
        1792, 0, 769, 768, 1792, 1792, 0, 1280, 1799, 2818, 1792, 0, 1792, 1792, 1792, 7942, 768, 1792, 1792, 2,
        1792, 1280, 0, 1792, 768, 0, 0, 256, 9472, 1792, 768, 1794, 1792, 1792, 1280, 3598, 1792, 2816, 1792, 1792,
        1792, 1280, 256, 6656, 2818, 1792, 1792, 1280, 1792, 0, 6656, 0, 1792, 1792, 512, 1792, 1792, 0, 0, 1792,
        1792, 3584, 3328, 778, 0, 5641, 1792, 1792, 1792, 4096, 1792, 0, 1280, 1792, 0, 1792, 256, 3072, 1792,
        1792, 0, 1792, 4096, 5641, 1792, 1792, 0, 4352, 4096, 4096, 4096, 4096, 4096, 1792, 1792, 1792, 1792, 0,
        1536, 5641, 1792, 1792, 1792, 0,
    };
}
