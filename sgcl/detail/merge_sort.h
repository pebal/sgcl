//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl::detail {
    // Merge sort of an intrusive list linked through the member `Next`, by
    // address: the allocators keep their lists of empty pages sorted so
    // that a thread refills from the lowest addresses first
    // (object_pool_allocator_base.h). Two sorted lists into one:
    template<auto Next, class T>
    T* merge(T* left, T* right) {
        if (!left) {
            return right;
        }
        if (!right) {
            return left;
        }
        T* result = nullptr;
        if (left < right) {
            result = left;
            left = left->*Next;
        } else {
            result = right;
            right = right->*Next;
        }
        T* current = result;
        while (left && right) {
            if (left < right) {
                current->*Next = left;
                current = left;
                left = left->*Next;
            } else {
                current->*Next = right;
                current = right;
                right = right->*Next;
            }
        }
        current->*Next = left ? left : right;
        return result;
    }

    // The list split in halves by a slow and a fast walk, each half sorted,
    // the two merged.
    template<auto Next, class T>
    T* merge_sort(T* head) {
        if (!head || !(head->*Next)) {
            return head;
        }
        T* slow = head;
        T* fast = head->*Next;
        while (fast && fast->*Next) {
            slow = slow->*Next;
            fast = (fast->*Next)->*Next;
        }
        T* mid = slow->*Next;
        slow->*Next = nullptr;
        T* left = merge_sort<Next>(head);
        T* right = merge_sort<Next>(mid);
        return merge<Next>(left, right);
    }
}
