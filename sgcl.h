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

#define GC_MAX_SLEEP_TIME_SEC 30
#define GC_TRIGER_PERCENTAGE 25

#ifdef __MINGW32__
#define GC_MAX_STACK_OFFSET 1024
#endif

namespace gc {
	class collected;
	class object;

	template<typename _T, typename = std::enable_if_t<std::is_convertible_v<_T*, const object*>>>
	class ptr;

	template<typename _Type>
	class ptr<_Type[]>;

	template<typename _Type, size_t _N>
	class ptr<_Type[_N]>;

	template<typename _T, typename = std::enable_if_t<std::is_convertible_v<_T*, const object*>>>
	class weak_ptr;

	template<typename _Type>
	class weak_ptr<_Type[]>;

	template<typename _Type, size_t _N>
	class weak_ptr<_Type[_N]>;

	template<typename _T, typename = std::enable_if_t<std::is_convertible_v<_T*, const object*>>>
	class atomic_ptr;

	template<typename _Type>
	class atomic_ptr<_Type[]>;

	template<typename _Type, size_t _N>
	class atomic_ptr<_Type[_N]>;

	template<typename _T, typename = std::enable_if_t<std::is_convertible_v<_T*, const object*>>>
	class atomic_weak_ptr;

	template<typename _Type>
	class atomic_weak_ptr<_Type[]>;

	template<typename _Type, size_t _N>
	class atomic_weak_ptr<_Type[_N]>;

	template<typename _T, typename ..._A>
	std::enable_if_t<std::is_convertible_v<_T*, const object*>, ptr<_T>> make(_A&&...);

	struct _Priv {
		_Priv() = delete;

		class _Ptr;
		class _Weak_ptr;
		class _Object;
		class _Memory_block;

		struct _Object_metadata {
			std::atomic<_Object*> next_allocated_object = {nullptr};
			_Object* prev_object = {nullptr};
			_Object* next_object = {nullptr};
			union {
				_Ptr* first_ptr = {nullptr};
				std::atomic<_Memory_block*> first_memory_block;
			};
			union {
				_Object* (*vget)(void*);
				_Object* (*clone)(const _Object*);
				void* _raw_ptr;
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

		class _Collector;
		struct _Memory;
		struct _Thread_data;
		class _Object {
		protected:
			_Object() = default;
			_Object(const _Object&) noexcept {}

			virtual ~_Object() = default;

			_Object& operator=(const _Object&) noexcept {
				return *this;
			}

			union {
				mutable _Object_metadata metadata = {};
			};

			friend struct _Priv;
			template<typename _T, typename> friend class ptr;
		};

		static _Thread_data& local_thread_data() {
			static thread_local _Thread_data _local_thread_data;
			return _local_thread_data;
		}

		class _Memory_block {
		public:
			struct data {
				_Object* get() const noexcept {
					return ptr.load(std::memory_order_relaxed);
				}

				_Object* vget() const noexcept {
					return ptr.load(std::memory_order_acquire);
				}

				void set(_Object* __p) noexcept {
					ptr.store(__p, std::memory_order_release);
					if (__p && !__p->metadata.update_counter.load(std::memory_order_acquire)) {
						__p->metadata.update_counter.store(1, std::memory_order_release);
					}
				}

				std::atomic<_Object*> ptr = {nullptr};
				union {
					_Memory_block* memory_block;
					struct {
						unsigned offset;
						unsigned next_data;
					};
				};
			};

			_Memory_block(data* const __b, unsigned __s)
			: buffer(__b)
			, size(__s)
			, free_counter(__s) {
				for (unsigned i = 0; i < size; ++i) {
					buffer[i].offset = i;
					buffer[i].next_data = i + 1;
				}
			}

			virtual ~_Memory_block() = default;

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

			void free(data* __d) noexcept {
				__d->next_data = _free_cell;
				_free_cell = __d->offset;
				++free_counter;
				if (free_counter == size) {
					is_rechable.store(false, std::memory_order_release);
				}
			}

			bool realloc_request() const noexcept {
				return _blocked && free_counter >= size / 2;
			}

			_Memory_block* prev_block = {nullptr};
			std::atomic<_Memory_block*> next_block = {nullptr};
			data* const buffer;
			const unsigned size;
			unsigned free_counter;
			std::atomic<bool> is_rechable = {true};

		private:
			unsigned _free_cell = 0;
			bool _blocked = {false};
		};

		struct _Memory {
			struct alloc_state {
				std::pair<_Ptr*, _Ptr*> range;
				_Ptr* first_ptr;
				_Ptr* last_ptr;
			};

			static alloc_state& local_alloc_state() {
				static thread_local alloc_state _local_alloc_state = {{nullptr, nullptr}, nullptr, nullptr};
				return _local_alloc_state;
			}

			static void register_object(_Object* __p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_object->metadata.next_allocated_object.store(__p, std::memory_order_release);
				thread_data.last_allocated_object = __p;
			}

			static void register_container(_Object* __p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_container->metadata.next_allocated_object.store(__p, std::memory_order_release);
				thread_data.last_allocated_container = __p;
			}

			static _Object* try_lock_last_ptr() noexcept {
				auto& thread_data = local_thread_data();
				auto last_ptr = thread_data.last_allocated_object;
				if (last_ptr->metadata.next_object_can_registered.load(std::memory_order_relaxed)) {
					last_ptr->metadata.next_object_can_registered.store(false, std::memory_order_release);
					return last_ptr;
				}
				return nullptr;
			}

			static void unlock_last_ptr(_Object* __p) noexcept {
				if (__p) {
					__p->metadata.next_object_can_registered.store(true, std::memory_order_release);
				}
			}
		};

		struct _Thread_data {
			struct local_list : _Object {
				explicit local_list(_Object* __f = nullptr) noexcept
				: first_ptr(__f) {
					metadata.is_reachable = false;
					metadata.can_remove_object = true;
					if (__f) {
						__f->metadata.is_reachable = false;
					}
				}
				std::atomic<bool> is_reachable = {true};
				_Object* first_ptr;
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

			_Thread_data()
			: last_allocated_object(new _Object)
			, last_allocated_container(new _Object)
			, _local_list(new local_list {last_allocated_object}) {
				auto old_list = list.last_local_list.exchange(_local_list, std::memory_order_relaxed);
				old_list->next_local_list.store(_local_list, std::memory_order_release);
			}

			~_Thread_data() {
				_local_list->is_reachable.store(false, std::memory_order_release);
			}

			volatile bool init = {false};
			_Object* last_allocated_object;
			_Object* last_allocated_container;
			inline static global_list list = {};

		private:
			local_list* const _local_list;
		};

		class _Ptr {
		public:
			_Ptr() noexcept {
#if GC_MAX_STACK_OFFSET
				int local = 0;
				size_t this_addr = reinterpret_cast<size_t>(this);
				size_t local_addr = reinterpret_cast<size_t>(&local);
				size_t offset = local_addr > this_addr ? local_addr - this_addr : this_addr - local_addr;

				if (offset > GC_MAX_STACK_OFFSET) {
#endif
					auto& state = _Memory::local_alloc_state();
					bool root = !(this >= state.range.first && this < state.range.second);
					this->_type = root ? _Ptr_type::root : _Ptr_type::undefined;
					if (!root) {
						if (!state.last_ptr){
							state.first_ptr = state.last_ptr = this;
						} else {
							state.last_ptr = state.last_ptr->_next_ptr = this;
						}
					}
#if GC_MAX_STACK_OFFSET
				} else {
					this->_type = _Ptr_type::root;
				}
#endif
//				if (this->_type == _Ptr_type::root) {
//					static thread_local _Ptr** stack = new _Ptr*[1024];
//				}
			}

			_Ptr(const _Ptr&) = delete;
			virtual ~_Ptr() = default;
			_Ptr& operator=(const _Ptr&) = delete;

			_Ptr* next_ptr() const noexcept {
				return _next_ptr;
			}

			virtual const _Object* vget() const noexcept = 0;
			virtual void vreset() noexcept = 0;

		protected:
			bool _is_root() const noexcept {
				return _type == _Ptr_type::root;
			}

			static void _update_counter(const _Object* __p, int __v) noexcept {
				if (__p) {
					__p->metadata.ref_counter.fetch_add(__v, std::memory_order_release);
				}
			}

			static void _update(const _Object* __p) noexcept {
				if (__p && !__p->metadata.update_counter.load(std::memory_order_acquire)) {
					__p->metadata.update_counter.store(1, std::memory_order_release);
				}
			}

			static void _update_atomic(const _Object* __p) noexcept {
				if (__p && __p->metadata.update_counter.load(std::memory_order_acquire) < _Object_metadata::atomic_update_value) {
					__p->metadata.update_counter.store(_Object_metadata::atomic_update_value, std::memory_order_release);
				}
			}

			void _create(const _Object* __p) const noexcept {
				_update(__p);
				if (!_is_root()) {
					__p->metadata.ref_counter.store(0, std::memory_order_release);
				}
			}

			void _init(const _Object* __p) const noexcept {
				if (_is_root()) {
					_update_counter(__p, 1);
				}
				_update(__p);
			}

			void _init_move(const _Object* __p, bool __p_is_root) const noexcept {
				if (__p) {
					if (_is_root() && !__p_is_root) {
						_update_counter(__p, 1);
					} else if (!_is_root() && __p_is_root) {
						_update_counter(__p, -1);
					}
				}
				_update(__p);
			}

			void _set(const _Object* __l, const _Object* __r, bool __u = true) const noexcept {
				if (_is_root()) {
					_update_counter(__l, -1);
					_update_counter(__r, 1);
				}
				if (__u) {
					_update(__r);
				}
			}

			void _set_move(const _Object* __l, const _Object* __r, bool __p_is_root) const noexcept {
				if (_is_root()) {
					_update_counter(__l, -1);
				}
				_init_move(__r, __p_is_root);
			}

			void _remove(const _Object* __p) const noexcept {
				if (_is_root()) {
					_update_counter(__p, -1);
				}
			}

			_Object* _clone(const _Object* __ptr) const {
				return __ptr->metadata.clone(__ptr);
			}

		private:
			enum class _Ptr_type : size_t {
				undefined = 0,
				root = 1
			};

			union {
				_Ptr* _next_ptr;
				_Ptr_type _type;
			};
		};

		class _Weak_ptr {
		protected:
			inline static void _init(_Object* __p) noexcept {
				if (__p) {
					__p->metadata.weak_ref_counter.fetch_add(1, std::memory_order_release);
				}
			}

			inline static void _set(_Object* __l, _Object* __r) noexcept {
				if (__l != __r) {
					_remove(__l);
					_init(__r);
				}
			}

			inline static void _move(_Object* __l, _Object* __r) noexcept {
				if (__l != __r) {
					_remove(__l);
				}
			}

			inline static void _remove(_Object* __p) noexcept {
				if (__p) {
					__p->metadata.weak_ref_counter.fetch_sub(1, std::memory_order_release);
				}
			}

			inline static bool _expired(_Object* __p) noexcept {
				return !__p->metadata.weak_ref_counter.load(std::memory_order_acquire);
			}

			inline static bool _try_lock(_Object* __p) noexcept {
				if (__p) {
					__p->metadata.ref_counter.fetch_add(1, std::memory_order_release);
					auto counter = __p->metadata.update_counter.load(std::memory_order_acquire);
					while(!counter &&
								!__p->metadata.update_counter.compare_exchange_weak(counter, 1, std::memory_order_release, std::memory_order_relaxed));
					return counter != _Object_metadata::lock_update_value;
				}
				return false;
			}
		};

		class _Collector {
		public:
			_Collector() {
				_thread = std::thread([this]{_main_loop();});
			}

			~_Collector() {
				_abort.store(true, std::memory_order_release);
				_thread.join();
			}

		private:
			struct object_list {
				void push(_Object* __p) noexcept {
					if (!first) {
						first = last = __p;
					} else {
						last->metadata.next_object = __p;
						__p->metadata.prev_object = last;
						last = __p;
					}
				}

				void push(_Object* __f, _Object* __l) noexcept {
					if (__f) {
						if (!first) {
							first = __f;
						} else {
							last->metadata.next_object = __f;
							__f->metadata.prev_object = last;
						}
						last = __l;
					}
				}

				void pop(_Object* __p) noexcept {
					auto prev = __p->metadata.prev_object;
					auto next = __p->metadata.next_object;
					if (prev) {
						prev->metadata.next_object = next;
						__p->metadata.prev_object = nullptr;
					}
					if (next) {
						next->metadata.prev_object = prev;
						__p->metadata.next_object = nullptr;
					}
					if (first == __p) {
						first = next;
					}
					if (last == __p) {
						last = prev;
					}
				}
				_Object* first = {nullptr};
				_Object* last = {nullptr};
			};

			void _mark_root_objects() noexcept {
				static auto clock = std::chrono::high_resolution_clock::now();
				//static int counter = 0;
				static double rest = 0;
				auto c = std::chrono::high_resolution_clock::now();
				double duration = std::chrono::duration<double, std::milli>(c - clock).count();
				duration = _Object_metadata::atomic_update_value * duration / 100 + rest; // 100ms for atomic_update_value
				int iduration = (int)duration;
				int grain = std::min((int)_Object_metadata::atomic_update_value, iduration);
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
						new_counter = !abort ? counter == 1 || counter == _Object_metadata::atomic_update_value ? counter - 1 : counter - std::min(counter, grain) : 0;
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

			void _mark_local_allocated_objects(_Thread_data::local_list* __list) {
				bool list_is_unreachable = !__list->is_reachable.load(std::memory_order_acquire);
				_Object* next = __list->first_ptr;
				while(next) {
					_Object* ptr = __list->first_ptr;
					if (!ptr->metadata.is_registered) {
						(ptr->metadata.is_reachable ? _reachable_objects : _objects).push(ptr);
						ptr->metadata.is_registered = true;
						++_last_allocated_objects_number;
					}
					if (ptr->metadata.next_object_can_registered) {
						next = ptr->metadata.next_allocated_object;
						if (next || list_is_unreachable) {
							ptr->metadata.can_remove_object = true;
							__list->first_ptr = next;
						}
					} else {
						next = nullptr;
					}
				}
			}

			void _mark_allocated_objects() noexcept {
				_Thread_data::local_list* prev_list = nullptr;
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

			void _mark_child_objects(_Object*& __p) noexcept {
				auto ptr = __p ? __p->metadata.next_object : _reachable_objects.first;
				while(ptr) {
					auto child_ptr = ptr->metadata.first_ptr;
					while(child_ptr) {
						if (auto p = const_cast<_Object*>(child_ptr->vget())) {
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
				__p = _reachable_objects.last;
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
									!ptr->metadata.update_counter.compare_exchange_weak(counter, _Object_metadata::lock_update_value, std::memory_order_release, std::memory_order_relaxed));
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

			bool _try_delete(_Object* ptr) noexcept {
				ptr->metadata.next_object = nullptr;
				ptr->metadata.prev_object = nullptr;
				if (ptr->metadata.can_remove_object && ptr->metadata.weak_ref_counter.load(std::memory_order_acquire) == 0) {
					assert(ptr->metadata.update_counter.load(std::memory_order_acquire) == 0 ||
								 ptr->metadata.update_counter.load(std::memory_order_acquire) == _Object_metadata::lock_update_value);
					auto _raw_ptr = ptr->metadata._raw_ptr;
					ptr->metadata.~_Object_metadata();
					if (!_abort.load(std::memory_order_acquire)) {
						::operator delete(_raw_ptr);
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
					ptr->metadata._raw_ptr = dynamic_cast<void*>(ptr);
					ptr->metadata.is_managed = false;
					ptr->~_Object();
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
				using namespace std::chrono_literals;
				std::thread destroyer;
				int finalizeation_counter = 2;
				do {
					_mark_root_objects();

					_last_allocated_objects_number = 0;
					int i = 0;
					do {
						_mark_allocated_objects();
						if (_last_allocated_objects_number * 100 / GC_TRIGER_PERCENTAGE >= _live_objects_number + 4000 ||
								_last_destroyed_objects_number * 100 / GC_TRIGER_PERCENTAGE >= _live_objects_number ||
								_abort.load(std::memory_order_acquire)) {
							break;
						}
						std::this_thread::sleep_for(1ms);
					} while(++i < GC_MAX_SLEEP_TIME_SEC * 1000);

					_Object* last_processed_object = nullptr;
					do {
						_mark_child_objects(last_processed_object);
					}
					while(_mark_updated_objects());
					if (destroyer.joinable()) {
						destroyer.join();
					}
					_live_objects_number += _last_allocated_objects_number;
					_live_objects_number -= _last_destroyed_objects_number;

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
			}

			std::thread _thread;
			std::atomic<bool> _abort = {false};
			_Thread_data::global_list& _allocated_objects = _Thread_data::list;
			object_list _objects;
			object_list _reachable_objects;
			object_list _unreachable_objects;
			object_list _garbage_objects;

			size_t _last_allocated_objects_number = {0};
			size_t _last_destroyed_objects_number = {0};
			size_t _live_objects_number = {0};
		};

		inline static _Collector _Collector_instance;

		template<typename _Type>
		static ptr<_Type> _Make_ptr(_Type* __ptr) noexcept {
			return ptr<_Type>(__ptr, nullptr);
		}

		template<typename _Type, typename ..._Args>
		static _Type* _Make(_Args&&... __args) {
			auto last_ptr = _Memory::try_lock_last_ptr();

			auto mem = static_cast<_Ptr*>(::operator new(sizeof(_Type)));

			auto& state = _Memory::local_alloc_state();
			auto old_state = state;
			state = {{mem, mem + sizeof(_Type) / sizeof(_Ptr*)}, nullptr, nullptr};

			_Type* obj;
			try {
				obj = new(mem) _Type(std::forward<_Args>(__args)...);
			}
			catch (...) {
				::operator delete(mem);
				state = old_state;
				_Memory::unlock_last_ptr(last_ptr);
				throw;
			}

			auto& metadata = obj->_metadata();
			metadata.first_ptr = state.first_ptr;
			metadata.clone = ptr<_Type>::_Clone;
			metadata.is_managed = true;
			metadata.ref_counter.store(1, std::memory_order_relaxed);

			state = old_state;

			_Memory::register_object(obj);
			_Memory::unlock_last_ptr(last_ptr);

			return obj;
		}

		friend class collected;
		friend class object;
		template<typename _T, typename> friend class ptr;
		template<typename _T, typename> friend class weak_ptr;
		template<typename _T, typename ..._A> friend std::enable_if_t<std::is_convertible_v<_T*, const object*>, ptr<_T>> gc::make(_A&&...);
		template<typename _T> friend struct std::less;

	public:
		template<typename _T>
		static auto _raw_ptr(const _T& __p) noexcept {
			return __p._get();
		}

		template<class _T>
		struct _Proxy_ptr {
			template<class ..._A>
			_Proxy_ptr(_A&& ...__a) : value(std::forward<_A>(__a)...) {}
			operator _T() const { return  value; }
			_T value;
		};
	}; // class _Priv

	class object : public _Priv::_Object {
		void operator&() = delete;
		void *operator new(size_t) = delete;
		void *operator new[](size_t) = delete;

		static void* operator new(std::size_t __count, void* __ptr) {
			return ::operator new(__count, __ptr);
		}

		_Priv::_Object_metadata& _metadata() noexcept {
			return metadata;
		}

	public:
		bool is_managed() const noexcept {
			return metadata.is_managed;
		}

		friend struct _Priv;
	};

	class collected : public virtual object {
	};

	template<typename _Type = object, typename>
	class ptr final : protected _Priv::_Ptr {
	public:
		using value_type = _Type;

		constexpr ptr() noexcept
		: _ptr(nullptr) {
		}

		constexpr ptr(std::nullptr_t) noexcept
		: _ptr(nullptr) {
		}

		explicit ptr(_Type* __p) noexcept
		: _ptr(__p && __p->is_managed() ? __p : nullptr) {
			assert(!__p || __p->is_managed());
			_Ptr::_init(__p);
		}

		ptr(const ptr& __p) noexcept
		: _Ptr() {
			_init(__p, std::memory_order_relaxed);
		}

		template<typename _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		ptr(const ptr<_Other_type>& __p) noexcept
		: _Ptr() {
			_init(__p, std::memory_order_relaxed);
		}

		ptr(ptr&& __p) noexcept
		: _Ptr() {
			_init_move(std::move(__p));
		}

		template<typename _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		ptr(ptr<_Other_type>&& __p) noexcept
		: _Ptr() {
			_init_move(std::move(__p));
		}

		~ptr() noexcept {
			reset();
		}

		ptr& operator=(std::nullptr_t) noexcept {
			_set(nullptr);
			return *this;
		}

		ptr& operator=(const ptr& __p) {
			if (this != &__p) {
				_set(__p);
			}
			return *this;
		}

		template<typename _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		ptr& operator=(const ptr<_Other_type>& __p) {
			_set(__p);
			return *this;
		}

		ptr& operator=(ptr&& __p) noexcept {
			if (this != &__p) {
				_move(std::move(__p));
			}
			return *this;
		}

		template<typename _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		ptr& operator=(ptr<_Other_type>&& __p) noexcept {
			_move(std::move(__p));
			return *this;
		}

		explicit operator bool() const noexcept {
			return (_get() != nullptr);
		}

		_Type* operator->() const noexcept {
			assert(_get() != nullptr);
			return _get();
		}

		_Type& operator*() noexcept {
			assert(_get() != nullptr);
			return *_get();
		}

		const _Type& operator*() const noexcept {
			assert(_get() != nullptr);
			return *_get();
		}

		ptr<_Type> clone() const {
			auto p = _get();
			return p ? _Priv::_Make_ptr(dynamic_cast<_Type*>(_clone(p))) : ptr<_Type>();
		}

		void reset() noexcept {
			_set(nullptr);
		}

		void swap(ptr& __p) noexcept {
			if (this != &__p) {
				_Type* r = __p._ptr.load(std::memory_order_relaxed);
				_Type* l = _ptr.load(std::memory_order_relaxed);
				if (l != r) {
					__p._ptr.store(l, std::memory_order_release);
					_ptr.store(r, std::memory_order_release);
					_Ptr::_init(r);
					__p._Ptr::_init(l);
					_Ptr::_remove(l);
					__p._Ptr::_remove(r);
				}
			}
		}

	private:
		std::atomic<_Type*> _ptr;

		// for make
		explicit ptr(_Type* __p, std::nullptr_t) noexcept
		: _ptr(__p) {
			_Ptr::_create(__p);
		}

		ptr(const ptr& __p, std::memory_order __m) noexcept
		: _Ptr() {
			_init(__p, __m);
		}

		template<typename _Other_type>
		void _init(const ptr<_Other_type>& __p, std::memory_order __m) noexcept {
			_Type* rp = __p._ptr.load(__m);
			_ptr.store(rp, std::memory_order_release);
			_Ptr::_init(rp);
		}

		template<typename _Other_type>
		void _init_move(ptr<_Other_type>&& __p) noexcept {
			_Type* rp = __p._ptr.load(std::memory_order_relaxed);
			_ptr.store(rp, std::memory_order_release);
			_Ptr::_init_move(rp, __p._is_root());
			__p._ptr.store(nullptr, std::memory_order_relaxed);
		}

		void _set(std::nullptr_t) noexcept {
			_Type* p = _ptr.load(std::memory_order_relaxed);
			if (p != nullptr) {
				_ptr.store(nullptr, std::memory_order_relaxed);
				_Ptr::_remove(p);
			}
		}

		template<typename _Other_type>
		void _set(const ptr<_Other_type>& __p) {
			_Type* r = __p._ptr.load(std::memory_order_relaxed);
			_Type* l = _ptr.load(std::memory_order_relaxed);
			if (l != r) {
				_ptr.store(r, std::memory_order_release);
				_Ptr::_set(l, r);
			}
		}

		template<typename _Other_type>
		void _move(ptr<_Other_type>&& __p) noexcept {
			_Type* r = __p._ptr.load(std::memory_order_relaxed);
			_Type* l = _ptr.load(std::memory_order_relaxed);
			if (l != r) {
				_ptr.store(r, std::memory_order_release);
				_Ptr::_set_move(l, r, __p._is_root());
			} else {
				__p._Ptr::_remove(r);
			}
			__p._ptr.store(nullptr, std::memory_order_relaxed);
		}

		_Type* _get() const noexcept {
			return _ptr.load(std::memory_order_relaxed);
		}

		const _Priv::_Object* vget() const noexcept override {
			return _ptr.load(std::memory_order_acquire);
		}

		void vreset() noexcept override {
			_ptr.store(nullptr, std::memory_order_relaxed);
		}

		ptr _load(std::memory_order __m = std::memory_order_seq_cst) const noexcept {
			return ptr(*this, __m);
		}

		void _store(const ptr& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			if (this != &__p) {
				_Type* l = _ptr.load(std::memory_order_acquire);
				_Type* r = __p._ptr.load(std::memory_order_relaxed);
				if (l != r) {
					_Ptr::_update_atomic(l);
					_Ptr::_update_atomic(r);
					if (_is_root()) {
						l = _ptr.exchange(r, __m == std::memory_order_release ? std::memory_order_acq_rel : __m);
						if (l != r) {
							_Ptr::_set(l, r, false);
						}
					} else {
						_ptr.store(r, __m);
					}
				}
			}
		}

		ptr _exchange(const ptr& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			if (this != &__p) {
				_Type* l = _ptr.load(std::memory_order_acquire);
				_Type* r = __p._ptr.load(std::memory_order_relaxed);
				_Ptr::_update_atomic(l);
				_Ptr::_update_atomic(r);
				l = _ptr.exchange(r, __m);
				if (l != r) {
					_Ptr::_set(l, r, false);
				}
				return ptr(l);
			}
			return *this;
		}

		bool _compare_exchange_weak(ptr& __e, const ptr& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(__e, __p, [__m](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_weak(__l, __r, __m); });
		}

		bool _compare_exchange_weak(ptr& __e, const ptr& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _compare_exchange(__e, __p, [__s, __f](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_weak(__l, __r, __s, __f); });
		}

		bool _compare_exchange_strong(ptr& __e, const ptr& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(__e, __p, [__m](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_strong(__l, __r, __m); });
		}

		bool _compare_exchange_strong(ptr& __e, const ptr& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _compare_exchange(__e, __p, [__s, __f](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_strong(__l, __r, __s, __f); });
		}

		template<class _F>
		bool _compare_exchange(ptr& __e, const ptr& __p, _F __compare_exchange) noexcept {
			if (this != &__p) {
				_Type* r = __p._ptr.load(std::memory_order_relaxed);
				_Type* e = __e._ptr.load(std::memory_order_relaxed);
				_Type* l = e;
				_Ptr::_update_atomic(l);
				_Ptr::_update_atomic(r);
				bool result = __compare_exchange(_ptr, l, r);
				if (result) {
					if (l != r) {
						_Ptr::_set(l, r, false);
					}
				} else {
					__e._ptr.store(l, std::memory_order_release);
					__e._Ptr::_set(e, l);
				}
				return result;
			}
			return true;
		}

		static _Priv::_Object* _Clone(const _Priv::_Object* __p) {
			if constexpr (std::is_copy_constructible_v<_Type>) {
				auto p = dynamic_cast<const _Type*>(__p);
				return _Priv::_Make<std::remove_const_t<_Type>>(*p);
			} else {
				std::ignore = __p;
				assert(!"clone: no copy constructor");
				return nullptr;
			}
		}

		template<typename _T, typename> friend class ptr;
		template<typename _T, typename> friend class atomic_ptr;
		template<typename _T> friend auto _Priv::_raw_ptr(const _T&) noexcept;
		template<typename _T> friend ptr<_T> _Priv::_Make_ptr(_T*) noexcept;
		template<typename _T, typename ..._Args> friend _T* _Priv::_Make(_Args&&...);
	};

	template<typename _T, typename _U>
	inline bool operator==(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		return _Priv::_raw_ptr(__a) == _Priv::_raw_ptr(__b);
	}

	template<typename _T>
	inline bool operator==(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return !__a;
	}

	template<typename _T>
	inline bool operator==(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return !__a;
	}

	template<typename _T, typename _U>
	inline bool operator!=(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		return !(__a == __b);
	}

	template<typename _T>
	inline bool operator!=(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return (bool)__a;
	}

	template<typename _T>
	inline bool operator!=(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return (bool)__a;
	}

	template<typename _T, typename _U>
	inline bool operator<(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		using V = typename std::common_type<_T*, _U*>::type;
		return std::less<V>()(_Priv::_raw_ptr(__a), _Priv::_raw_ptr(__b));
	}

	template<typename _T>
	inline bool operator<(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return std::less<_T*>()(_Priv::_raw_ptr(__a), nullptr);
	}

	template<typename _T>
	inline bool operator<(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return std::less<_T*>()(nullptr, _Priv::_raw_ptr(__a));
	}

	template<typename _T, typename _U>
	inline bool operator<=(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		return !(__b < __a);
	}

	template<typename _T>
	inline bool operator<=(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return !(nullptr < __a);
	}

	template<typename _T>
	inline bool operator<=(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return !(__a < nullptr);
	}

	template<typename _T, typename _U>
	inline bool operator>(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		return (__b < __a);
	}

	template<typename _T>
	inline bool operator>(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return nullptr < __a;
	}

	template<typename _T>
	inline bool operator>(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return __a < nullptr;
	}

	template<typename _T, typename _U>
	inline bool operator>=(const ptr<_T>& __a, const ptr<_U>& __b) noexcept {
		return !(__a < __b);
	}

	template<typename _T>
	inline bool operator>=(const ptr<_T>& __a, std::nullptr_t) noexcept {
		return !(__a < nullptr);
	}

	template<typename _T>
	inline bool operator>=(std::nullptr_t, const ptr<_T>& __a) noexcept {
		return !(nullptr < __a);
	}

	template<typename _T, typename _U>
	inline ptr<_T> static_pointer_cast(const ptr<_U>& __r) noexcept {
		return ptr<_T>(static_cast<_T*>(_Priv::_raw_ptr(__r)));
	}

	template<typename _T, typename _U>
	inline ptr<_T> const_pointer_cast(const ptr<_U>& __r) noexcept {
		return ptr<_T>(const_cast<_T*>(_Priv::_raw_ptr(__r)));
	}

	template<typename _T, typename _U>
	inline ptr<_T> dynamic_pointer_cast(const ptr<_U>& __r) noexcept {
		return ptr<_T>(dynamic_cast<_T*>(_Priv::_raw_ptr(__r)));
	}

	template<typename _T>
	std::ostream& operator<<(std::ostream& os, const ptr<_T>& p) {
		os << _Priv::_raw_ptr(p);
		return os;
	}

	template<typename _Type = object, typename>
	class weak_ptr final : protected _Priv::_Weak_ptr {
	public:
		using value_type = _Type;

		constexpr weak_ptr() noexcept
		: _ptr(nullptr) {
		}

		weak_ptr(std::nullptr_t) noexcept
		: _ptr(nullptr) {
		}

		weak_ptr(_Type* __p) noexcept
		: _ptr(__p) {
			assert(!__p || __p->is_managed());
			_init(__p);
		}

		weak_ptr(const weak_ptr& __p) noexcept
		: _ptr(__p._ptr) {
			_init(__p._ptr);
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr(const weak_ptr<_Other_type>& __p) noexcept
		: _ptr(__p._ptr) {
			_init(__p._ptr);
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr(const gc::ptr<_Other_type>& __p) noexcept
			: _ptr(_Priv::_raw_ptr(__p)) {
			_init(_ptr);
		}

		weak_ptr(weak_ptr&& __p) noexcept
		: _ptr(__p._ptr) {
			__p._ptr = nullptr;
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr(weak_ptr<_Other_type>&& __p) noexcept
		: _ptr(__p._ptr) {
			__p._ptr = nullptr;
		}

		weak_ptr& operator=(const weak_ptr& __p) noexcept {
			if (this != &__p) {
				_set(_ptr, __p._ptr);
				_ptr = __p._ptr;
			}
			return *this;
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr& operator=(const weak_ptr<_Other_type>& __p) noexcept {
			_set(_ptr, __p._ptr);
			_ptr = __p._ptr;
			return *this;
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr& operator=(const gc::ptr<_Other_type>& __p) noexcept {
			auto p = _Priv::_raw_ptr(__p);
			_set(_ptr, p);
			_ptr = p;
			return *this;
		}

		weak_ptr& operator=(weak_ptr&& __p) noexcept {
			if (this != &__p) {
				_move(_ptr, __p._ptr);
				_ptr = __p._ptr;
				__p._ptr = nullptr;
			}
			return *this;
		}

		template<class _Other_type, typename = std::enable_if_t<std::is_convertible_v<_Other_type*, _Type*>>>
		weak_ptr& operator=(weak_ptr<_Other_type>&& __p) noexcept {
			_move(_ptr, __p._ptr);
			_ptr = __p._ptr;
			__p._ptr = nullptr;
			return *this;
		}

		bool expired() const noexcept {
			if (_ptr && _expired(_ptr)) {
				_remove(_ptr);
				_ptr = nullptr;
			}
			return _ptr == nullptr;
		}

		gc::ptr<_Type> lock() const noexcept {
			if (_try_lock(_ptr)) {
				return gc::ptr<_Type>(_ptr, nullptr);
			}
			_remove(_ptr);
			_ptr = nullptr;
			return {nullptr};
		}

		void reset() {
			_remove(_ptr);
		}

		void swap(weak_ptr& __p) noexcept {
			std::swap(_ptr, __p._ptr);
		}

	private:
		mutable _Type* _ptr;

		weak_ptr(_Type* __p, std::nullptr_t) noexcept
		: _ptr(__p) {
		}

		template<typename _T, typename> friend class atomic_weak_ptr;
	};

	template<typename _Type = object, typename>
	class atomic_ptr {
	public:
		using value_type = typename ptr<_Type>::value_type;

		atomic_ptr() = default;
		atomic_ptr(const atomic_ptr& __p) = delete;

		atomic_ptr(const ptr<_Type>& __p) noexcept
		: _ptr(__p) {
		}

		atomic_ptr(ptr<_Type>&& __p) noexcept
			: _ptr(std::move(__p)) {
		}

		atomic_ptr operator=(const atomic_ptr& __p) = delete;

		void operator=(const ptr<_Type>& __p) noexcept {
			store(__p);
		}

		bool is_lock_free() const noexcept {
			return _ptr._ptr.is_lock_free();
		}

		operator ptr<_Type>() const noexcept {
			return load();
		}

		ptr<_Type> load(std::memory_order __m = std::memory_order_seq_cst) const noexcept {
			return _ptr._load(__m);
		}

		void store(const ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			_ptr._store(__p, __m);
		}

		ptr<_Type> exchange(const ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _ptr._exchange(__p, __m);
		}

		bool compare_exchange_weak(ptr<_Type>& __e, const ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _ptr._compare_exchange_weak(__e, __p, __m);
		}

		bool compare_exchange_weak(ptr<_Type>& __e, const ptr<_Type>& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _ptr._compare_exchange_weak(__e, __p, __s, __f);
		}

		bool compare_exchange_strong(ptr<_Type>& __e, const ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _ptr._compare_exchange_strong(__e, __p, __m);
		}

		bool compare_exchange_strong(ptr<_Type>& __e, const ptr<_Type>& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _ptr._compare_exchange_strong(__e, __p, __s, __f);
		}

	private:
		ptr<_Type> _ptr;
	};

	template<typename _Type = object, typename>
	class atomic_weak_ptr {
	public:
		using value_type = _Type;

		atomic_weak_ptr() noexcept
		: _ptr(nullptr) {
		}

		atomic_weak_ptr(const atomic_weak_ptr& __p) = delete;

		atomic_weak_ptr(const weak_ptr<_Type>& __p) noexcept
		: _ptr(__p._ptr) {
			weak_ptr<_Type>::_init(__p._ptr);
		}

		atomic_weak_ptr(weak_ptr<_Type>&& __p) noexcept
		: _ptr(std::move(__p._ptr)) {
			__p._ptr = nullptr;
		}

		atomic_weak_ptr operator=(const atomic_weak_ptr& __p) = delete;

		void operator=(const weak_ptr<_Type>& __p) noexcept {
			store(__p);
		}

		bool is_lock_free() const noexcept {
			return _ptr.is_lock_free();
		}

		operator weak_ptr<_Type>() const noexcept {
			return load();
		}

		weak_ptr<_Type> load(std::memory_order __m = std::memory_order_seq_cst) const noexcept {
			return weak_ptr<_Type>(_ptr._load(__m));
		}

		void store(const weak_ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			auto l = _ptr.load(std::memory_order_acquire);
			if (l != __p._ptr) {
				auto l = _ptr.exchange(__p._ptr, __m == std::memory_order_release ? std::memory_order_acq_rel : __m);
				if (l != __p._ptr) {
					weak_ptr<_Type>::_set(l, __p._ptr);
				}
			}
		}

		weak_ptr<_Type> exchange(const weak_ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			auto l = _ptr.exchange(__p._ptr, __m);
			weak_ptr<_Type>::_init(__p._ptr);
			return weak_ptr<_Type>(l, nullptr);
		}

		bool _compare_exchange_weak(weak_ptr<_Type>& __e, const weak_ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(__e, __p, [__m](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_weak(__l, __r, __m); });
		}

		bool _compare_exchange_weak(weak_ptr<_Type>& __e, const weak_ptr<_Type>& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _compare_exchange(__e, __p, [__s, __f](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_weak(__l, __r, __s, __f); });
		}

		bool _compare_exchange_strong(weak_ptr<_Type>& __e, const weak_ptr<_Type>& __p, std::memory_order __m = std::memory_order_seq_cst) noexcept {
			return _compare_exchange(__e, __p, [__m](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_strong(__l, __r, __m); });
		}

		bool _compare_exchange_strong(weak_ptr<_Type>& __e, const weak_ptr<_Type>& __p, std::memory_order __s, std::memory_order __f) noexcept {
			return _compare_exchange(__e, __p, [__s, __f](auto& __ptr, auto& __l, auto __r){ return __ptr.compare_exchange_strong(__l, __r, __s, __f); });
		}

	private:
		std::atomic<_Type*> _ptr;

		template<class _F>
		bool _compare_exchange(weak_ptr<_Type>& __e, const weak_ptr<_Type>& __p, _F __compare_exchange) noexcept {
			_Type* r = __p._ptr;
			_Type* e = __e._ptr;
			_Type* l = e;
			bool result = __compare_exchange(_ptr, l, r);
			if (result) {
				if (l != r) {
					weak_ptr<_Type>::_Ptr::_set(l, r);
				}
			} else {
				__e._ptr = l;
				weak_ptr<_Type>::_set(e, l);
			}
			return result;
		}
	};

	template<typename _Type, typename ..._Args>
	std::enable_if_t<std::is_convertible_v<_Type*, const object*>, ptr<_Type>> make(_Args&&... __args) {
		return _Priv::_Make_ptr(_Priv::_Make<_Type>(std::forward<_Args>(__args)...));
	}
} // namespace gc

#endif // SGCL_H
