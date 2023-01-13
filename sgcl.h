//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) Sebastian Nibisz
// SPDX-License-Identifier: LGPL-3.0-only
//------------------------------------------------------------------------------
#ifndef SGCL_H
#define SGCL_H

#include <any>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>

// the maximum sleep time of the GC thread in seconds
#define SGCL_MAX_SLEEP_TIME_SEC 30
// the percentage amount of allocations that will wake up the GC thread
#define SGCL_TRIGER_PERCENTAGE 25

//#define SGCL_DEBUG

#ifdef SGCL_DEBUG
	#define SGCL_LOG_PRINT_LEVEL 3
#endif

#if SGCL_LOG_PRINT_LEVEL
	#include <iostream>
#endif

//------------------------------------------------------------------------------
// Reduces memory usage on x86-64 platforms by using two highest bits of pointer
//
// Warning!
// User heap must be allocated in the low half of virtual address space
//------------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
	#define SGCL_ARCH_X86_64
#endif

namespace sgcl {
	template<class>
	class tracked_ptr;

	template<class T, size_t N>
	class tracked_ptr<T[N]>;

	template<class T, class ...A>
	auto make_tracked(A&&...);

	template<class T>
	tracked_ptr<void> Priv_clone(const void* p);

	struct metadata_base {
		const std::type_info& type;
		std::any& user_data;

	protected:
		metadata_base(const std::type_info& type, std::any& user_data)
		: type(type)
		, user_data(user_data) {
		}
	};

	template<class T>
	struct metadata : metadata_base {
		metadata()
		: metadata_base(typeid(T), _user_data) {
		}
		template<class U>

		static void set(U t) noexcept {
			_user_data = std::move(t);
		}

		static const std::any& get() noexcept {
			return _user_data;
		}

	private:
		inline static std::any _user_data = nullptr;
	};

	namespace Priv {
		static constexpr size_t SqrMaxTypeNumber = 64;
		[[maybe_unused]] static constexpr size_t MaxTypeNumber = SqrMaxTypeNumber * SqrMaxTypeNumber;
		static constexpr ptrdiff_t MaxStackOffset = 1024;
		static constexpr size_t PageSize = 4096;
		static constexpr size_t PageDataSize = PageSize - sizeof(uintptr_t);
		using Pointer = std::atomic<void*>;
	}

	[[maybe_unused]] static constexpr size_t MaxAliasingDataSize = Priv::PageDataSize;

	namespace Priv {
		struct States {
			using Value = uint8_t;
			static constexpr Value Unused = std::numeric_limits<uint8_t>::max();
			static constexpr Value BadAlloc = Unused - 1;
			static constexpr Value AtomicReachable = BadAlloc - 1;
			static constexpr Value Reachable = 1;
			static constexpr Value Used = 0;
		};

		struct Array_metadata;
		struct Array_base {
			constexpr Array_base(size_t c) noexcept
			: count(c) {
			}

			~Array_base() noexcept;

			template<class T>
			static void destroy(void* data, size_t count) noexcept {
				for (size_t i = count; i > 0; --i) {
					std::destroy_at((T*)data + i - 1);
				}
			}

			std::atomic<Array_metadata*> metadata = {nullptr};
			const size_t count;
		};

		struct Page;

		template<class T>
		struct Page_info;

		template<class T>
		tracked_ptr<void> Clone(const void*);

		struct Metadata {
			template<class T>
			Metadata(T*)
			: pointer_offsets(Page_info<T>::pointer_offsets)
			, destroy(!std::is_trivially_destructible_v<T> || std::is_base_of_v<Array_base, T> ? Page_info<T>::destroy : nullptr)
			, free(Page_info<T>::Heap_allocator::free)
			, clone(!std::is_base_of_v<Array_base, T> ? Clone<T> : nullptr)
			, object_size(Page_info<T>::ObjectSize)
			, object_count(Page_info<T>::ObjectCount)
			, is_array(std::is_base_of_v<Array_base, T>)
			, public_metadata(Page_info<T>::public_metadata()) {
			}

			std::atomic<ptrdiff_t*>& pointer_offsets;
			void (*const destroy)(void*) noexcept;
			void (*const free)(Page*);
			tracked_ptr<void> (*const clone)(const void*);
			const size_t object_size;
			const unsigned object_count;
			bool is_array;
			metadata_base& public_metadata;
			Page* empty_page = {nullptr};
			Metadata* next = {nullptr};
		};

		struct Array_metadata {
			template<class T>
			Array_metadata(T*)
			: pointer_offsets(Page_info<T>::pointer_offsets)
			, destroy(!std::is_trivially_destructible_v<T> ? Array_base::destroy<T> : nullptr)
			, object_size(Page_info<T>::ObjectSize)
			, public_metadata(Page_info<T[]>::public_metadata()) {
			}

			std::atomic<ptrdiff_t*>& pointer_offsets;
			void (*const destroy)(void*, size_t) noexcept;
			const size_t object_size;
			metadata_base& public_metadata;
		};

		Array_base::~Array_base() noexcept {
			auto metadata = this->metadata.load(std::memory_order_acquire);
			if (metadata && metadata->destroy) {
				metadata->destroy(this+ 1, count);
			}
		}

		struct Block;
		struct Page {
			using State = States::Value;
			using Flag = uint64_t;
			static constexpr unsigned FlagBitCount = sizeof(Flag) * 8;

			struct Flags {
				Flag registered = {0};
				Flag reachable = {0};
				Flag marked = {0};
			};

			template<class T>
			Page(Block* block, T* data) noexcept
			: metadata(&Page_info<T>::private_metadata())
			, block(block)
			, data((uintptr_t)data)
			, multiplier((1ull << 32 | 0x10000) / metadata->object_size) {
				assert(metadata != nullptr);
				assert(data != nullptr);
				auto states = this->states();
				for (unsigned i = 0; i < metadata->object_count; ++i) {
					new(states + i) std::atomic<State>(States::Used);
				}
				auto flags = this->flags();
				auto count = this->flags_count();
				for (unsigned i = 0; i < count; ++i) {
					new (flags + i) Flags;
				}
			}

			~Page() {
				if constexpr(!std::is_trivially_destructible_v<std::atomic<State>>) {
					auto states = this->states();
					std::destroy(states, states + metadata->object_count);
				}
			}

			std::atomic<State>* states() const noexcept {
				return (std::atomic<State>*)(this + 1);
			}

			Flags* flags() const noexcept {
				auto states_size = (sizeof(std::atomic<State>) * metadata->object_count + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
				return (Flags*)((uintptr_t)states() + states_size);
			}

			unsigned flags_count() const noexcept {
				return (metadata->object_count + FlagBitCount - 1) / FlagBitCount;
			}

			void clear_flags() noexcept {
				auto flags = this->flags();
				auto count = flags_count();
				for (unsigned i = 0; i < count; ++i) {
					flags[i].reachable = 0;
					flags[i].marked = 0;
				}
			}

			static constexpr unsigned flag_index_of(unsigned i) noexcept {
				return i / FlagBitCount;
			}

			static constexpr Flag flag_mask_of(unsigned i) noexcept {
				return Flag(1) << (i % FlagBitCount);
			}

			unsigned index_of(const void* p) noexcept {
				return ((uintptr_t)p - data) * multiplier >> 32;
			}

			uintptr_t data_of(unsigned index) noexcept {
				return data + index * metadata->object_size;
			}

			static Page* page_of(const void* p) noexcept {
				auto page = ((uintptr_t)p & ~(uintptr_t)(PageSize - 1));
				return *((Page**)page);
			}

			static Metadata& metadata_of(const void* p) noexcept {
				auto page = Page::page_of(p);
				return *page->metadata;
			}

			static void* base_address_of(const void* p) noexcept {
				auto page = page_of(p);
				auto index = page->index_of(p);
				return (void*)page->data_of(index);
			}

			static void set_state(const void* p, State s) noexcept {
				auto page = Page::page_of(p);
				auto index = page->index_of(p);
				auto &state = page->states()[index];
				state.store(s, std::memory_order_release);
			}

			Metadata* const metadata;
			Block* const block;
			const uintptr_t data;
			const uint64_t multiplier;
			bool reachable = {false};
			bool registered = {false};
			bool is_used = {true};
			std::atomic_bool on_empty_list = {false};
			Page* next_reachable = {nullptr};
			Page* next_registered = {nullptr};
			Page* next_empty = {nullptr};
			Page* next = {nullptr};
		};

		struct Pointer_pool_base {
			Pointer_pool_base(unsigned s, unsigned o)
			: _size(s)
			, _offset(o)
			, _position(s) {
			}
			void fill(void* data) {
				for(auto i = 0u; i < _size; ++i, data = (void*)((uintptr_t)data + _offset)) {
					_indexes[i] = data;
				}
				_position = 0;
			}
			void fill(Page* page) {
				auto data = page->data;
				auto object_size = page->metadata->object_size;
				auto states = page->states();
				auto count = page->metadata->object_count;
				for(int i = count - 1; i >= 0; --i) {
					if (states[i].load(std::memory_order_relaxed) == States::Unused) {
						_indexes[--_position] = (void*)(data + i * object_size);
						states[i].store(States::Used, std::memory_order_relaxed);
					}
				}
			}
			unsigned pointer_count() const noexcept {
				return _size - _position;
			}
			bool is_empty() const noexcept {
				return _position == _size;
			}
			bool is_full() const noexcept {
				return _position == 0;
			}
			void* alloc() noexcept {
				return _indexes[_position++];
			}
			void free(void* p) noexcept {
				_indexes[--_position] = p;
			}

		protected:
			void** _indexes;

		private:
			const unsigned _size;
			const unsigned _offset;
			unsigned _position;
		};

		template<unsigned Size, unsigned Offset>
		struct Pointer_pool : Pointer_pool_base {
			constexpr Pointer_pool()
			: Pointer_pool_base(Size, Offset) {
				Pointer_pool_base::_indexes = _indexes.data();
			}
			Pointer_pool(void* data)
			: Pointer_pool() {
				Pointer_pool_base::_indexes = _indexes.data();
				fill(data);
			}

			Pointer_pool* next = nullptr;

		private:
			std::array<void*, Size> _indexes;
		};

		template<class>
		struct Large_object_allocator;

		template<class>
		struct Small_object_allocator;

		template<class T>
		struct Page_info {
			static constexpr size_t ObjectSize = sizeof(std::remove_extent_t<std::conditional_t<std::is_same_v<T, void>, char, T>>);
			static constexpr size_t ObjectCount = std::max(size_t(1), PageDataSize / ObjectSize);
			static constexpr size_t StatesSize = (sizeof(std::atomic<Page::State>) * ObjectCount + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
			static constexpr size_t FlagsCount = (ObjectCount + Page::FlagBitCount - 1) / Page::FlagBitCount;
			static constexpr size_t FlagsSize = sizeof(Page::Flags) * FlagsCount;
			static constexpr size_t HeaderSize = sizeof(Page) + StatesSize + FlagsSize;
			using Heap_allocator = std::conditional_t<ObjectSize <= PageDataSize, Small_object_allocator<T>, Large_object_allocator<T>>;

			static void destroy(void* p) noexcept {
				std::destroy_at((T*)p);
			}

			inline static auto& public_metadata() {
				static auto metadata = new sgcl::metadata<T>;
				return *metadata;
			}

			inline static auto& private_metadata() {
				static auto metadata = new Metadata((std::remove_extent_t<T>*)0);
				return *metadata;
			}

			inline static auto& array_metadata() {
				static auto metadata = new Array_metadata((std::remove_extent_t<std::conditional_t<std::is_same_v<T, void>, char, T>>*)0);
				return *metadata;
			}

			inline static std::atomic<ptrdiff_t*> pointer_offsets = nullptr;
		};

		template<size_t Size = 1>
		struct Array : Array_base {
			constexpr Array(size_t c) noexcept
			: Array_base(c) {
			}
			template<class T>
			void init() {
				if constexpr(!std::is_trivial_v<T>) {
					_init<T>();
				}
				metadata.store(&Page_info<T>::array_metadata(), std::memory_order_release);
			}
			template<class T>
			void init(const T& v) {
				_init<T>(v);
				metadata.store(&Page_info<T>::array_metadata(), std::memory_order_release);
			}
			char data[Size];

		private:
			template<class T, class... A>
			void _init(A&&... a);
		};

		template<>
		struct Page_info<Array<>> : public Page_info<Array<PageDataSize>> {
			using Heap_allocator = Large_object_allocator<Array<>>;

			static void destroy(void* p) noexcept {
				std::destroy_at((Array<>*)p);
			}
		};

		struct Block {
			static constexpr size_t PageCount = 15;

			Block() noexcept {
				void* data = this + 1;
				for (size_t i = 0; i < PageCount; ++i) {
					*((Block**)data) = this;
					data = (void*)((uintptr_t)data + PageSize);
				}
			}

			static void* operator new(size_t) {
				void* mem = ::operator new(sizeof(uintptr_t) + sizeof(Block) + PageSize * (PageCount + 1));
				uintptr_t addres = (uintptr_t)mem + sizeof(uintptr_t) + sizeof(Block) + PageSize;
				addres = addres & ~(PageSize - 1);
				void* ptr = (void*)addres;
				Block* block = (Block*)ptr - 1;
				*((void**)block - 1) = mem;
				return block;
			}

			static void operator delete(void* p, size_t) noexcept {
				::operator delete(*((void**)p - 1));
			}

			Block* next = {nullptr};
			unsigned page_count = {0};
		};

		struct Block_allocator {
			using Pointer_pool = Priv::Pointer_pool<Block::PageCount, PageSize>;

			~Block_allocator() noexcept {
				void* page = nullptr;
				while (!_pointer_pool.is_empty()) {
					auto data = _pointer_pool.alloc();
					*((void**)data + 1) = page;
					page = (void*)data;
				}
				if (page) {
					free(page);
				}
			}

			void* alloc() {
				if (_pointer_pool.is_empty()) {
					auto page = _empty_pages.load(std::memory_order_acquire);
					while(page && !_empty_pages.compare_exchange_weak(page, *((void**)page + 1), std::memory_order_relaxed, std::memory_order_acquire));
					if (page) {
						return page;
					} else {
						auto block = new Block;
						_pointer_pool.fill(block + 1);
					}
				}
				return _pointer_pool.alloc();
			}

			static void free(void* page) {
				auto last = page;
				while(*((void**)last + 1)) {
					last = *((void**)last + 1);
				}
				*((void**)last + 1) = _empty_pages.exchange(nullptr, std::memory_order_acquire);
				Block* block = nullptr;
				auto p = page;
				while(p) {
					Block* b = *((Block**)p);
					if (!b->page_count) {
						b->next = block;
						block = b;
					}
					++b->page_count;
					p = *((void**)p + 1);
				}
				void* prev = nullptr;
				p = page;
				while(p) {
					auto next = *((void**)p + 1);
					Block* b = *((Block**)p);
					if (b->page_count == Block::PageCount) {
						if (!prev) {
							page = next;
						} else {
							*((void**)prev + 1) = next;
						}
					} else {
						prev = p;
					}
					p = next;
				}
				_empty_pages.store(page, std::memory_order_release);
				while(block) {
					auto next = block->next;
					if (block->page_count == Block::PageCount) {
						delete block;
					} else {
						block->page_count = 0;
					}
					block = next;
				}
			}

		private:
			inline static std::atomic<void*> _empty_pages = {nullptr};
			Pointer_pool _pointer_pool;
		};

		struct Heap_allocator_base {
			virtual ~Heap_allocator_base() noexcept = default;
			inline static std::atomic<Page*> pages = {nullptr};
		};

		template<class T>
		struct Large_object_allocator : Heap_allocator_base {
			T* alloc(size_t size = 0) const {
				auto mem = ::operator new(sizeof(T) + size + sizeof(uintptr_t), std::align_val_t(PageSize));
				auto data = (T*)((uintptr_t)mem + sizeof(uintptr_t));
				auto hmem = ::operator new(Page_info<T>::HeaderSize);
				auto page = new(hmem) Page(nullptr, data);
				*((Page**)mem) = page;
				page->next = pages.load(std::memory_order_relaxed);
				while(!pages.compare_exchange_weak(page->next, page, std::memory_order_release, std::memory_order_relaxed));
				return data;
			}

			static void free(Page* pages) noexcept {
				Page* page = pages;
				while(page) {
					auto data = (void*)(page->data - sizeof(uintptr_t));
					::operator delete(data, std::align_val_t(PageSize));
					page->is_used = false;
					page = page->next_empty;
				}
			}
		};

		struct Small_object_allocator_base : Heap_allocator_base {
			Small_object_allocator_base(Block_allocator& ba, Pointer_pool_base& pa, std::atomic<Page*>& pb) noexcept
			: _block_allocator(ba)
			, _pointer_pool(pa)
			, _pages_buffer(pb) {
			}

			~Small_object_allocator_base() noexcept override {
				while (!_pointer_pool.is_empty()) {
					auto ptr = _pointer_pool.alloc();
					auto index = _current_page->index_of(ptr);
					_current_page->states()[index].store(States::Unused, std::memory_order_relaxed);
				}
			}

			void* alloc(size_t) {
				if  (_pointer_pool.is_empty()) {
					auto page = _pages_buffer.load(std::memory_order_acquire);
					while(page && !_pages_buffer.compare_exchange_weak(page, page->next_empty, std::memory_order_relaxed, std::memory_order_acquire));
					if (page) {
						_pointer_pool.fill(page);
						page->on_empty_list.store(false, std::memory_order_relaxed);
					} else {
						page = _alloc_page();
						_pointer_pool.fill((void*)(page->data));
						page->next = pages.load(std::memory_order_relaxed);
						while(!pages.compare_exchange_weak(page->next, page, std::memory_order_release, std::memory_order_relaxed));
					}
					_current_page = page;
				}
				assert(!_pointer_pool.is_empty());
				return (void*)_pointer_pool.alloc();
			}

		private:
			Block_allocator& _block_allocator;
			Pointer_pool_base& _pointer_pool;
			std::atomic<Page*>& _pages_buffer;
			Page* _current_page = {nullptr};

			virtual Page* _create_page_parameters(Block* block, void* data) = 0;

			Page* _alloc_page() {
				auto mem = _block_allocator.alloc();
				auto block = *((Block**)mem);
				auto data = (void*)((uintptr_t)mem + sizeof(uintptr_t));
				auto page = _create_page_parameters(block, data);
				*((Page**)mem) = page;
				return page;
			}

		protected:
			static void _remove_empty(Page*& pages, Page*& empty_pages) noexcept {
				auto page = pages;
				Page* prev = nullptr;
				while(page) {
					auto next = page->next_empty;
					auto states = page->states();
					auto object_count = page->metadata->object_count;
					auto unused_count = 0u;
					for (unsigned i = 0; i < object_count; ++i) {
						auto state = states[i].load(std::memory_order_relaxed);
						if (state == States::Unused) {
							++unused_count;
						}
					}
					if (unused_count == object_count) {
						page->next_empty = empty_pages;
						empty_pages = page;
						if (!prev) {
							pages = next;
						} else {
							prev->next_empty = next;
						}
					} else {
						prev = page;
					}
					page = next;
				}
			}

			static void _free(Page* pages) noexcept {
				Page* page = pages;
				void* empty = nullptr;
				while(page) {
					auto data = page->data - sizeof(uintptr_t);
					*((Block**)data) = page->block;
					page->is_used = false;
					*((void**)data + 1) = empty;
					empty = (void*)data;
					page = page->next_empty;
				}
				Block_allocator::free(empty);
			}

			static void _free(Page* pages, std::atomic<Page*>& pages_buffer) noexcept {
				Page* empty_pages = nullptr;
				_remove_empty(pages, empty_pages);
				pages = pages_buffer.exchange(pages, std::memory_order_relaxed);
				_remove_empty(pages, empty_pages);
				pages = pages_buffer.exchange(pages, std::memory_order_relaxed);
				if (pages) {
					auto last = pages;
					while(last->next_empty) {
						last = last->next_empty;
					}
					last->next_empty = pages_buffer.load(std::memory_order_relaxed);
					while(!pages_buffer.compare_exchange_weak(last->next_empty, pages, std::memory_order_release, std::memory_order_relaxed));
				}
				if (empty_pages) {
					_free(empty_pages);
				}
			}
		};

		template<class T>
		struct Small_object_allocator : Small_object_allocator_base {
			using Pointer_pool = Priv::Pointer_pool<Page_info<T>::ObjectCount, sizeof(std::conditional_t<std::is_same_v<T, void>, char, T>)>;

			constexpr Small_object_allocator(Block_allocator& a) noexcept
			: Small_object_allocator_base(a, _pointer_pool, _pages_buffer) {
			}

			static void free(Page* pages) {
				_free(pages, _pages_buffer);
			}

		private:
			inline static std::atomic<Page*> _pages_buffer = {nullptr};
			Pointer_pool _pointer_pool;

			Page* _create_page_parameters(Block* block, void* data) override {
				auto mem = ::operator new(Page_info<T>::HeaderSize);
				auto page = new(mem) Page(block, (T*)data);
				return page;
			}
		};

		struct Roots_allocator {
			static constexpr size_t PointerCount = PageSize / sizeof(Pointer);
			using Page = std::array<Pointer, PointerCount>;
			using Pointer_pool = Priv::Pointer_pool<PointerCount, sizeof(Pointer)>;

			struct Page_node {
				Page_node* next = {nullptr};
				Page page = {nullptr};
			};

			constexpr Roots_allocator() noexcept
			:_pointer_pool({nullptr, nullptr}) {
			}

			~Roots_allocator() noexcept {
				for(auto pointer_pool : _pointer_pool) {
					if (pointer_pool) {
						auto& pool = pointer_pool->is_empty() ? _global_empty_pointer_pool : _global_pointer_pool;
						pointer_pool->next = pool.load(std::memory_order_acquire);
						while(!pool.compare_exchange_weak(pointer_pool->next, pointer_pool, std::memory_order_release, std::memory_order_relaxed));
					}
				}
			}

			Pointer* alloc() {
				auto [pool1, pool2] = _pointer_pool;
				if (pool1 && !pool1->is_empty()) {
					return (Pointer*)pool1->alloc();
				}
				if (pool2 && !pool2->is_empty()) {
					_pointer_pool = {pool2, pool1};
					return (Pointer*)pool2->alloc();
				}
				auto new_pool = _global_pointer_pool.load(std::memory_order_acquire);
				while(new_pool && !_global_pointer_pool.compare_exchange_weak(new_pool, new_pool->next, std::memory_order_release, std::memory_order_acquire));
				if (!new_pool) {
					auto node = new Page_node;
					new_pool = new Pointer_pool(node->page.data());
					node->next = pages.load(std::memory_order_acquire);
					while(!pages.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed));
				}
				if (pool1) {
					if (pool2) {
						pool2->next = _global_empty_pointer_pool.load(std::memory_order_acquire);
						while(!_global_empty_pointer_pool.compare_exchange_weak(pool2->next, pool2, std::memory_order_release, std::memory_order_relaxed));
					}
					pool2 = pool1;
				}
				pool1 = new_pool;
				_pointer_pool = {pool1, pool2};
				return (Pointer*)pool1->alloc();
			}

			void free(Pointer* p) noexcept {
				auto [pool1, pool2] = _pointer_pool;
				if (pool1 && !pool1->is_full()) {
					pool1->free(p);
					return;
				}
				if (pool2 && !pool2->is_full()) {
					pool2->free(p);
					_pointer_pool = {pool2, pool1};
					return;
				}
				auto new_pool = _global_empty_pointer_pool.load(std::memory_order_acquire);
				while(new_pool && !_global_empty_pointer_pool.compare_exchange_weak(new_pool, new_pool->next, std::memory_order_release, std::memory_order_acquire));
				if (!new_pool) {
					new_pool = new Pointer_pool();
				}
				if (pool1) {
					if (pool2) {
						pool2->next = _global_pointer_pool.load(std::memory_order_acquire);
						while(!_global_pointer_pool.compare_exchange_weak(pool2->next, pool2, std::memory_order_release, std::memory_order_relaxed));
					}
					pool2 = pool1;
				}
				pool1 = new_pool;
				_pointer_pool = {pool1, pool2};
				pool1->free(p);
			}

			inline static std::atomic<Page_node*> pages = {nullptr};

		private:
			std::array<Pointer_pool*, 2> _pointer_pool;
			inline static std::atomic<Pointer_pool*> _global_pointer_pool = {nullptr};
			inline static std::atomic<Pointer_pool*> _global_empty_pointer_pool = {nullptr};
		};

		struct Stack_allocator {
			static constexpr unsigned PageCount = 256;
			static constexpr size_t PointerCount = PageSize / sizeof(Pointer);
			using Page = std::array<Pointer, PointerCount>;

			~Stack_allocator() noexcept {
				for (auto& page : pages) {
					delete page.load(std::memory_order_relaxed);
				}
			}

			Pointer* alloc(void* p) {
				auto index = ((uintptr_t)p / PageSize) % PageCount;
				auto page = pages[index].load(std::memory_order_relaxed);
				if (!page) {
					page = new Page{nullptr};
					pages[index].store(page, std::memory_order_release);
				}
				auto offset = ((uintptr_t)p % PageSize) / sizeof(Pointer);
				return &(*page)[offset];
			}

			std::atomic<Page*> pages[PageCount] = {nullptr};
		};

		struct Thread {
			struct Data {
				Data(Block_allocator* b, Stack_allocator* s) noexcept
				: block_allocator(b)
				, stack_allocator(s) {
				}
				std::unique_ptr<Block_allocator> block_allocator;
				std::unique_ptr<Stack_allocator> stack_allocator;
				std::atomic<bool> is_used = {true};
				Pointer recursive_alloc_pointer = {nullptr};
				void* last_recursive_alloc_pointer = {nullptr};
				std::atomic<uint64_t> alloc_count = {0};
				std::atomic<uint64_t> alloc_size = {0};
				Data* next = {nullptr};
			};

			struct Alloc_state {
				std::pair<uintptr_t, uintptr_t> range;
				size_t size;
				Pointer** ptrs;
			};

			Thread()
			:	block_allocator(new Block_allocator)
			, stack_allocator(new Stack_allocator)
			, roots_allocator(new Roots_allocator)
			, _data(new Data{block_allocator, stack_allocator}){
#if SGCL_LOG_PRINT_LEVEL >= 3
				std::cout << "[sgcl] start thread id: " << std::this_thread::get_id() << std::endl;
#endif
				_data->next = threads_data.load(std::memory_order_acquire);
				while(!threads_data.compare_exchange_weak(_data->next, _data, std::memory_order_release, std::memory_order_relaxed));
			}

			~Thread();

			template<class T>
			auto& alocator() {
				if constexpr(std::is_same_v<typename Page_info<T>::Heap_allocator, Small_object_allocator<T>>) {
					static unsigned index = _type_index++;
					assert(index < MaxTypeNumber);
					auto ax = heaps.get();
					if (!ax) {
						ax = new std::array<std::unique_ptr<std::array<std::unique_ptr<Heap_allocator_base>, SqrMaxTypeNumber>>, SqrMaxTypeNumber>;
						heaps.reset(ax);
					}
					auto& ay = (*ax)[index / SqrMaxTypeNumber];
					if (!ay) {
						ay.reset(new std::array<std::unique_ptr<Heap_allocator_base>, SqrMaxTypeNumber>);
					}
					auto& alocator = (*ay)[index % SqrMaxTypeNumber];
					if (!alocator) {
						alocator.reset(new Small_object_allocator<T>(*block_allocator));
					}
					return static_cast<Small_object_allocator<T>&>(*alocator);
				}
				else {
					static Large_object_allocator<T> allocator;
					return allocator;
				}
			}

			bool set_recursive_alloc_pointer(void* p) noexcept {
				if (!_data->recursive_alloc_pointer.load(std::memory_order_relaxed)) {
					_data->recursive_alloc_pointer.store(p, std::memory_order_release);
					return true;
				}
				return false;
			}

			void clear_recursive_alloc_pointer() noexcept {
				_data->recursive_alloc_pointer.store(nullptr, std::memory_order_release);
			}

			void update_allocated(size_t s) {
				auto v = _data->alloc_count.load(std::memory_order_relaxed);
				_data->alloc_count.store(v + 1, std::memory_order_relaxed);
				v = _data->alloc_size.load(std::memory_order_relaxed);
				_data->alloc_size.store(v + s, std::memory_order_relaxed);
			}

			Block_allocator* const block_allocator;
			Stack_allocator* const stack_allocator;
			const std::unique_ptr<Roots_allocator> roots_allocator;
			std::unique_ptr<std::array<std::unique_ptr<std::array<std::unique_ptr<Heap_allocator_base>, SqrMaxTypeNumber>>, SqrMaxTypeNumber>> heaps;
			Alloc_state alloc_state = {{0, 0}, 0, nullptr};

			inline static std::atomic<Data*> threads_data = {nullptr};
			inline static std::thread::id main_thread_id = {};

		private:
			Data* const _data;
			inline static std::atomic<int> _type_index = {0};
		};

		struct Main_thread_detector {
			Main_thread_detector() noexcept {
				Thread::main_thread_id = std::this_thread::get_id();
			}
		};
		static Main_thread_detector main_thread_detector;

		static Thread& current_thread() {
			static thread_local Thread instance;
			return instance;
		}

		struct Collector {
			struct Counter {
				Counter operator+(const Counter& c) const noexcept {
					return {count + c.count, size + c.size};
				}
				Counter operator-(const Counter& c) const noexcept {
					return {count - c.count, size - c.size};
				}
				void operator+=(const Counter& c) noexcept {
					count += c.count;
					size += c.size;
				}
				void operator-=(const Counter& c) noexcept {
					count += c.count;
					size += c.size;
				}
				uint64_t count = {0};
				uint64_t size = {0};
			};

			Collector() {
				std::thread([this]{_main_loop();}).detach();
			}

			~Collector() {
				abort();
			}

			inline static void abort() noexcept {
				_abort.store(true, std::memory_order_relaxed);
			}

			inline static bool aborted() noexcept {
				return _abort.load(std::memory_order_relaxed);
			}

		private:
			Counter _alloc_counter() const {
				Counter allocated;
				auto data = Thread::threads_data.load(std::memory_order_acquire);
				while(data) {
					allocated.count += data->alloc_count.load(std::memory_order_relaxed);
					allocated.size += data->alloc_size.load(std::memory_order_relaxed);
					data = data->next;
				}
				return allocated + _allocated_rest;
			}

			bool _check_threads() noexcept {
				Thread::Data* prev = nullptr;
				auto data = Thread::threads_data.load(std::memory_order_acquire);
				while(data) {
					auto next = data->next;
					if (data->is_used.load(std::memory_order_acquire)) {
						auto ptr = data->recursive_alloc_pointer.load(std::memory_order_relaxed);
						if (ptr && ptr == data->last_recursive_alloc_pointer) {
							return false;
						}
						prev = data;
					} else {
						if (!prev) {
							auto rdata = data;
							if (!Thread::threads_data.compare_exchange_strong(rdata, next, std::memory_order_relaxed, std::memory_order_acquire)) {
								while(rdata->next != data) {
									rdata = rdata->next;
								}
								prev = rdata;
							}
						}
						if (prev) {
							prev->next = next;
						}
						_allocated_rest.count += data->alloc_count.load(std::memory_order_relaxed);
						_allocated_rest.size += data->alloc_size.load(std::memory_order_relaxed);
						delete data;
					}
					data = next;
				}
				data = Thread::threads_data.load(std::memory_order_acquire);
				while(data) {
					auto ptr = data->recursive_alloc_pointer.load(std::memory_order_relaxed);
					data->last_recursive_alloc_pointer = ptr;
					data = data->next;
				}
				return true;
			}

			struct Timer {
				Timer() noexcept {
					reset();
				}
				double duration() noexcept {
					auto clock = std::chrono::steady_clock::now();
					return std::chrono::duration<double, std::milli>(clock - _clock).count();
				}
				void reset() noexcept {
					_clock = std::chrono::steady_clock::now();
				}

			private:
				std::chrono::steady_clock::time_point _clock;
			};

			void _update_states() {
				static Timer timer;
				static double rest = 0;
				double duration = timer.duration();
				duration = States::AtomicReachable * duration / 100 + rest; // 100ms for AtomicReachable
				auto iduration = (unsigned)duration;
				if (iduration >= 1) {
					timer.reset();
					rest = duration - iduration;
				}
				auto time_value = (States::Value)std::min((unsigned)States::AtomicReachable, iduration);
				auto page = _registered_pages;
				while(page) {
					auto states = page->states();
					auto flags = page->flags();
					auto count = page->metadata->object_count;
					for (unsigned i = 0; i < count; ++i) {
						auto state = states[i].load(std::memory_order_relaxed);
						if (state >= States::Reachable && state <= States::AtomicReachable) {
							auto index = Page::flag_index_of(i);
							auto mask = Page::flag_mask_of(i);
							auto& flag = flags[index];
							if (flag.registered & mask) {
								state -= state == States::Reachable || state == States::AtomicReachable ? 1 : std::min(state, time_value);
								states[i].store(state, std::memory_order_relaxed);
							}
						}
					}
					page = page->next_registered;
				}
			}

			void _mark_live_objects() noexcept {
				Page* prev = nullptr;
				auto page = Heap_allocator_base::pages.load(std::memory_order_acquire);
				while(page) {
					auto next = page->next;
					if (!page->is_used) {
						if (!prev) {
							auto rpage = page;
							if (!Heap_allocator_base::pages.compare_exchange_strong(rpage, next, std::memory_order_relaxed, std::memory_order_acquire)) {
								while(rpage->next != page) {
									rpage = rpage->next;
								}
								prev = rpage;
							}
						}
						if (prev) {
							prev->next = next;
						}
						delete page;
					} else {
						page->clear_flags();
						auto states = page->states();
						auto flags = page->flags();
						auto count = page->metadata->object_count;
						for (unsigned i = 0; i < count; ++i) {
							auto state = states[i].load(std::memory_order_relaxed);
							if (state != States::Unused) {
								if (state == States::BadAlloc) {
									// to implement
									continue;
								}
								if (state >= States::Reachable) {
									auto index = Page::flag_index_of(i);
									auto mask = Page::flag_mask_of(i);
									auto& flag = flags[index];
									if (!(flag.registered & mask)) {
										if (!page->registered) {
											page->registered = true;
											page->next_registered = _registered_pages;
											_registered_pages = page;
										}
										if (!page->reachable) {
											page->reachable = true;
											page->next_reachable = _reachable_pages;
											_reachable_pages = page;
										}
										flag.registered |= mask;
										flag.reachable |= mask;
									}
								}
							}
						}
						prev = page;
					}
					page = next;
				}
			}

			void _mark(const void* p) noexcept {
				if (p) {
					auto page = Page::page_of(p);
					auto object_index = page->index_of(p);
					auto index = Page::flag_index_of(object_index);
					auto mask = Page::flag_mask_of(object_index);
					auto& flag = page->flags()[index];
					if (flag.registered & mask && !(flag.marked & mask)) {
						if (!page->reachable) {
							page->reachable = true;
							page->next_reachable = _reachable_pages;
							_reachable_pages = page;
						}
						flag.reachable |= mask;
					}
				}
			}

			void _mark_stack() noexcept {
				auto data = Thread::threads_data.load(std::memory_order_acquire);
				while(data) {
					for (auto& p: data->stack_allocator->pages) {
						auto page = p.load(std::memory_order_relaxed);
						if (page) {
							for (auto& p: *page) {
								_mark(p.load(std::memory_order_relaxed));
							}
						}
					}
					data = data->next;
				}
			}

			void _mark_roots() noexcept {
				auto node = Roots_allocator::pages.load(std::memory_order_acquire);
				while(node) {
					for (auto& p :node->page) {
						_mark(p.load(std::memory_order_relaxed));
					}
					node = node->next;
				}
			}

			void _mark_childs(Page* page, unsigned index) noexcept {
				auto data = page->data_of(index);
				auto offsets = page->metadata->pointer_offsets.load(std::memory_order_acquire);
				unsigned size = (unsigned)offsets[0];
				for (unsigned i = 1; i <= size; ++i) {
					auto p = (Pointer*)(data + offsets[i]);
					_mark(p->load(std::memory_order_relaxed));
				}
			}

			void _mark_array_childs(Page* page, unsigned index) noexcept {
				auto data = page->data_of(index);
				auto array = (Array_base*)data;
				auto metadata = array->metadata.load(std::memory_order_acquire);
				if (metadata) {
					auto offsets = metadata->pointer_offsets.load(std::memory_order_acquire);
					if (offsets) {
						unsigned size = (unsigned)offsets[0];
						if (size) {
							data += sizeof(Array_base);
							auto object_size = metadata->object_size;
							for (size_t c = 0; c < array->count; ++c, data += object_size) {
								for (unsigned i = 1; i <= size; ++i) {
									auto p = (Pointer*)(data + offsets[i]);
									_mark(p->load(std::memory_order_relaxed));
								}
							}
						}
					}
				}
			}

			void _mark_reachable() noexcept {
				auto page = _reachable_pages;
				_reachable_pages = nullptr;
				while(page) {
					auto flags = page->flags();
					auto count = page->flags_count();
					bool marked;
					do {
						marked = false;
						for (unsigned i = 0; i < count; ++i) {
							auto& flag = flags[i];
							while (flag.reachable) {
								for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
									auto mask = Page::Flag(1) << j;
									if (flag.reachable & mask) {
										flag.marked |= mask;
										marked = true;
										auto index = i * Page::FlagBitCount + j;
										if (page->metadata->is_array) {
											_mark_array_childs(page, index);
										} else {
											_mark_childs(page, index);
										}
									}
								}
								flag.reachable &= ~flag.marked;
							}
						}
					} while(marked);
					page->reachable = false;
					page = page->next_reachable;
					if (!page) {
						page = _reachable_pages;
						_reachable_pages = nullptr;
					}
				}
			}

			void _mark_updated() noexcept {
				auto page = _registered_pages;
				while(page) {
					bool reachable = false;
					auto states = page->states();
					auto flags = page->flags();
					auto count = page->metadata->object_count;
					for (unsigned i = 0; i < count; ++i) {
						auto state = states[i].load(std::memory_order_relaxed);
						if (state >= States::Reachable && state <= States::AtomicReachable) {
							auto index = Page::flag_index_of(i);
							auto mask = Page::flag_mask_of(i);
							auto& flag = flags[index];
							if ((flag.registered & mask) && !(flag.marked & mask)) {
								flag.reachable |= mask;
								reachable = true;
							}
						}
					}
					if (reachable && !page->reachable) {
							page->reachable = true;
							page->next_reachable = _reachable_pages;
							_reachable_pages = page;
					}
					page = page->next_registered;
				}
			}

			Counter _remove_garbage() noexcept {
				Counter released;
				auto page = _registered_pages;
				Metadata* metadata = nullptr;
				while(page) {
					bool deregistered = false;
					auto destroy = page->metadata->destroy;
					auto states = page->states();
					auto data = page->data;
					auto object_size = page->metadata->object_size;
					auto flags = page->flags();
					auto count = page->flags_count();
					bool on_free_list = false;
					for (unsigned i = 0; i < count; ++i) {
						auto& flag = flags[i];
						auto f = flag.registered & ~flag.marked;
						if (f) {
							for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
								auto mask = Page::Flag(1) << j;
								if (f & mask) {
									auto index = i * Page::FlagBitCount + j;
									if (destroy) {
										auto p = data + index * object_size;
										auto data = page->data_of(index);
										if (!page->metadata->is_array) {
											auto offsets = page->metadata->pointer_offsets.load(std::memory_order_acquire);
											unsigned size = (unsigned)offsets[0];
											for (unsigned i = 1; i <= size; ++i) {
												auto p = (Pointer*)(data + offsets[i]);
												p->store(nullptr, std::memory_order_relaxed);
											}
										} else {
											auto array = (Array_base*)data;
											auto metadata = array->metadata.load(std::memory_order_acquire);
											auto offsets = metadata->pointer_offsets.load(std::memory_order_acquire);
											if (offsets) {
												unsigned size = (unsigned)offsets[0];
												if (size) {
													auto data = (uintptr_t)array + sizeof(Array_base);
													auto object_size = metadata->object_size;
													for (size_t c = 0; c < array->count; ++c, data += object_size) {
														for (unsigned i = 1; i <= size; ++i) {
															auto p = (Pointer*)(data + offsets[i]);
															p->store(nullptr, std::memory_order_relaxed);
														}
													}
												}
											}
										}
										destroy((void*)p);
									}
									released.count++;
									if (!page->metadata->is_array || object_size != sizeof(Array<PageDataSize>)) {
										released.size += object_size;
									} else {
										auto array = (Array_base*)data;
										auto metadata = array->metadata.load(std::memory_order_acquire);
										released.size += sizeof(Array<>) + metadata->object_size * array->count;
									}
									flag.registered &= ~mask;
									if (!deregistered) {
										on_free_list = page->on_empty_list.exchange(true, std::memory_order_relaxed);
										deregistered = true;
									}
									states[index].store(States::Unused, std::memory_order_relaxed);
								}
							}
						}
					}
					if (deregistered && !on_free_list) {
						if (!page->metadata->empty_page) {
							page->metadata->next = metadata;
							metadata = page->metadata;
						}
						page->next_empty = page->metadata->empty_page;
						page->metadata->empty_page = page;
					}
					page = page->next_registered;
				}
				while(metadata) {
					metadata->free(metadata->empty_page);
					metadata->empty_page = nullptr;
					metadata = metadata->next;
				}
				Page* prev = nullptr;
				page = _registered_pages;
				while(page) {
					auto next = page->next_registered;
					if (!page->is_used) {
						if (!prev) {
							_registered_pages = next;
						} else {
							prev->next_registered = next;
						}
					} else {
						prev = page;
					}
					page = next;
				}
				return released;
			}

			void _main_loop() noexcept {
				static constexpr size_t MinLiveSize = PageSize;
				static constexpr size_t MinLiveCount = MinLiveSize / sizeof(uintptr_t) * 2;
#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
				using namespace std::chrono_literals;
				int finalization_counter = 5;
				Counter allocated;
				Counter removed;
				do {
#if SGCL_LOG_PRINT_LEVEL >= 2
					auto start = std::chrono::high_resolution_clock::now();
#endif
					if (_check_threads()) {
						_update_states();
						_mark_live_objects();
						_mark_stack();
						_mark_roots();
						while(_reachable_pages) {
							_mark_reachable();
							_mark_updated();
						}
						Counter last_removed = _remove_garbage();
						Counter last_allocated = _alloc_counter() - allocated;
						Counter live = allocated + last_allocated - (removed + last_removed);
#if SGCL_LOG_PRINT_LEVEL >= 2
						auto end = std::chrono::high_resolution_clock::now();
						std::cout << "[sgcl] live objects: " << live.count << ", destroyed: " << last_removed.count << ", time: "
											<< std::chrono::duration<double, std::milli>(end - start).count() << "ms"
											<< std::endl;
#endif
						Timer timer;
						do {
							auto last_allocated = _alloc_counter() - allocated;
							auto live = allocated + last_allocated - (removed + last_removed);
							if ((std::max(last_allocated.count, last_removed.count) * 100 / SGCL_TRIGER_PERCENTAGE >= live.count + MinLiveCount)
							 || (std::max(last_allocated.size, last_removed.size) * 100 / SGCL_TRIGER_PERCENTAGE >= live.size + MinLiveSize)
							 || aborted()) {
								break;
							}
							std::this_thread::sleep_for(1ms);
						} while(!live.count || timer.duration() < SGCL_MAX_SLEEP_TIME_SEC * 1000);
						allocated += last_allocated;
						removed += last_removed;
						if (!last_removed.count && aborted()) {
							if (live.count) {
								--finalization_counter;
							} else {
								finalization_counter = 0;
							}
						}
					}
					if (!aborted()) {
						std::this_thread::yield();
					}
				} while(finalization_counter);

#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
#endif
			}

			Page* _reachable_pages = {nullptr};
			Page* _registered_pages = {nullptr};
			Counter _allocated_rest;		

			inline static std::atomic<bool> _abort = {false};
		};

		Thread::~Thread() {
			using namespace std::chrono_literals;
			_data->is_used.store(false, std::memory_order_release);
			if (std::this_thread::get_id() == main_thread_id) {
				Collector::abort();
			}
#if SGCL_LOG_PRINT_LEVEL >= 3
			std::cout << "[sgcl] stop thread id: " << std::this_thread::get_id() << std::endl;
#endif
		}

		class Tracked_ptr {
#ifdef SGCL_ARCH_X86_64
			static constexpr uintptr_t StackFlag = uintptr_t(1) << 63;
			static constexpr uintptr_t ExternalHeapFlag = uintptr_t(1) << 62;
#else
			static constexpr uintptr_t StackFlag = 1;
			static constexpr uintptr_t ExternalHeapFlag = 2;
#endif
			static constexpr uintptr_t ClearMask = ~(StackFlag | ExternalHeapFlag);

		protected:
			Tracked_ptr(Tracked_ptr* p = nullptr) {
				uintptr_t this_addr = (uintptr_t)this;
				uintptr_t stack_addr = (uintptr_t)&this_addr;
				ptrdiff_t offset = this_addr - stack_addr;
				bool stack = -MaxStackOffset <= offset && offset <= MaxStackOffset;
				if (!stack) {
					auto& thread = current_thread();
					auto& state = thread.alloc_state;
					bool root = !(state.range.first <= this_addr && this_addr < state.range.second);
					if (!root) {
						if (state.ptrs) {
							state.ptrs[state.size++] = &_ptr_value;
						}
#ifdef SGCL_ARCH_X86_64
						_ref = nullptr;
#else
						_ref = &_ptr_value;
#endif
					} else {
						if (p && p->_allocated_on_external_heap()) {
							_ref = p->_ref;
						} else {
							auto ref = thread.roots_allocator->alloc();
							_ref = _set_flag(ref, ExternalHeapFlag);
						}
					}
				} else {
					auto ref = current_thread().stack_allocator->alloc(this);
					_ref = _set_flag(ref, StackFlag);
				}
			}

			~Tracked_ptr() {
#ifdef SGCL_DEBUG
				if (_allocated_on_heap()) {
					_store_no_update((void*)1);
					return;
				}
#endif
				if (_ref && !_allocated_on_heap()) {
					_store(nullptr);
					if (!_allocated_on_stack() && !Collector::aborted()) {
						current_thread().roots_allocator->free(_ref);
					}
				}
			}

			Tracked_ptr(const Tracked_ptr& r) = delete;
			Tracked_ptr& operator=(const Tracked_ptr&) = delete;

			void _clear_ref() noexcept {
				_ref = nullptr;
			}

			bool _allocated_on_heap() const noexcept {
				return !((uintptr_t)_ref & (StackFlag | ExternalHeapFlag));
			}

			bool _allocated_on_stack() const noexcept {
				return (uintptr_t)_ref & StackFlag;
			}

			bool _allocated_on_external_heap() const noexcept {
				return (uintptr_t)_ref & ExternalHeapFlag;
			}

			static void _update(const void* p) noexcept {
				if (p) {
					Page::set_state(p, States::Reachable);
				}
			}

			static void _update_atomic(const void* p) noexcept {
				if (p) {
					Page::set_state(p, States::AtomicReachable);
				}
			}

			void* _load() const noexcept {
				return _ptr()->load(std::memory_order_relaxed);
			}

			void* _load(const std::memory_order m) const noexcept {
				auto l = _ptr()->load(m);
				_update_atomic(l);
				return l;
			}

			void _store(std::nullptr_t) noexcept {
				_ptr()->store(nullptr, std::memory_order_relaxed);
			}

			void _store_no_update(const void* p) noexcept {
				_ptr()->store(const_cast<void*>(p), std::memory_order_relaxed);
			}

			void _store(const void* p) noexcept {
				_store_no_update(p);
				_update(p);
			}

			void _store(const void* p, const std::memory_order m) noexcept {
				_ptr()->store(const_cast<void*>(p), m);
				_update_atomic(p);
			}

			void* _exchange(const void* p, const std::memory_order m) noexcept {
				auto l = _load();
				_update_atomic(l);
				_update_atomic(p);
				l = _ptr()->exchange(const_cast<void*>(p), m);
				return l;
			}

			template<class F>
			bool _compare_exchange(void*& o, const void* n, F f) noexcept {
				auto l = _load();
				_update_atomic(l);
				_update_atomic(n);
				return f(_ptr(), o, n);
			}

			void* _base_address_of(const void* p) const noexcept {
				return Page::base_address_of(p);
			}

			template<class T>
			metadata_base& _metadata() const noexcept {
				if constexpr(std::is_array_v<T>) {
					return Page_info<T>::public_metadata();
				} else {
					auto p = _load();
					if (p) {
						return Page::metadata_of(p).public_metadata;
					} else {
						return Page_info<T>::public_metadata();
					}
				}
			}

		private:
			template<class T>
			static T _set_flag(T p, uintptr_t f) noexcept {
				auto v = (uintptr_t)p | f;
				return (T)v;
			}

			template<class T>
			static constexpr T _remove_flags(T p) noexcept {
				auto v = (uintptr_t)p & ClearMask;
				return (T)v;
			}

#ifdef SGCL_ARCH_X86_64
			Pointer* _ptr() noexcept {
				if (_allocated_on_heap()) {
					return &_ptr_value;
				}
				return _remove_flags(_ref);
			}

			const Pointer* _ptr() const noexcept {
				if (_allocated_on_heap()) {
					return &_ptr_value;
				}
				return _remove_flags(_ref);
			}

			union {
				Pointer* _ref;
				Pointer _ptr_value;
			};
#else
			Pointer* _ptr() noexcept {
				return _remove_flags(_ref);
			}

			const Pointer* _ptr() const noexcept {
				return _remove_flags(_ref);
			}

			Pointer* _ref;
			Pointer _ptr_value;
#endif
			template<size_t> friend struct Array;
		};

		template<size_t S>
		template<class T, class... A>
		void Array<S>::_init(A&&... a) {
			using Info = Page_info<std::remove_cv_t<T>>;
			Pointer** ptrs = nullptr;
			if (!Info::pointer_offsets.load(std::memory_order_acquire)) {
				ptrs = (Pointer**)(::operator new(sizeof(T)));
			}
			auto& thread = current_thread();
			auto& state = thread.alloc_state;
			auto old_state = state;
			state = {{(uintptr_t)(this->data), (uintptr_t)((T*)this->data + count)}, 0, ptrs};
			try {
				new(const_cast<char*>(this->data)) T(std::forward<A>(a)...);
			}
			catch (...) {
				if (ptrs) {
					::operator delete(ptrs);
				}
				state = old_state;
				throw;
			}
			if (ptrs) {
				auto offsets = new ptrdiff_t[state.size + 1];
				offsets[0] = state.size;
				for (size_t i = 0; i < state.size; ++i) {
					offsets[i + 1] = (uintptr_t)ptrs[i] - (uintptr_t)this->data;
				}
				::operator delete(ptrs);
				ptrdiff_t* old = nullptr;
				if (!Info::pointer_offsets.compare_exchange_strong(old, offsets)) {
					::operator delete(offsets);
				}
			}
			state.ptrs = nullptr;
			if constexpr(std::is_base_of_v<Tracked_ptr, T>) {
				for (size_t i = 1; i < count; ++i) {
					auto p = (Tracked_ptr*)const_cast<std::remove_cv_t<T>*>((T*)this->data + i);
					if constexpr(sizeof...(A)) {
						p->_ptr_value.store((a.get())..., std::memory_order_relaxed);
					} else {
						p->_ptr_value.store(nullptr, std::memory_order_relaxed);
					}
#ifndef SGCL_ARCH_X86_64
					p->_ref = &p->_ptr_value;
#endif
				}
			} else {
				for (size_t i = 1; i < count; ++i) {
					new(const_cast<std::remove_cv_t<T>*>((T*)this->data + i)) T(std::forward<A>(a)...);
				}
			}
			state = old_state;
		}

		void collector_init() {
			static Collector collector_instance;
		}

		template<class T, class U = T, class ...A>
		static tracked_ptr<U> Make_tracked(size_t size, A&&... a) {
			using Info = Page_info<std::remove_cv_t<T>>;
			collector_init();

			Pointer** ptrs = nullptr;
			if (!Info::pointer_offsets.load(std::memory_order_acquire)) {
				ptrs = (Pointer**)(::operator new(sizeof(T)));
			}

			auto& thread = current_thread();
			auto& allocator = thread.alocator<std::remove_cv_t<T>>();
			auto mem = (T*)allocator.alloc(size);
			bool first_recursive_pointer = thread.set_recursive_alloc_pointer(mem);

			auto& state = thread.alloc_state;
			auto old_state = state;
			state = {{(uintptr_t)(mem), (uintptr_t)(mem + 1)}, 0, ptrs};

			std::remove_cv_t<T>* ptr;
			try {
				ptr = const_cast<std::remove_cv_t<T>*>(new(mem) T(std::forward<A>(a)...));
				thread.update_allocated(sizeof(T) + size);
			}
			catch (...) {
				Page::set_state(mem, States::BadAlloc);
				if (ptrs) {
					::operator delete(ptrs);
				}
				state = old_state;
				throw;
			}

			if (ptrs) {
				auto offsets = new ptrdiff_t[state.size + 1];
				offsets[0] = state.size;
				for (size_t i = 0; i < state.size; ++i) {
					offsets[i + 1] = (uintptr_t)ptrs[i] - (uintptr_t)ptr;
				}
				::operator delete(ptrs);
				ptrdiff_t* old = nullptr;
				if (!Info::pointer_offsets.compare_exchange_strong(old, offsets)) {
					::operator delete(offsets);
				}
			}

			state = old_state;

			if constexpr(std::is_same_v<T, U>) {
				return tracked_ptr<T>(ptr, [&]{
					if (first_recursive_pointer) {
						thread.clear_recursive_alloc_pointer();
					}
				});
			} else {
				return tracked_ptr<U>(ptr->data, [&]{
					if (first_recursive_pointer) {
						thread.clear_recursive_alloc_pointer();
					}
				});
			}
		}
	}; // namespace Priv


	template<class T>
	class tracked_ptr : Priv::Tracked_ptr {
	public:
		using element_type = std::remove_extent_t<T>;

		constexpr tracked_ptr()
		: Tracked_ptr() {
			_store(nullptr);
		}

		constexpr tracked_ptr(std::nullptr_t)
		: Tracked_ptr() {
			_store(nullptr);
		}

		template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
		explicit tracked_ptr(U* p)
		: Tracked_ptr() {
			_store(p);
		}

		tracked_ptr(const tracked_ptr& p)
		: Tracked_ptr() {
			_store(p.get());
		}

		template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
		tracked_ptr(const tracked_ptr<U>& p)
		: Tracked_ptr() {
			_store(static_cast<T*>(p.get()));
		}

		tracked_ptr(tracked_ptr&& p)
		: Tracked_ptr(&p) {
			if (_allocated_on_external_heap() && p._allocated_on_external_heap()) {
				p._clear_ref();
			} else {
				_store(p.get());
				p._store(nullptr);
			}
		}

		template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
		tracked_ptr(tracked_ptr<U>&& p)
		: Tracked_ptr(&p) {
			if (_allocated_on_external_heap() && p._allocated_on_external_heap()) {
				p._clear_ref();
			} else {
				_store(static_cast<T*>(p.get()));
				p._store(nullptr);
			}
		}

		tracked_ptr& operator=(std::nullptr_t) noexcept {
			_store(nullptr);
			return *this;
		}

		tracked_ptr& operator=(const tracked_ptr& p) noexcept {
			_store(p.get());
			return *this;
		}

		template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
		tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept {
			_store(static_cast<T*>(p.get()));
			return *this;
		}

		tracked_ptr& operator=(tracked_ptr&& p) noexcept {
			_store(p.get());
			p._store(nullptr);
			return *this;
		}

		template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
		tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept {
			_store(static_cast<T*>(p.get()));
			p._store(nullptr);
			return *this;
		}

		explicit operator bool() const noexcept {
			return (get() != nullptr);
		}

		template <class U = T, std::enable_if_t<!std::disjunction_v<std::is_void<U>, std::is_array<U>>, int> = 0>
		U& operator*() const noexcept {
			assert(get() != nullptr);
			return *get();
		}

		template <class U = T, std::enable_if_t<!std::disjunction_v<std::is_void<U>, std::is_array<U>>, int> = 0>
		U* operator->() const noexcept {
			assert(get() != nullptr);
			return get();
		}

		template <class U = T, class E = element_type, std::enable_if_t<std::is_array_v<U>, int> = 0>
		E& operator[](size_t i) const noexcept {
			assert(get() != nullptr);
			return get()[i];
		}

		element_type* get() const noexcept {
			return (element_type*)_load();
		}

		void reset() noexcept {
			_store(nullptr);
		}

		void swap(tracked_ptr& p) noexcept {
			auto l = get();
			_store(p.get());
			p._store(l);
		}

		template <class U = T, std::enable_if_t<!std::is_array_v<U>, int> = 0>
		tracked_ptr<T> clone() const {
			auto p = Priv::Page::metadata_of(get()).clone(get());
			return tracked_ptr<T>((T*)p.get());
		}

		template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
		bool is() const noexcept {
			return type() == typeid(U);
		}

		template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
		tracked_ptr<U> as() const noexcept {
			if (is<U>()) {
				auto address = _base_address_of(get());
				return tracked_ptr<U>((U*)address);
			} else {
				return {nullptr};
			}
		}

		const std::type_info& type() const noexcept {
			return _metadata().type;
		}

		const std::any& metadata() const noexcept {
			return _metadata().user_data;
		}

		template<class U>
		const U& metadata() const noexcept {
			return *std::any_cast<U>(&_metadata().user_data);
		}

	private:
		// for make
		template<class F>
		explicit tracked_ptr(void* p, F f)
		: Tracked_ptr() {
			_store(p);
			f();
		}

		tracked_ptr(const tracked_ptr& p, const std::memory_order m)
		: Tracked_ptr() {
			auto r = p._load(m);
			_store_no_update(r);
		}

		auto _metadata() const noexcept {
			return Tracked_ptr::_metadata<T>();
		}

		template<class> friend class tracked_ptr;
		template<class> friend class atomic_ptr;
		template<class, class U, class ...A> friend tracked_ptr<U> Priv::Make_tracked(size_t, A&&...);
		friend std::atomic<tracked_ptr>;
	};

	template<class T, class U>
	inline bool operator==(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		return a.get() == b.get();
	}

	template<class T>
	inline bool operator==(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return !a;
	}

	template<class T>
	inline bool operator==(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return !a;
	}

	template<class T, class U>
	inline bool operator!=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		return !(a == b);
	}

	template<class T>
	inline bool operator!=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return (bool)a;
	}

	template<class T>
	inline bool operator!=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return (bool)a;
	}

	template<class T, class U>
	inline bool operator<(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		using V = typename std::common_type<T*, U*>::type;
		return std::less<V>()(a.get(), b.get());
	}

	template<class T>
	inline bool operator<(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return std::less<T*>()(a.get(), nullptr);
	}

	template<class T>
	inline bool operator<(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return std::less<T*>()(nullptr, a.get());
	}

	template<class T, class U>
	inline bool operator<=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		return !(b < a);
	}

	template<class T>
	inline bool operator<=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return !(nullptr < a);
	}

	template<class T>
	inline bool operator<=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return !(a < nullptr);
	}

	template<class T, class U>
	inline bool operator>(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		return (b < a);
	}

	template<class T>
	inline bool operator>(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return nullptr < a;
	}

	template<class T>
	inline bool operator>(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return a < nullptr;
	}

	template<class T, class U>
	inline bool operator>=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
		return !(a < b);
	}

	template<class T>
	inline bool operator>=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
		return !(a < nullptr);
	}

	template<class T>
	inline bool operator>=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
		return !(nullptr < a);
	}

	template<class T, class U>
	inline tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& r) noexcept {
		return tracked_ptr<T>(static_cast<T*>(r.get()));
	}

	template<class T, class U>
	inline tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& r) noexcept {
		return tracked_ptr<T>(const_cast<T*>(r.get()));
	}

	template<class T, class U>
	inline tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& r) noexcept {
		return tracked_ptr<T>(dynamic_cast<T*>(r.get()));
	}

	template<class T>
	std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p) {
		s << p.get();
		return s;
	}

	namespace Priv {
		template<class T>
		tracked_ptr<void> Clone(const void* p) {
			if constexpr (std::is_copy_constructible_v<T>) {
				return p ? make_tracked<T>(*((const T*)p)) : nullptr;
			} else {
				std::ignore = p;
				assert(!"[sgcl] clone(): no copy constructor");
				return nullptr;
			}
		}

		template<class T>
		struct Maker {
			template<class ...A>
			static tracked_ptr<T> make_tracked(A&&... a) {
				return Make_tracked<T>(0, std::forward<A>(a)...);
			}
		};

		template<class T>
		struct Maker<T[]> {
			static tracked_ptr<T[]> make_tracked(size_t count) {
				tracked_ptr<T[]> p;
				_make_tracked(count, p);
				return p;
			}
			static tracked_ptr<T[]> make_tracked(size_t count, const std::remove_extent_t<T>& v) {
				tracked_ptr<T[]> p;
				_make_tracked(count, p, &v);
				return p;
			}

		private:
			template<size_t N>
			static void _make(size_t count, tracked_ptr<T[]>& p) {
				if (sizeof(T) * count <= N && sizeof(Array<N>) <= PageDataSize) {
					p = Make_tracked<Array<N>, T[]>(0, count);
				} else {
					if constexpr(sizeof(Array<N>) < PageDataSize) {
						_make<N * 2>(count, p);
					} else {
						if (sizeof(T) * count <= PageDataSize - sizeof(Array_base)) {
							p = Make_tracked<Array<PageDataSize - sizeof(Array_base)>, T[]>(0, count);
						} else {
							p = Make_tracked<Array<>, T[]>(sizeof(T) * count, count);
						}
					}
				}
			}
			static void _make_tracked(size_t count, tracked_ptr<T[]>& p, const std::remove_extent_t<T>* v = nullptr) {
				if (count) {
					auto offset = (size_t)(&((Array<>*)0)->data);
					_make<1>(count, p);
					auto array = (Array<>*)((char*)p.get() - offset);
					if (v) {
						array->template init<T>(*v);
					} else {
						array->template init<T>();
					}
				}
			}
		};
	}

	template<class T, class ...A>
	auto make_tracked(A&&... a) {
		return Priv::Maker<T>::make_tracked(std::forward<A>(a)...);
	}
}

template<class T>
struct std::atomic<sgcl::tracked_ptr<T>> {
	using value_type = sgcl::tracked_ptr<T>;

	atomic() = default;

	atomic(std::nullptr_t)
	: _ptr(nullptr) {
	}

	atomic(const value_type& p)
	: _ptr(p) {
	}

	atomic(const atomic&) = delete;
	atomic& operator =(const atomic&) = delete;

	static constexpr bool is_always_lock_free = std::atomic<T*>::is_always_lock_free;

	bool is_lock_free() const noexcept {
		return std::atomic<T*>::is_lock_free();
	}

	void operator=(const value_type& p) noexcept {
		store(p);
	}

	void store(const value_type& p, const memory_order m = std::memory_order_seq_cst) noexcept {
		_store(p, m);
	}

	value_type load(const memory_order m = std::memory_order_seq_cst) const {
		return _load(m);
	}

	void load(value_type& o, const memory_order m = std::memory_order_seq_cst) const noexcept {
		_load(o, m);
	}

	value_type exchange(const value_type& p, const memory_order m = std::memory_order_seq_cst) {
		return _exchange(p, m);
	}

	void exchange(value_type& o, const value_type& p, const memory_order m = std::memory_order_seq_cst) noexcept {
		_exchange(o, p, m);
	}

	bool compare_exchange_strong(value_type& o, const value_type& n, const memory_order m = std::memory_order_seq_cst) noexcept {
		return _compare_exchange_strong(o, n, m);
	}

	bool compare_exchange_strong(value_type& o, const value_type& n, const memory_order s, const memory_order f) noexcept {
		return _compare_exchange_strong(o, n, s, f);
	}

	bool compare_exchange_weak(value_type& o, const value_type& n, const memory_order m = std::memory_order_seq_cst) noexcept {
		return _compare_exchange_weak(o, n, m);
	}

	bool compare_exchange_weak(value_type& o, const value_type& n, const memory_order s, const memory_order f) noexcept {
		return _compare_exchange_weak(o, n, s, f);
	}

	operator value_type() const noexcept {
		return load();
	}

private:
	value_type _load(const std::memory_order m = std::memory_order_seq_cst) const {
		return value_type(_ptr, m);
	}

	void _load(value_type& o, const std::memory_order m = std::memory_order_seq_cst) const noexcept {
		auto l = _ptr._load(m);
		o._store_no_update(l);
	}

	void _store(const value_type& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
		auto r = p.get();
		_ptr._store(r, m);
	}

	value_type _exchange(const value_type& p, const std::memory_order m = std::memory_order_seq_cst) {
		auto r = p.get();
		return value_type((T*)_ptr._exchange(r, m));
	}

	void _exchange(value_type& o, const value_type& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
		auto r = p.get();
		auto l = _ptr._exchange(r, m);
		o._store_no_update(l);
	}

	bool _compare_exchange_weak(value_type& o, const value_type& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
		return _compare_exchange(o, n, [m](auto p, auto& l, auto r){ return p->compare_exchange_weak(l, const_cast<void*>(r), m); });
	}

	bool _compare_exchange_weak(value_type& o, const value_type& n, std::memory_order s, std::memory_order f) noexcept {
		return _compare_exchange(o, n, [s, f](auto p, auto& l, auto r){ return p->compare_exchange_weak(l, const_cast<void*>(r), s, f); });
	}

	bool _compare_exchange_strong(value_type& o, const value_type& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
		return _compare_exchange(o, n, [m](auto p, auto& l, auto r){ return p->compare_exchange_strong(l, const_cast<void*>(r), m); });
	}

	bool _compare_exchange_strong(value_type& o, const value_type& n, std::memory_order s, std::memory_order f) noexcept {
		return _compare_exchange(o, n, [s, f](auto p, auto& l, auto r){ return p->compare_exchange_strong(l, const_cast<void*>(r), s, f); });
	}

	template<class F>
	bool _compare_exchange(value_type& o, const value_type& n, F f) noexcept {
		auto r = n.get();
		void* l = const_cast<std::remove_cv_t<typename value_type::element_type>*>(o.get());
		if (!_ptr._compare_exchange(l, r, f)) {
			o._store_no_update(l);
			return false;
		}
		return true;
	}

	sgcl::tracked_ptr<T> _ptr;
};

#endif // SGCL_H
