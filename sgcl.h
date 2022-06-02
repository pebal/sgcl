/*******************************************************************************
** SGCL - Concurrent Garbage Collector
** Copyright (c) 2022 Sebastian Nibisz
** Distributed under the MIT License.
*******************************************************************************/
#ifndef SGCL_H
#define SGCL_H

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <type_traits>
#include <thread>

#define SGCL_MAX_SLEEP_TIME_SEC 10
#define SGCL_TRIGER_PERCENTAGE 25
#define SGCL_LOG_PRINT_LEVEL 1

#if SGCL_LOG_PRINT_LEVEL
#include <iostream>
#endif

#ifdef __MINGW32__
#define GC_MAX_STACK_OFFSET 1024
#endif

namespace gc {
	class collected;
	class object;

	template<typename T, typename = std::enable_if_t<std::is_convertible_v<T*, const object*>>>
	class ptr;

	template<typename T>
	class ptr<T[]>;

	template<typename T, size_t N>
	class ptr<T[N]>;

	template<typename T, typename = std::enable_if_t<std::is_convertible_v<T*, const object*>>>
	class weak_ptr;

	template<typename T>
	class weak_ptr<T[]>;

	template<typename T, size_t N>
	class weak_ptr<T[N]>;

	template<typename T, typename = std::enable_if_t<std::is_convertible_v<T*, const object*>>>
	class atomic_ptr;

	template<typename T>
	class atomic_ptr<T[]>;

	template<typename T, size_t N>
	class atomic_ptr<T[N]>;

	template<typename T, typename = std::enable_if_t<std::is_convertible_v<T*, const object*>>>
	class atomic_weak_ptr;

	template<typename T>
	class atomic_weak_ptr<T[]>;

	template<typename T, size_t N>
	class atomic_weak_ptr<T[N]>;

	template<typename T, typename ...A>
	std::enable_if_t<std::is_convertible_v<T*, const object*>, ptr<T>> make(A&&...);

	struct Priv {
		Priv() = delete;

		class Ptr;
		class Weak_ptr;
		class Object;
		class Memory_block;

		struct Object_metadata {
			std::atomic<Object*> next_allocated_object = {nullptr};
			Object* prev_object = {nullptr};
			Object* next_object = {nullptr};
			union {
				Ptr* first_ptr = {nullptr};
				std::atomic<Memory_block*> first_memory_block;
			};
			union {
				Object* (*vget)(void*);
				Object* (*clone)(const Object*);
				void* raw_ptr;
			};
			std::atomic_uint ref_counter = {0};
			std::atomic_uint weak_ref_counter = {0};
			std::atomic_int8_t update_counter = {1};
			std::atomic_bool next_object_can_registered = {true};
			bool can_remove_object = {false};
			bool is_reachable = {true};
			bool is_managed = {false};
			bool is_container = {false};
			bool is_registered = {false};

			static constexpr int8_t atomic_update_value = std::numeric_limits<int8_t>::max();
			static constexpr int8_t lock_update_value = std::numeric_limits<int8_t>::min();
		};

		class Collector;
		struct Memory;
		struct Thread_data;

		class Object {
		protected:
			Object() = default;
			Object(const Object&) noexcept {}

			virtual ~Object() = default;

			Object& operator=(const Object&) noexcept {
				return *this;
			}

			union {
				mutable Object_metadata metadata = {};
			};

			friend struct Priv;
			template<typename, typename> friend class ptr;
		};

		static Thread_data& local_thread_data() {
			static thread_local Thread_data data_instance;
			return data_instance;
		}

		class Memory_block {
		public:
			struct data {
				Object* get() const noexcept {
					return ptr.load(std::memory_order_relaxed);
				}

				Object* vget() const noexcept {
					return ptr.load(std::memory_order_acquire);
				}

				void set(Object* p) noexcept {
					ptr.store(p, std::memory_order_release);
					if (p && !p->metadata.update_counter.load(std::memory_order_acquire)) {
						p->metadata.update_counter.store(1, std::memory_order_release);
					}
				}

				std::atomic<Object*> ptr = {nullptr};
				union {
					Memory_block* memory_block;
					struct {
						unsigned offset;
						unsigned next_data;
					};
				};
			};

			Memory_block(data* const b, unsigned s)
			: buffer(b)
			, size(s)
			, free_counter(s) {
				for (unsigned i = 0; i < size; ++i) {
					buffer[i].offset = i;
					buffer[i].next_data = i + 1;
				}
			}

			virtual ~Memory_block() = default;

			data* alloc() noexcept {
				if (free_counter) {
					data* c = &buffer[_free_cell];
					_free_cell = buffer[_free_cell].next_data;
					--free_counter;
					return c;
				}
				_blocked = true;
				return nullptr;
			}

			void free(data* d) noexcept {
				d->next_data = _free_cell;
				_free_cell = d->offset;
				++free_counter;
				if (free_counter == size) {
					is_rechable.store(false, std::memory_order_release);
				}
			}

			bool realloc_request() const noexcept {
				return _blocked && free_counter >= size / 2;
			}

			Memory_block* prev_block = {nullptr};
			std::atomic<Memory_block*> next_block = {nullptr};
			data* const buffer;
			const unsigned size;
			unsigned free_counter;
			std::atomic<bool> is_rechable = {true};

		private:
			unsigned _free_cell = 0;
			bool _blocked = {false};
		};

		template<unsigned Size = 16>
		struct Memory_block_data : Memory_block {
			Memory_block_data()
			: Memory_block(buffer, Size) {
			}
			data buffer[Size];
		};

		struct Memory_buffer : Object {
			Memory_buffer(bool s)
			: _fixed_size(s) {
				metadata.is_container = true;
				new (&metadata.first_memory_block) std::atomic<Memory_block*>(nullptr);
			}

			Memory_block::data* alloc() {
				Memory_block::data* data = nullptr;
				auto last_block = _block;
				if (_block) {
					data = _block->alloc();
					if (!data) {
						_block = nullptr;
					}
				}
				if (!_block) {
					_block = _new_block();
					if (last_block) {
						_block->prev_block = last_block;
						last_block->next_block.store(_block, std::memory_order_release);
					} else {
						metadata.first_memory_block.store(_block, std::memory_order_release);
					}
					data = _block->alloc();
				}
				++_size;
				return data;
			}

			void free(Memory_block::data* d) noexcept {
				auto* block = _block_of(d);
				block->free(d);
				--_size;
			}

			void realloc(Memory_block::data*& d) noexcept {
				auto data = alloc();
				data->set(d->get());
				free(d);
				d = data;
			}

			void update(Memory_block::data*& d) noexcept {
				if (_block_of(d)->realloc_request()) {
					realloc(d);
				}
			}

		private:
			const bool _fixed_size;
			unsigned _size = {0};
			Memory_block* _block = {nullptr};

			static constexpr unsigned _index_of(unsigned s, unsigned i = 0) noexcept {
				return s ? _index_of(s / 2, i + 1) : i;
			}

			static Memory_block* _block_of(Memory_block::data* d) noexcept {
				Memory_block* block = (Memory_block_data<>*)((char*)(d - d->offset) - (char*)&(((Memory_block_data<>*)nullptr)->buffer));
				return block;
			}

			Memory_block* _new_block() const {
				static std::function<Memory_block*()> new_block[] = {
					[]{ return new Memory_block_data<16>(); },
					[]{ return new Memory_block_data<32>(); },
					[]{ return new Memory_block_data<64>(); },
					[]{ return new Memory_block_data<128>(); },
					[]{ return new Memory_block_data<256>(); },
					[]{ return new Memory_block_data<512>(); },
					[]{ return new Memory_block_data<1024>(); },
					[]{ return new Memory_block_data<2048>(); },
					[]{ return new Memory_block_data<4096>(); }
				};
				return _fixed_size ? new_block[0]() : new_block[std::min((unsigned)std::size(new_block) - 1, std::max(4u, _index_of(_size)) - 3)]();
			}
		};

		struct Memory {
			struct alloc_state {
				std::pair<Ptr*, Ptr*> range;
				Ptr* first_ptr;
				Ptr* last_ptr;
			};

			static alloc_state& local_alloc_state() {
				static thread_local alloc_state _local_alloc_state = {{nullptr, nullptr}, nullptr, nullptr};
				return _local_alloc_state;
			}

			static void register_object(Object* p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_object->metadata.next_allocated_object.store(p, std::memory_order_release);
				thread_data.last_allocated_object = p;
			}

			static void register_container(Object* p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_container->metadata.next_allocated_object.store(p, std::memory_order_release);
				thread_data.last_allocated_container = p;
			}

			static Object* try_lock_last_ptr() noexcept {
				auto& thread_data = local_thread_data();
				auto last_ptr = thread_data.last_allocated_object;
				if (last_ptr->metadata.next_object_can_registered.load(std::memory_order_relaxed)) {
					last_ptr->metadata.next_object_can_registered.store(false, std::memory_order_release);
					return last_ptr;
				}
				return nullptr;
			}

			static void unlock_last_ptr(Object* p) noexcept {
				if (p) {
					p->metadata.next_object_can_registered.store(true, std::memory_order_release);
				}
			}
		};

		struct Thread_data {
			struct local_list : Object {
				explicit local_list(Object* p = nullptr) noexcept
				: first_ptr(p) {
					metadata.is_reachable = false;
					metadata.can_remove_object = true;
					if (p) {
						p->metadata.is_reachable = false;
					}
				}
				std::atomic<bool> is_reachable = {true};
				Object* first_ptr;
				std::atomic<local_list*> next_local_list = {nullptr};
			};

			struct global_list {
				global_list() {
					auto new_list = new local_list;
					first_local_list = new_list;
					last_local_list.store(new_list, std::memory_order_release);
				}
				local_list* first_local_list;
				std::atomic<local_list*> last_local_list;
			};

			Thread_data()
			: last_allocated_object(new Object)
			, last_allocated_container(new Object)
			, _local_list(new local_list {last_allocated_object}) {
#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] start thread id: " << std::this_thread::get_id() << std::endl;
#endif
				auto old_list = list.last_local_list.exchange(_local_list, std::memory_order_relaxed);
				old_list->next_local_list.store(_local_list, std::memory_order_release);
			}

			~Thread_data() {
#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] stop thread id: " << std::this_thread::get_id() << std::endl;
#endif
				_local_list->is_reachable.store(false, std::memory_order_release);
			}

			volatile bool init = {false};
			Object* last_allocated_object;
			Object* last_allocated_container;
			inline static global_list list = {};

		private:
			local_list* const _local_list;
		};

		class Ptr {
		public:
			Ptr() noexcept {
#if GC_MAX_STACK_OFFSET
				int local = 0;
				size_t this_addr = reinterpret_cast<size_t>(this);
				size_t local_addr = reinterpret_cast<size_t>(&local);
				size_t offset = local_addr > this_addr ? local_addr - this_addr : this_addr - local_addr;

				if (offset > GC_MAX_STACK_OFFSET) {
#endif
					auto& state = Memory::local_alloc_state();
					bool root = !(this >= state.range.first && this < state.range.second);
					this->_type = root ? Ptr_type::root : Ptr_type::undefined;
					if (!root) {
						if (!state.last_ptr){
							state.first_ptr = state.last_ptr = this;
						} else {
							state.last_ptr = state.last_ptr->_next_ptr = this;
						}
					}
#if GC_MAX_STACK_OFFSET
				} else {
					this->_type = Ptr_type::root;
				}
#endif
			}

			Ptr(const Ptr&) = delete;
			virtual ~Ptr() = default;
			Ptr& operator=(const Ptr&) = delete;

			Ptr* next_ptr() const noexcept {
				return _next_ptr;
			}

			virtual const Object* vget() const noexcept = 0;
			virtual void vreset() noexcept = 0;

		protected:
			bool _is_root() const noexcept {
				return _type == Ptr_type::root;
			}

			static void _update_counter(const Object* p, int v) noexcept {
				if (p) {
					p->metadata.ref_counter.fetch_add(v, std::memory_order_release);
				}
			}

			static void _update(const Object* p) noexcept {
				if (p && !p->metadata.update_counter.load(std::memory_order_acquire)) {
					p->metadata.update_counter.store(1, std::memory_order_release);
				}
			}

			static void _update_atomic(const Object* p) noexcept {
				if (p && p->metadata.update_counter.load(std::memory_order_acquire) < Object_metadata::atomic_update_value) {
					p->metadata.update_counter.store(Object_metadata::atomic_update_value, std::memory_order_release);
				}
			}

			void _create(const Object* p) const noexcept {
				_update(p);
				if (!_is_root()) {
					p->metadata.ref_counter.store(0, std::memory_order_release);
				}
			}

			void _init(const Object* p) const noexcept {
				if (_is_root()) {
					_update_counter(p, 1);
				}
				_update(p);
			}

			void _init_move(const Object* p, bool p_is_root) const noexcept {
				if (p) {
					if (_is_root() && !p_is_root) {
						_update_counter(p, 1);
					} else if (!_is_root() && p_is_root) {
						_update_counter(p, -1);
					}
				}
				_update(p);
			}

			void _set(const Object* l, const Object* r, bool u = true) const noexcept {
				if (_is_root()) {
					_update_counter(l, -1);
					_update_counter(r, 1);
				}
				if (u) {
					_update(r);
				}
			}

			void _set_move(const Object* l, const Object* r, bool p_is_root) const noexcept {
				if (_is_root()) {
					_update_counter(l, -1);
				}
				_init_move(r, p_is_root);
			}

			void _remove(const Object* p) const noexcept {
				if (_is_root()) {
					_update_counter(p, -1);
				}
			}

			Object* _clone(const Object* ptr) const {
				return ptr->metadata.clone(ptr);
			}

		private:
			enum class Ptr_type : size_t {
				undefined = 0,
				root = 1
			};

			union {
				Ptr* _next_ptr;
				Ptr_type _type;
			};
		};

		class Weak_ptr {
		protected:
			inline static void _init(Object* p) noexcept {
				if (p) {
					p->metadata.weak_ref_counter.fetch_add(1, std::memory_order_release);
				}
			}

			inline static void _set(Object* l, Object* r) noexcept {
				if (l != r) {
					_remove(l);
					_init(r);
				}
			}

			inline static void _move(Object* l, Object* r) noexcept {
				if (l != r) {
					_remove(l);
				}
			}

			inline static void _remove(Object* p) noexcept {
				if (p) {
					p->metadata.weak_ref_counter.fetch_sub(1, std::memory_order_release);
				}
			}

			inline static bool _expired(Object* p) noexcept {
				return !p->metadata.weak_ref_counter.load(std::memory_order_acquire);
			}

			inline static bool _try_lock(Object* p) noexcept {
				if (p) {
					p->metadata.ref_counter.fetch_add(1, std::memory_order_release);
					auto counter = p->metadata.update_counter.load(std::memory_order_acquire);
					while(!counter &&
								!p->metadata.update_counter.compare_exchange_weak(counter, 1, std::memory_order_release, std::memory_order_relaxed));
					return counter != Object_metadata::lock_update_value;
				}
				return false;
			}
		};

		class Collector {
		public:
			Collector() {
				_thread = std::thread([this]{_main_loop();});
			}

			~Collector() {
				_abort.store(true, std::memory_order_release);
				_thread.join();
			}

		private:
			struct object_list {
				void push(Object* p) noexcept {
					if (!first) {
						first = last = p;
					} else {
						last->metadata.next_object = p;
						p->metadata.prev_object = last;
						last = p;
					}
				}

				void push(Object* f, Object* l) noexcept {
					if (f) {
						if (!first) {
							first = f;
						} else {
							last->metadata.next_object = f;
							f->metadata.prev_object = last;
						}
						last = l;
					}
				}

				void pop(Object* p) noexcept {
					auto prev = p->metadata.prev_object;
					auto next = p->metadata.next_object;
					if (prev) {
						prev->metadata.next_object = next;
						p->metadata.prev_object = nullptr;
					}
					if (next) {
						next->metadata.prev_object = prev;
						p->metadata.next_object = nullptr;
					}
					if (first == p) {
						first = next;
					}
					if (last == p) {
						last = prev;
					}
				}
				Object* first = {nullptr};
				Object* last = {nullptr};
			};

			void _mark_root_objects() noexcept {
				static auto clock = std::chrono::high_resolution_clock::now();
				//static int counter = 0;
				static double rest = 0;
				auto c = std::chrono::high_resolution_clock::now();
				double duration = std::chrono::duration<double, std::milli>(c - clock).count();
				duration = Object_metadata::atomic_update_value * duration / 100 + rest; // 100ms for atomic_update_value
				int iduration = (int)duration;
				int grain = std::min((int)Object_metadata::atomic_update_value, iduration);
				if (iduration >= 1) {
					clock = c;
					rest = duration - iduration;
				}

				bool abort = _abort.load(std::memory_order_acquire);
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->metadata.next_object;
					int new_counter = 0;
					int counter = ptr->metadata.update_counter.load(std::memory_order_acquire);
					if (counter > 0) {
						new_counter = !abort ? counter == 1 || counter == Object_metadata::atomic_update_value ? counter - 1 : counter - std::min(counter, grain) : 0;
						if (new_counter != counter) {
							ptr->metadata.update_counter.store(new_counter, std::memory_order_release);
						}
					}
					auto ref_counter = ptr->metadata.ref_counter.load(std::memory_order_acquire);
					if (ref_counter > 0 || new_counter > 0) {
						ptr->metadata.is_reachable = true;
						_objects.pop(ptr);
						_reachable_objects.push(ptr);
					} else {
						ptr->metadata.is_reachable = false;
					}
					ptr = next;
				}
			}

			void _mark_local_allocated_objects(Thread_data::local_list* l) {
				bool list_is_unreachable = !l->is_reachable.load(std::memory_order_acquire);
				Object* next = l->first_ptr;
				while(next) {
					Object* ptr = l->first_ptr;
					if (!ptr->metadata.is_registered) {
						(ptr->metadata.is_reachable ? _reachable_objects : _objects).push(ptr);
						ptr->metadata.is_registered = true;
						++_last_allocated_objects_number;
					}
					if (ptr->metadata.next_object_can_registered) {
						next = ptr->metadata.next_allocated_object;
						if (next || list_is_unreachable) {
							ptr->metadata.can_remove_object = true;
							l->first_ptr = next;
						}
					} else {
						next = nullptr;
					}
				}
			}

			void _mark_allocated_objects() noexcept {
				Thread_data::local_list* prev_list = nullptr;
				auto list = _allocated_objects.first_local_list;
				while(list) {
					auto next_list = list->next_local_list.load(std::memory_order_acquire);
					_mark_local_allocated_objects(list);
					if (!list->first_ptr && next_list) {
						_objects.push(list);
						++_last_allocated_objects_number;
						if (!prev_list) {
							_allocated_objects.first_local_list = next_list;
						} else {
							prev_list->next_local_list.store(next_list, std::memory_order_relaxed);
						}
					} else {
						prev_list = list;
					}
					list = next_list;
				}
			}

			void _mark_child_objects(Object*& p) noexcept {
				auto ptr = p ? p->metadata.next_object : _reachable_objects.first;
				while(ptr) {
					auto child_ptr = ptr->metadata.first_ptr;
					while(child_ptr) {
						if (auto p = const_cast<Object*>(child_ptr->vget())) {
							if (!p->metadata.is_reachable) {
								p->metadata.is_reachable = true;
								_objects.pop(p);
								_reachable_objects.push(p);
							}
						}
						child_ptr = child_ptr->next_ptr();
					}
					ptr = ptr->metadata.next_object;
				}
				p = _reachable_objects.last;
			}

			bool _mark_updated_objects() noexcept {
				auto last_processed_object = _reachable_objects.last;
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->metadata.next_object;
					assert(!ptr->metadata.is_reachable);
					if (ptr->metadata.update_counter.load(std::memory_order_acquire)) {
						ptr->metadata.is_reachable = true;
						_objects.pop(ptr);
						_reachable_objects.push(ptr);
					}
					ptr = next;
				}
				return last_processed_object != _reachable_objects.last;
			}

			void _move_objects_to_unreachable() noexcept {
				object_list tmp_list;
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->metadata.next_object;
					if (ptr->metadata.weak_ref_counter.load(std::memory_order_acquire)) {
						auto counter = ptr->metadata.update_counter.load(std::memory_order_acquire);
						while(!counter &&
									!ptr->metadata.update_counter.compare_exchange_weak(counter, Object_metadata::lock_update_value, std::memory_order_release, std::memory_order_relaxed));
						if (counter) {
							_objects.pop(ptr);
							tmp_list.push(ptr);
						}
					}
					ptr = next;
				}
				_unreachable_objects = _objects;
				_objects = tmp_list;
			}

			void _move_reachable_to_objects() noexcept {
				_objects.push(_reachable_objects.first, _reachable_objects.last);
				_reachable_objects = {nullptr, nullptr};
			}

			bool _try_delete(Object* p) noexcept {
				p->metadata.next_object = nullptr;
				p->metadata.prev_object = nullptr;
				if (p->metadata.can_remove_object && p->metadata.weak_ref_counter.load(std::memory_order_acquire) == 0) {
					assert(p->metadata.update_counter.load(std::memory_order_acquire) == 0 ||
								 p->metadata.update_counter.load(std::memory_order_acquire) == Object_metadata::lock_update_value);
					auto raw_ptr = p->metadata.raw_ptr;
					p->metadata.~Object_metadata();
					if (!_abort.load(std::memory_order_acquire)) {
						::operator delete(raw_ptr);
					}
					return true;
				}
				return false;
			}

			void _remove_garbage() noexcept {
				object_list tmp_list;
				auto ptr = _garbage_objects.first;
				while(ptr) {
					auto next = ptr->metadata.next_object;
					if (!_try_delete(ptr)) {
						tmp_list.push(ptr);
					}
					ptr = next;
				}
				_garbage_objects = tmp_list;
			}

			void _sweep() noexcept {
				object_list tmp_list;
				auto ptr = _unreachable_objects.first;
				while(ptr) {
					auto next = ptr->metadata.next_object;
					auto child_ptr = ptr->metadata.first_ptr;
					while(child_ptr) {
						child_ptr->vreset();
						child_ptr = child_ptr->next_ptr();
					}
					ptr->metadata.raw_ptr = dynamic_cast<void*>(ptr);
					ptr->metadata.is_managed = false;
					ptr->~Object();
					if (!_try_delete(ptr)) {
						tmp_list.push(ptr);
					}
					++_last_destroyed_objects_number;
					ptr = next;
				}
				_garbage_objects.push(tmp_list.first, tmp_list.last);
				_unreachable_objects = {nullptr, nullptr};
			}

			void _main_loop() noexcept {
#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
				using namespace std::chrono_literals;
				std::thread destroyer;
				int finalizeation_counter = 2;
				do {
					_mark_root_objects();

					_last_allocated_objects_number = 0;
					int i = 0;
					do {
						_mark_allocated_objects();
						if (_last_allocated_objects_number * 100 / SGCL_TRIGER_PERCENTAGE >= _live_objects_number + 4000 ||
								_last_destroyed_objects_number * 100 / SGCL_TRIGER_PERCENTAGE >= _live_objects_number ||
								_abort.load(std::memory_order_acquire)) {
							break;
						}
						std::this_thread::sleep_for(1ms);
					} while(++i < SGCL_MAX_SLEEP_TIME_SEC * 1000);

					Object* last_processed_object = nullptr;
					do {
						_mark_child_objects(last_processed_object);
					}
					while(_mark_updated_objects());
					if (destroyer.joinable()) {
						destroyer.join();
					}
					_live_objects_number += _last_allocated_objects_number;
					_live_objects_number -= _last_destroyed_objects_number;

#if SGCL_LOG_PRINT_LEVEL == 2
					std::cout << "[sgcl] last registered/destroyed/live objects number: "
										<< _last_allocated_objects_number << "/"
										<< _last_destroyed_objects_number << "/"
										<< _live_objects_number << std::endl;
#endif
					_last_destroyed_objects_number = 0;
					bool garbage = _objects.first != _objects.last;
					_move_objects_to_unreachable();
					destroyer = std::thread([this]{
						_remove_garbage();
						_sweep();
					});
					_move_reachable_to_objects();

					if (!_abort.load(std::memory_order_acquire)) {
						std::this_thread::yield();
					}

					if (_abort.load(std::memory_order_acquire) && !garbage) {
						--finalizeation_counter;
					}
				} while(finalizeation_counter);

				if (destroyer.joinable()) {
					destroyer.join();
				}

#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
				std::cout << "[sgcl] live objects number: " << _live_objects_number << std::endl;
#endif
			}

			std::thread _thread;
			std::atomic<bool> _abort = {false};
			Thread_data::global_list& _allocated_objects = Thread_data::list;
			object_list _objects;
			object_list _reachable_objects;
			object_list _unreachable_objects;
			object_list _garbage_objects;

			size_t _last_allocated_objects_number = {0};
			size_t _last_destroyed_objects_number = {0};
			size_t _live_objects_number = {0};
		};

		inline static Collector collector_instance;

		template<typename T>
		static ptr<T> make_ptr(T* p) noexcept {
			return ptr<T>(p, nullptr);
		}

		template<typename T, typename ...A>
		static T* make(A&&... a) {
			auto last_ptr = Memory::try_lock_last_ptr();

			auto mem = static_cast<Ptr*>(::operator new(sizeof(T)));

			auto& state = Memory::local_alloc_state();
			auto old_state = state;
			state = {{mem, mem + sizeof(T) / sizeof(Ptr*)}, nullptr, nullptr};

			T* obj;
			try {
				obj = new(mem) T(std::forward<A>(a)...);
			}
			catch (...) {
				::operator delete(mem);
				state = old_state;
				Memory::unlock_last_ptr(last_ptr);
				throw;
			}

			auto& metadata = obj->_metadata();
			metadata.first_ptr = state.first_ptr;
			metadata.clone = ptr<T>::_Clone;
			metadata.is_managed = true;
			metadata.ref_counter.store(1, std::memory_order_relaxed);

			state = old_state;

			Memory::register_object(obj);
			Memory::unlock_last_ptr(last_ptr);

			return obj;
		}

		friend class collected;
		friend class object;
		template<typename, typename> friend class ptr;
		template<typename, typename> friend class weak_ptr;
		template<typename T, typename ...A> friend std::enable_if_t<std::is_convertible_v<T*, const object*>, ptr<T>> gc::make(A&&...);
		template<typename> friend struct std::less;

	public:
		template<typename T>
		static auto raw_ptr(const T& p) noexcept {
			return p._get();
		}

		template<class T>
		struct Proxy_ptr {
			template<class ...A>
			Proxy_ptr(A&& ...a) : value(std::forward<A>(a)...) {}
			operator T() const { return  value; }
			T value;
		};
	}; // class Priv

	class object : public Priv::Object {
		void operator&() = delete;
		void *operator new(size_t) = delete;
		void *operator new[](size_t) = delete;

		static void* operator new(std::size_t c, void* p) {
			return ::operator new(c, p);
		}

		Priv::Object_metadata& _metadata() noexcept {
			return metadata;
		}

	public:
		bool is_managed() const noexcept {
			return metadata.is_managed;
		}

		friend struct Priv;
	};

	class collected : public virtual object {
	};

	template<typename T = object, typename>
	class ptr final : protected Priv::Ptr {
	public:
		using value_type = T;

		constexpr ptr() noexcept
		: _ptr(nullptr) {
		}

		constexpr ptr(std::nullptr_t) noexcept
		: _ptr(nullptr) {
		}

		explicit ptr(T* p) noexcept
		: _ptr(p && p->is_managed() ? p : nullptr) {
			assert(!p || p->is_managed());
			Ptr::_init(p);
		}

		ptr(const ptr& p) noexcept
		: Ptr() {
			_init(p, std::memory_order_relaxed);
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr(const ptr<U>& p) noexcept
		: Ptr() {
			_init(p, std::memory_order_relaxed);
		}

		ptr(ptr&& p) noexcept
		: Ptr() {
			_init_move(std::move(p));
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr(ptr<U>&& p) noexcept
		: Ptr() {
			_init_move(std::move(p));
		}

		~ptr() noexcept {
			reset();
		}

		ptr& operator=(std::nullptr_t) noexcept {
			_set(nullptr);
			return *this;
		}

		ptr& operator=(const ptr& p) {
			if (this != &p) {
				_set(p);
			}
			return *this;
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr& operator=(const ptr<U>& p) {
			_set(p);
			return *this;
		}

		ptr& operator=(ptr&& p) noexcept {
			if (this != &p) {
				_move(std::move(p));
			}
			return *this;
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr& operator=(ptr<U>&& p) noexcept {
			_move(std::move(p));
			return *this;
		}

		explicit operator bool() const noexcept {
			return (_get() != nullptr);
		}

		T* operator->() const noexcept {
			assert(_get() != nullptr);
			return _get();
		}

		T& operator*() noexcept {
			assert(_get() != nullptr);
			return *_get();
		}

		const T& operator*() const noexcept {
			assert(_get() != nullptr);
			return *_get();
		}

		ptr<T> clone() const {
			auto p = _get();
			return p ? Priv::make_ptr(dynamic_cast<T*>(_clone(p))) : ptr<T>();
		}

		void reset() noexcept {
			_set(nullptr);
		}

		void swap(ptr& p) noexcept {
			if (this != &p) {
				T* r = p._ptr.load(std::memory_order_relaxed);
				T* l = _ptr.load(std::memory_order_relaxed);
				if (l != r) {
					p._ptr.store(l, std::memory_order_release);
					_ptr.store(r, std::memory_order_release);
					Ptr::_init(r);
					p.Ptr::_init(l);
					Ptr::_remove(l);
					p.Ptr::_remove(r);
				}
			}
		}

	private:
		std::atomic<T*> _ptr;

		// for make
		explicit ptr(T* p, std::nullptr_t) noexcept
		: _ptr(p) {
			Ptr::_create(p);
		}

		ptr(const ptr& p, std::memory_order m) noexcept
		: Ptr() {
			_init(p, m);
		}

		template<typename U>
		void _init(const ptr<U>& p, std::memory_order m) noexcept {
			T* rp = p._ptr.load(m);
			_ptr.store(rp, std::memory_order_release);
			Ptr::_init(rp);
		}

		template<typename U>
		void _init_move(ptr<U>&& p) noexcept {
			T* rp = p._ptr.load(std::memory_order_relaxed);
			_ptr.store(rp, std::memory_order_release);
			Ptr::_init_move(rp, p._is_root());
			p._ptr.store(nullptr, std::memory_order_relaxed);
		}

		void _set(std::nullptr_t) noexcept {
			T* p = _ptr.load(std::memory_order_relaxed);
			if (p != nullptr) {
				_ptr.store(nullptr, std::memory_order_relaxed);
				Ptr::_remove(p);
			}
		}

		template<typename U>
		void _set(const ptr<U>& p) {
			T* r = p._ptr.load(std::memory_order_relaxed);
			T* l = _ptr.load(std::memory_order_relaxed);
			if (l != r) {
				_ptr.store(r, std::memory_order_release);
				Ptr::_set(l, r);
			}
		}

		template<typename U>
		void _move(ptr<U>&& p) noexcept {
			T* r = p._ptr.load(std::memory_order_relaxed);
			T* l = _ptr.load(std::memory_order_relaxed);
			if (l != r) {
				_ptr.store(r, std::memory_order_release);
				Ptr::_set_move(l, r, p._is_root());
			} else {
				p.Ptr::_remove(r);
			}
			p._ptr.store(nullptr, std::memory_order_relaxed);
		}

		T* _get() const noexcept {
			return _ptr.load(std::memory_order_relaxed);
		}

		const Priv::Object* vget() const noexcept override {
			return _ptr.load(std::memory_order_acquire);
		}

		void vreset() noexcept override {
			_ptr.store(nullptr, std::memory_order_relaxed);
		}

		ptr _load(std::memory_order m = std::memory_order_seq_cst) const noexcept {
			return ptr(*this, m);
		}

		void _store(const ptr& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			if (this != &p) {
				T* l = _ptr.load(std::memory_order_acquire);
				T* r = p._ptr.load(std::memory_order_relaxed);
				if (l != r) {
					Ptr::_update_atomic(l);
					Ptr::_update_atomic(r);
					if (_is_root()) {
						l = _ptr.exchange(r, m == std::memory_order_release ? std::memory_order_acq_rel : m);
						if (l != r) {
							Ptr::_set(l, r, false);
						}
					} else {
						_ptr.store(r, m);
					}
				}
			}
		}

		ptr _exchange(const ptr& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			if (this != &p) {
				T* l = _ptr.load(std::memory_order_acquire);
				T* r = p._ptr.load(std::memory_order_relaxed);
				Ptr::_update_atomic(l);
				Ptr::_update_atomic(r);
				l = _ptr.exchange(r, m);
				if (l != r) {
					Ptr::_set(l, r, false);
				}
				return ptr(l);
			}
			return *this;
		}

		bool _compare_exchange_weak(ptr& o, const ptr& n, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(o, n, [m](auto& p, auto& l, auto r){ return p.compare_exchange_weak(l, r, m); });
		}

		bool _compare_exchange_weak(ptr& o, const ptr& n, std::memory_order s, std::memory_order f) noexcept {
			return _compare_exchange(o, n, [s, f](auto& p, auto& l, auto r){ return p.compare_exchange_weak(l, r, s, f); });
		}

		bool _compare_exchange_strong(ptr& o, const ptr& n, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(o, n, [m](auto& p, auto& l, auto r){ return p.compare_exchange_strong(l, r, m); });
		}

		bool _compare_exchange_strong(ptr& o, const ptr& n, std::memory_order s, std::memory_order f) noexcept {
			return _compare_exchange(o, n, [s, f](auto& p, auto& l, auto r){ return p.compare_exchange_strong(l, r, s, f); });
		}

		template<class F>
		bool _compare_exchange(ptr& o, const ptr& n, F f) noexcept {
			if (this != &n) {
				T* r = n._ptr.load(std::memory_order_relaxed);
				T* e = o._ptr.load(std::memory_order_relaxed);
				T* l = e;
				Ptr::_update_atomic(l);
				Ptr::_update_atomic(r);
				bool result = f(_ptr, l, r);
				if (result) {
					if (l != r) {
						Ptr::_set(l, r, false);
					}
				} else {
					o._ptr.store(l, std::memory_order_release);
					o.Ptr::_set(e, l);
				}
				return result;
			}
			return true;
		}

		static Priv::Object* _Clone(const Priv::Object* p) {
			if constexpr (std::is_copy_constructible_v<T>) {
				auto n = dynamic_cast<const T*>(p);
				return Priv::make<std::remove_const_t<T>>(*n);
			} else {
				std::ignore = p;
				assert(!"clone: no copy constructor");
				return nullptr;
			}
		}

		template<typename, typename> friend class ptr;
		template<typename, typename> friend class atomic_ptr;
		template<typename U> friend auto Priv::raw_ptr(const U&) noexcept;
		template<typename U> friend ptr<U> Priv::make_ptr(U*) noexcept;
		template<typename U, typename ...A> friend U* Priv::make(A&&...);
	};

	template<typename T, typename U>
	inline bool operator==(const ptr<T>& a, const ptr<U>& b) noexcept {
		return Priv::raw_ptr(a) == Priv::raw_ptr(b);
	}

	template<typename T>
	inline bool operator==(const ptr<T>& a, std::nullptr_t) noexcept {
		return !a;
	}

	template<typename T>
	inline bool operator==(std::nullptr_t, const ptr<T>& a) noexcept {
		return !a;
	}

	template<typename T, typename U>
	inline bool operator!=(const ptr<T>& a, const ptr<U>& b) noexcept {
		return !(a == b);
	}

	template<typename T>
	inline bool operator!=(const ptr<T>& a, std::nullptr_t) noexcept {
		return (bool)a;
	}

	template<typename T>
	inline bool operator!=(std::nullptr_t, const ptr<T>& a) noexcept {
		return (bool)a;
	}

	template<typename T, typename U>
	inline bool operator<(const ptr<T>& a, const ptr<U>& b) noexcept {
		using V = typename std::common_type<T*, U*>::type;
		return std::less<V>()(Priv::raw_ptr(a), Priv::raw_ptr(b));
	}

	template<typename T>
	inline bool operator<(const ptr<T>& a, std::nullptr_t) noexcept {
		return std::less<T*>()(Priv::raw_ptr(a), nullptr);
	}

	template<typename T>
	inline bool operator<(std::nullptr_t, const ptr<T>& a) noexcept {
		return std::less<T*>()(nullptr, Priv::raw_ptr(a));
	}

	template<typename T, typename U>
	inline bool operator<=(const ptr<T>& a, const ptr<U>& b) noexcept {
		return !(b < a);
	}

	template<typename T>
	inline bool operator<=(const ptr<T>& a, std::nullptr_t) noexcept {
		return !(nullptr < a);
	}

	template<typename T>
	inline bool operator<=(std::nullptr_t, const ptr<T>& a) noexcept {
		return !(a < nullptr);
	}

	template<typename T, typename U>
	inline bool operator>(const ptr<T>& a, const ptr<U>& b) noexcept {
		return (b < a);
	}

	template<typename T>
	inline bool operator>(const ptr<T>& a, std::nullptr_t) noexcept {
		return nullptr < a;
	}

	template<typename T>
	inline bool operator>(std::nullptr_t, const ptr<T>& a) noexcept {
		return a < nullptr;
	}

	template<typename T, typename U>
	inline bool operator>=(const ptr<T>& a, const ptr<U>& b) noexcept {
		return !(a < b);
	}

	template<typename T>
	inline bool operator>=(const ptr<T>& a, std::nullptr_t) noexcept {
		return !(a < nullptr);
	}

	template<typename T>
	inline bool operator>=(std::nullptr_t, const ptr<T>& a) noexcept {
		return !(nullptr < a);
	}

	template<typename T, typename U>
	inline ptr<T> static_pointer_cast(const ptr<U>& r) noexcept {
		return ptr<T>(static_cast<T*>(Priv::raw_ptr(r)));
	}

	template<typename T, typename U>
	inline ptr<T> const_pointer_cast(const ptr<U>& r) noexcept {
		return ptr<T>(const_cast<T*>(Priv::raw_ptr(r)));
	}

	template<typename T, typename U>
	inline ptr<T> dynamic_pointer_cast(const ptr<U>& r) noexcept {
		return ptr<T>(dynamic_cast<T*>(Priv::raw_ptr(r)));
	}

	template<typename T>
	std::ostream& operator<<(std::ostream& s, const ptr<T>& p) {
		s << Priv::raw_ptr(p);
		return s;
	}

	template<typename T = object, typename>
	class weak_ptr final : protected Priv::Weak_ptr {
	public:
		using value_type = T;

		constexpr weak_ptr() noexcept
		: _ptr(nullptr) {
		}

		weak_ptr(std::nullptr_t) noexcept
		: _ptr(nullptr) {
		}

		weak_ptr(T* p) noexcept
		: _ptr(p) {
			assert(!p || p->is_managed());
			_init(p);
		}

		weak_ptr(const weak_ptr& p) noexcept
		: _ptr(p._ptr) {
			_init(p._ptr);
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr(const weak_ptr<U>& p) noexcept
		: _ptr(p._ptr) {
			_init(p._ptr);
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr(const gc::ptr<U>& p) noexcept
			: _ptr(Priv::raw_ptr(p)) {
			_init(_ptr);
		}

		weak_ptr(weak_ptr&& p) noexcept
		: _ptr(p._ptr) {
			p._ptr = nullptr;
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr(weak_ptr<U>&& p) noexcept
		: _ptr(p._ptr) {
			p._ptr = nullptr;
		}

		weak_ptr& operator=(const weak_ptr& p) noexcept {
			if (this != &p) {
				_set(_ptr, p._ptr);
				_ptr = p._ptr;
			}
			return *this;
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr& operator=(const weak_ptr<U>& p) noexcept {
			_set(_ptr, p._ptr);
			_ptr = p._ptr;
			return *this;
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr& operator=(const gc::ptr<U>& p) noexcept {
			auto r = Priv::raw_ptr(p);
			_set(_ptr, r);
			_ptr = r;
			return *this;
		}

		weak_ptr& operator=(weak_ptr&& p) noexcept {
			if (this != &p) {
				_move(_ptr, p._ptr);
				_ptr = p._ptr;
				p._ptr = nullptr;
			}
			return *this;
		}

		template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		weak_ptr& operator=(weak_ptr<U>&& p) noexcept {
			_move(_ptr, p._ptr);
			_ptr = p._ptr;
			p._ptr = nullptr;
			return *this;
		}

		bool expired() const noexcept {
			if (_ptr && _expired(_ptr)) {
				_remove(_ptr);
				_ptr = nullptr;
			}
			return _ptr == nullptr;
		}

		gc::ptr<T> lock() const noexcept {
			if (_try_lock(_ptr)) {
				return gc::ptr<T>(_ptr, nullptr);
			}
			_remove(_ptr);
			_ptr = nullptr;
			return {nullptr};
		}

		void reset() {
			_remove(_ptr);
		}

		void swap(weak_ptr& p) noexcept {
			std::swap(_ptr, p._ptr);
		}

	private:
		mutable T* _ptr;

		weak_ptr(T* p, std::nullptr_t) noexcept
		: _ptr(p) {
		}

		template<typename, typename> friend class atomic_weak_ptr;
	};

	template<typename T = object, typename>
	class atomic_ptr {
	public:
		using value_type = typename ptr<T>::value_type;

		atomic_ptr() = default;
		atomic_ptr(const atomic_ptr&) = delete;

		atomic_ptr(const ptr<T>& p) noexcept
		: _ptr(p) {
		}

		atomic_ptr(ptr<T>&& p) noexcept
			: _ptr(std::move(p)) {
		}

		atomic_ptr operator=(const atomic_ptr&) = delete;

		void operator=(const ptr<T>& p) noexcept {
			store(p);
		}

		bool is_lock_free() const noexcept {
			return _ptr._ptr.is_lock_free();
		}

		operator ptr<T>() const noexcept {
			return load();
		}

		ptr<T> load(std::memory_order m = std::memory_order_seq_cst) const noexcept {
			return _ptr._load(m);
		}

		void store(const ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			_ptr._store(p, m);
		}

		ptr<T> exchange(const ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _ptr._exchange(p, m);
		}

		bool compare_exchange_weak(ptr<T>& e, const ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _ptr._compare_exchange_weak(e, p, m);
		}

		bool compare_exchange_weak(ptr<T>& e, const ptr<T>& p, std::memory_order s, std::memory_order f) noexcept {
			return _ptr._compare_exchange_weak(e, p, s, f);
		}

		bool compare_exchange_strong(ptr<T>& e, const ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _ptr._compare_exchange_strong(e, p, m);
		}

		bool compare_exchange_strong(ptr<T>& e, const ptr<T>& p, std::memory_order s, std::memory_order f) noexcept {
			return _ptr._compare_exchange_strong(e, p, s, f);
		}

	private:
		ptr<T> _ptr;
	};

	template<typename T = object, typename>
	class atomic_weak_ptr {
	public:
		using value_type = T;

		atomic_weak_ptr() noexcept
		: _ptr(nullptr) {
		}

		atomic_weak_ptr(const atomic_weak_ptr&) = delete;

		atomic_weak_ptr(const weak_ptr<T>& p) noexcept
		: _ptr(p._ptr) {
			weak_ptr<T>::_init(p._ptr);
		}

		atomic_weak_ptr(weak_ptr<T>&& p) noexcept
		: _ptr(std::move(p._ptr)) {
			p._ptr = nullptr;
		}

		atomic_weak_ptr operator=(const atomic_weak_ptr&) = delete;

		void operator=(const weak_ptr<T>& p) noexcept {
			store(p);
		}

		bool is_lock_free() const noexcept {
			return _ptr.is_lock_free();
		}

		operator weak_ptr<T>() const noexcept {
			return load();
		}

		weak_ptr<T> load(std::memory_order m = std::memory_order_seq_cst) const noexcept {
			return weak_ptr<T>(_ptr._load(m));
		}

		void store(const weak_ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			auto l = _ptr.load(std::memory_order_acquire);
			if (l != p._ptr) {
				auto l = _ptr.exchange(p._ptr, m == std::memory_order_release ? std::memory_order_acq_rel : m);
				if (l != p._ptr) {
					weak_ptr<T>::_set(l, p._ptr);
				}
			}
		}

		weak_ptr<T> exchange(const weak_ptr<T>& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			auto l = _ptr.exchange(p._ptr, m);
			weak_ptr<T>::_init(p._ptr);
			return weak_ptr<T>(l, nullptr);
		}

		bool _compare_exchange_weak(weak_ptr<T>& o, const weak_ptr<T>& n, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(o, n, [m](auto& p, auto& l, auto r){ return p.compare_exchange_weak(l, r, m); });
		}

		bool _compare_exchange_weak(weak_ptr<T>& o, const weak_ptr<T>& n, std::memory_order s, std::memory_order f) noexcept {
			return _compare_exchange(o, n, [s, f](auto& p, auto& l, auto r){ return p.compare_exchange_weak(l, r, s, f); });
		}

		bool _compare_exchange_strong(weak_ptr<T>& o, const weak_ptr<T>& n, std::memory_order m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(o, n, [m](auto& p, auto& l, auto r){ return p.compare_exchange_strong(l, r, m); });
		}

		bool _compare_exchange_strong(weak_ptr<T>& o, const weak_ptr<T>& n, std::memory_order s, std::memory_order f) noexcept {
			return _compare_exchange(o, n, [s, f](auto& p, auto& l, auto r){ return p.compare_exchange_strong(l, r, s, f); });
		}

	private:
		std::atomic<T*> _ptr;

		template<class F>
		bool _compare_exchange(weak_ptr<T>& o, const weak_ptr<T>& n, F f) noexcept {
			T* r = n._ptr;
			T* e = o._ptr;
			T* l = e;
			bool result = f(_ptr, l, r);
			if (result) {
				if (l != r) {
					weak_ptr<T>::Ptr::_set(l, r);
				}
			} else {
				o._ptr = l;
				weak_ptr<T>::_set(e, l);
			}
			return result;
		}
	};

	template<typename T, typename ...A>
	std::enable_if_t<std::is_convertible_v<T*, const object*>, ptr<T>> make(A&&... a) {
		return Priv::make_ptr(Priv::make<T>(std::forward<A>(a)...));
	}
} // namespace gc

#endif // SGCL_H
