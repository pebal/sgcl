//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "types.h"
#include "tracked_ptr.h"
#include "unique_deleter.h"

#include <memory>

namespace sgcl {
    namespace Priv {
        template<class T>
        class Unique_ptr : public std::unique_ptr<T, Unique_deleter> {
            using Base = std::unique_ptr<T, Unique_deleter>;

        public:
            using element_type = typename Base::element_type;

            Unique_ptr(element_type* p, Priv::Tracked)
            : Base(p) {
                assert(!p || Priv::Page::is_unique(p) == true);
            }

            template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
            Unique_ptr(Unique_ptr<U>&& p)
            : Base(p.release()) {
            }

            template <class U = T, class E = element_type, std::enable_if_t<std::is_array_v<U>, int> = 0>
            E& operator[](size_t i) const noexcept {
                assert(get() != nullptr);
                assert(i < size());
                return this->get()[i];
            }

            template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
            size_t size() const noexcept {
                auto p = this->get();
                if (p) {
                    auto array = (Array_base*)Tracked_ptr::base_address_of(p);
                    return array->count;
                }
                else {
                    return 0;
                }
            }

            template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
            element_type* begin() const noexcept {
                return this->get();
            }

            template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
            element_type* end() const noexcept {
                return begin() + size();
            }

            Unique_ptr clone() const {
                auto p = this->get();
                return (element_type*)Tracked_ptr::clone(p);
            }

            template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
            bool is() const noexcept {
                return type() == typeid(U);
            }

            template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
            root_ptr<U> as() const noexcept {
                if (is<U>()) {
                    auto p = this->get();
                    auto address = Tracked_ptr::base_address_of(p);
                    return root_ptr<U>((U*)address);
                } else {
                    return {nullptr};
                }
            }

            const std::type_info& type() const noexcept {
                auto p = this->get();
                return Tracked_ptr::type_info<T>(p);
            }

            metadata*& metadata() const noexcept {
                auto p = this->get();
                return Tracked_ptr::metadata<T>(p);
            }

            constexpr bool is_array() const noexcept {
                auto p = this->get();
                return Tracked_ptr::is_array<T>(p);
            }

        private:
            Unique_ptr(element_type* p)
            : Base(p) {
            }

            Unique_ptr() {}

            template<class> friend struct Maker;
            friend struct Maker_base;
            template<class> friend class sgcl::tracked_ptr;
            template<class> friend class sgcl::root_ptr;
            template<class> friend struct sgcl::unsafe_ptr;
            friend struct sgcl::collector;
        };
    }
}
