//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl::detail {
    // One word of a coroutine frame (coroutine.h): the frame is a managed
    // buffer of these, zeroed when made (the constructor is what makes the
    // type one that may hold tracked pointers, type_info.h) and traced
    // conservatively (type_info.h: Conservative<FrameWord>): a word that
    // holds a managed address keeps its object, a word that holds data is
    // never taken as proof that the offset is data, since every word of a
    // frame may be either, and differently in every frame.
    struct FrameWord {
        FrameWord() noexcept
        : word(nullptr) {
        }

        void* word;
    };
}
