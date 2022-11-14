/*******************************************************************************
** SGCL - Concurrent Garbage Collector
** Copyright (c) 2022 Sebastian Nibisz
** Distributed under the MIT License.
*******************************************************************************/
#ifndef SGCL_H
#define SGCL_H

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>

#define SGCL_MAX_POINTER_TYPES 1024
#define SGCL_MAX_SLEEP_TIME_SEC 10
#define SGCL_TRIGER_PERCENTAGE 25
#define SGCL_LOG_PRINT_LEVEL 1

#if SGCL_LOG_PRINT_LEVEL
#include <iostream>
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

		class Object;
		class Ptr;
		class Weak_ptr;

		static constexpr int8_t ValueAtomicUpdate = std::numeric_limits<int8_t>::max();
		static constexpr int8_t ValueExpired = std::numeric_limits<int8_t>::min();

		struct Metadata {
			struct Static {
				size_t object_count;
				size_t object_size;
				std::pair<Metadata*(*)(void*), size_t>* pointer_offsets;
			};

			virtual ~Metadata() = default;
			virtual void destroy() const noexcept {}
			virtual Object* clone(const Object*) {
				return nullptr;
			}
			virtual Static static_metadata() const noexcept {
				return {0, 0, nullptr};
			}

			std::atomic<Metadata*> next_allocated_object = {nullptr};
			Metadata* prev_object = {nullptr};
			Metadata* next_object = {nullptr};
			//std::atomic_uint ref_counter = {0};
			std::atomic_uint weak_ref_counter = {0};
			std::atomic_int8_t update_counter = {1};
			std::atomic_bool next_object_can_registered = {true};
			bool can_remove_object = {false};
			bool is_reachable = {true};
			bool is_container = {false};
			bool is_registered = {false};
		};

		//class Collector;
		//struct Memory;
		struct Thread_data;

		class Object {
		protected:
			Object() = default;
			virtual ~Object() = default;

			Metadata* metadata = nullptr;

			friend struct Priv;
			template<typename, typename> friend class ptr;
		};

		static Thread_data& local_thread_data() {
			static thread_local Thread_data data_instance;
			return data_instance;
		}

		struct Stack {
			static constexpr int BlockSize = 256;
			using Block = std::array<std::atomic<void*>, BlockSize>;

			struct Block_node {
				Metadata*(*metadata)(void*) = {nullptr};
				Block_node* next = {nullptr};
				Block data = {nullptr};
			};

			struct Indexes {
				Indexes() : _position(size()) {
				}
				Indexes(Block& block) : _position(0) {
					for(int i = 0; i < size(); ++i) {
						_indexes[i] = &block[i];
					}
				}
				constexpr int size() const noexcept {
					return (int)_indexes.size();
				}
				bool is_empty() const noexcept {
					return _position == size();
				}
				bool is_full() const noexcept {
					return _position == 0;
				}
				std::atomic<void*>* alloc() noexcept {
					return _indexes[_position++];
				}
				void free(std::atomic<void*>* p) noexcept {
					p->store(nullptr, std::memory_order_relaxed);
					_indexes[--_position] = p;
				}

				Indexes* next = nullptr;

			private:
				std::array<std::atomic<void*>*, BlockSize> _indexes;
				int _position;
			};

			Stack() {
				_indexes = new std::atomic<Indexes*>[SGCL_MAX_POINTER_TYPES]{nullptr};
				_empty_indexes = new std::atomic<Indexes*>[SGCL_MAX_POINTER_TYPES]{nullptr};
			}

			template<class T>
			static Metadata* metadata_getter(void* p) {
				return static_cast<T*>(p)->metadata;
			}

			template<class T>
			std::atomic<void*>* alloc() {
				auto type_index = Stack::Type<T>::index;
				auto& indexes = _local_indexes[type_index];
				auto [idx1, idx2] = indexes;
				if (idx1 && !idx1->is_empty()) {
					return idx1->alloc();
				}
				if (idx2 && !idx2->is_empty()) {
					indexes = {idx2, idx1};
					return idx2->alloc();
				}
				auto new_idx = _indexes[type_index].load(std::memory_order_acquire);
				while(new_idx && !_indexes[type_index].compare_exchange_weak(new_idx, new_idx->next, std::memory_order_release, std::memory_order_acquire));
				if (!new_idx) {
					auto block = new Block_node{metadata_getter<T>};
					new_idx = new Indexes(block->data);
					block->next = blocks.load(std::memory_order_acquire);
					while(!blocks.compare_exchange_weak(block->next, block, std::memory_order_release, std::memory_order_relaxed));
				}
				if (idx1) {
					if (idx2) {
						idx2->next = _empty_indexes[type_index].load(std::memory_order_acquire);
						while(!_empty_indexes[type_index].compare_exchange_weak(idx2->next, idx2, std::memory_order_release, std::memory_order_relaxed));
					}
					idx2 = idx1;
				}
				idx1 = new_idx;
				indexes = {idx1, idx2};
				return idx1->alloc();
			}
			template<class T>
			void free(std::atomic<void*>* p) noexcept {
				auto type_index = Stack::Type<T>::index;
				auto& indexes = _local_indexes[type_index];
				auto [idx1, idx2] = indexes;
				if (idx1 && !idx1->is_full()) {
					idx1->free(p);
					return;
				}
				if (idx2 && !idx2->is_full()) {
					idx2->free(p);
					indexes = {idx2, idx1};
					return;
				}
				auto new_idx = _empty_indexes[type_index].load(std::memory_order_acquire);
				while(new_idx && !_empty_indexes[type_index].compare_exchange_weak(new_idx, new_idx->next, std::memory_order_release, std::memory_order_acquire));
				if (!new_idx) {
					new_idx = new Indexes();
				}
				if (idx1) {
					if (idx2) {
						idx2->next = _indexes[type_index].load(std::memory_order_acquire);
						while(!_indexes[type_index].compare_exchange_weak(idx2->next, idx2, std::memory_order_release, std::memory_order_relaxed));
					}
					idx2 = idx1;
				}
				idx1 = new_idx;
				indexes = {idx1, idx2};
				idx1->free(p);
			}

			std::atomic<Block_node*> blocks = {nullptr};

		private:
			std::atomic<Indexes*>* _indexes;
			std::atomic<Indexes*>* _empty_indexes;

			struct Local_indexes {
				Local_indexes() : _data(nullptr) {}
				~Local_indexes() {
					if (_data) {
						for (int i = 0; i < SGCL_MAX_POINTER_TYPES; ++i) {
							for(auto idx : _data[i]) {
								if (idx) {
									auto& indexes = idx->is_empty() ? stack._empty_indexes[i] : stack._indexes[i];
									idx->next = indexes.load(std::memory_order_acquire);
									while(!indexes.compare_exchange_weak(idx->next, idx, std::memory_order_release, std::memory_order_relaxed));
								}
							}
						}
						delete[] _data;
					}
				}
				std::array<Indexes*, 2>& operator[](int type_index) {
					assert(type_index < SGCL_MAX_POINTER_TYPES);
					if (!_data) {
						_data = new std::array<Indexes*, 2>[SGCL_MAX_POINTER_TYPES]{std::array<Indexes*, 2>{nullptr, nullptr}};
					}
					return _data[type_index];
				}
			private:
				std::array<Indexes*, 2>* _data;
			};

			inline static thread_local Local_indexes _local_indexes;
			inline static std::atomic<int> _type_index = {0};

		public:
			template<class T>
			struct Type {
				inline static int index = _type_index++;
			};
		};

		inline static Stack stack;

		struct Memory {
			struct alloc_state {
				std::pair<Ptr*, Ptr*> range;
				size_t size;
				std::pair<Metadata*(*)(void*), std::atomic<void*>*>* ptrs;
			};

			static alloc_state& local_alloc_state() {
				static thread_local alloc_state _local_alloc_state = {{nullptr, nullptr}, 0, nullptr};
				return _local_alloc_state;
			}

			static void register_object(Metadata* p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_object->next_allocated_object.store(p, std::memory_order_release);
				thread_data.last_allocated_object = p;
			}

			static void register_container(Metadata* p) noexcept {
				auto& thread_data = local_thread_data();
				thread_data.last_allocated_container->next_allocated_object.store(p, std::memory_order_release);
				thread_data.last_allocated_container = p;
			}

			static Metadata* try_lock_last_ptr() noexcept {
				auto& thread_data = local_thread_data();
				auto last_ptr = thread_data.last_allocated_object;
				if (last_ptr->next_object_can_registered.load(std::memory_order_relaxed)) {
					last_ptr->next_object_can_registered.store(false, std::memory_order_release);
					return last_ptr;
				}
				return nullptr;
			}

			static void unlock_last_ptr(Metadata* p) noexcept {
				if (p) {
					p->next_object_can_registered.store(true, std::memory_order_release);
				}
			}
		};

		struct Thread_data {
			struct local_list : Metadata {
				explicit local_list(Metadata* p = nullptr) noexcept
				: first_ptr(p) {
					is_reachable = false;
					can_remove_object = true;
					if (p) {
						p->is_reachable = false;
					}
				}
				std::atomic<bool> is_used = {true};
				Metadata* first_ptr;
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
			: last_allocated_object(new Metadata)
			, last_allocated_container(new Metadata)
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
				_local_list->is_used.store(false, std::memory_order_release);
			}

			volatile bool init = {false};
			Metadata* last_allocated_object;
			Metadata* last_allocated_container;
			inline static global_list list = {};

		private:
			local_list* const _local_list;
		};

		class Ptr {
		public:
			template<class T>
			Ptr(T*) noexcept {
				auto& state = Memory::local_alloc_state();
				bool root = !(this >= state.range.first && this < state.range.second);
				if (!root && state.ptrs) {
					state.ptrs[state.size++] = {Stack::metadata_getter<T>, &__ptr};
				}
				_ref = root ? stack.alloc<T>() : &__ptr;
			}

			Ptr(const Ptr&) = delete;
			Ptr& operator=(const Ptr&) = delete;

		protected:
			template<class T>
			void _free_ref() {
				if (_ref != &__ptr) {
					stack.free<T>(_ref);
				}
			}

			static void _update(const Object* p) noexcept {
				if (p) {
					p->metadata->update_counter.store(1, std::memory_order_release);
				}
			}

			Object* _clone(const Object* ptr) const {
				return ptr->metadata->clone(ptr);
			}

			std::atomic<void*>& _ptr() noexcept {
				return *_ref;
			}

			const std::atomic<void*>& _ptr() const noexcept {
				return *_ref;
			}

		private:
			std::atomic<void*>* _ref;
			std::atomic<void*> __ptr;
		};

		class Weak_ptr {
		protected:
			inline static void _init(Object* p) noexcept {
				if (p) {
					p->metadata->weak_ref_counter.fetch_add(1, std::memory_order_release);
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
					p->metadata->weak_ref_counter.fetch_sub(1, std::memory_order_release);
				}
			}

			inline static bool _expired(Object* p) noexcept {
				return p->metadata->update_counter.load(std::memory_order_acquire) == ValueExpired;
			}

			inline static bool _try_lock(Object* p) noexcept {
				/*
				if (p) {
					p->metadata->ref_counter.fetch_add(1, std::memory_order_release);
					auto counter = p->metadata->update_counter.load(std::memory_order_acquire);
					while(!counter &&
								!p->metadata->update_counter.compare_exchange_weak(counter, 1, std::memory_order_release, std::memory_order_relaxed));
					return counter != ValueExpired;
				}
				*/
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
				void push(Metadata* p) noexcept {
					if (!first) {
						first = last = p;
					} else {
						last->next_object = p;
						p->prev_object = last;
						last = p;
					}
				}

				void push(Metadata* f, Metadata* l) noexcept {
					if (f) {
						if (!first) {
							first = f;
						} else {
							last->next_object = f;
							f->prev_object = last;
						}
						last = l;
					}
				}

				void pop(Metadata* p) noexcept {
					auto prev = p->prev_object;
					auto next = p->next_object;
					if (prev) {
						prev->next_object = next;
						p->prev_object = nullptr;
					}
					if (next) {
						next->prev_object = prev;
						p->next_object = nullptr;
					}
					if (first == p) {
						first = next;
					}
					if (last == p) {
						last = prev;
					}
				}
				Metadata* first = {nullptr};
				Metadata* last = {nullptr};
			};

			/*
			void _mark_root_objects() noexcept {
				static auto clock = std::chrono::steady_clock::now();
				//static int counter = 0;
				static double rest = 0;
				auto c = std::chrono::steady_clock::now();
				double duration = std::chrono::duration<double, std::milli>(c - clock).count();
				duration = ValueAtomicUpdate * duration / 100 + rest; // 100ms for ValueAtomicUpdate
				int iduration = (int)duration;
				int grain = std::min((int)ValueAtomicUpdate, iduration);
				if (iduration >= 1) {
					clock = c;
					rest = duration - iduration;
				}

				bool abort = _abort.load(std::memory_order_acquire);
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->next_object;
					int new_counter = 0;
					int counter = ptr->update_counter.load(std::memory_order_acquire);
					if (counter > 0) {
						new_counter = !abort ? counter == 1 || counter == ValueAtomicUpdate ? counter - 1 : counter - std::min(counter, grain) : 0;
						if (new_counter != counter) {
							ptr->update_counter.store(new_counter, std::memory_order_release);
						}
					}
					auto ref_counter = ptr->ref_counter.load(std::memory_order_acquire);
					if (ref_counter > 0 || new_counter > 0) {
						ptr->is_reachable = true;
						_objects.pop(ptr);
						_reachable_objects.push(ptr);
					} else {
						ptr->is_reachable = false;
					}
					ptr = next;
				}
			}
			*/

			void _mark_root_objects() noexcept {
				static auto clock = std::chrono::steady_clock::now();
				static double rest = 0;
				auto c = std::chrono::steady_clock::now();
				double duration = std::chrono::duration<double, std::milli>(c - clock).count();
				duration = ValueAtomicUpdate * duration / 100 + rest; // 100ms for ValueAtomicUpdate
				int iduration = (int)duration;
				int grain = std::min((int)ValueAtomicUpdate, iduration);
				if (iduration >= 1) {
					clock = c;
					rest = duration - iduration;
				}

				bool abort = _abort.load(std::memory_order_acquire);
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->next_object;
					int new_counter = 0;
					int counter = ptr->update_counter.load(std::memory_order_acquire);
					if (counter > 0) {
						new_counter = !abort ? counter == 1 || counter == ValueAtomicUpdate ? counter - 1 : counter - std::min(counter, grain) : 0;
						if (new_counter != counter) {
							ptr->update_counter.store(new_counter, std::memory_order_release);
						}
					}
					if (new_counter > 0) {
						ptr->is_reachable = true;
						_objects.pop(ptr);
						_reachable_objects.push(ptr);
					} else {
						ptr->is_reachable = false;
					}
					ptr = next;
				}

				auto block = stack.blocks.load(std::memory_order_acquire);
				while(block) {
					for (auto& child : block->data) {
						auto object = child.load(std::memory_order_acquire);
						if (object) {
							auto ptr = block->metadata(object);
							if (!ptr->is_reachable) {
								ptr->is_reachable = true;
								_objects.pop(ptr);
								_reachable_objects.push(ptr);
							}
						}
					}
					block = block->next;
				}
			}

			void _mark_local_allocated_objects(Thread_data::local_list* l) {
				bool list_is_used = l->is_used.load(std::memory_order_acquire);
				Metadata* next = l->first_ptr;
				while(next) {
					Metadata* ptr = l->first_ptr;
					if (!ptr->is_registered) {
						(ptr->is_reachable ? _reachable_objects : _objects).push(ptr);
						ptr->is_registered = true;
						++_last_allocated_objects_number;
					}
					if (ptr->next_object_can_registered.load(std::memory_order_acquire)) {
						next = ptr->next_allocated_object;
						if (next || !list_is_used) {
							ptr->can_remove_object = true;
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

			void _mark_child_objects(Metadata*& p) noexcept {
				auto ptr = p ? p->next_object : _reachable_objects.first;
				while(ptr) {
					auto data = ptr->static_metadata();
					for (size_t i = 0; i < data.object_count; ++i) {
						auto offset = data.pointer_offsets;
						while(offset->second) {
							auto child_ptr = (std::atomic<void*>*)((size_t)ptr + data.object_size * i + offset->second);
							auto object = child_ptr->load(std::memory_order_acquire);
							if (object) {
								auto ptr = offset->first(object);
								if (!ptr->is_reachable) {
									ptr->is_reachable = true;
									_objects.pop(ptr);
									_reachable_objects.push(ptr);
								}
							}
							++offset;
						}
					}
					ptr = ptr->next_object;
				}
				p = _reachable_objects.last;
			}

			bool _mark_updated_objects() noexcept {
				auto last_processed_object = _reachable_objects.last;
				auto ptr = _objects.first;
				while(ptr) {
					auto next = ptr->next_object;
					assert(!ptr->is_reachable);
					if (ptr->update_counter.load(std::memory_order_acquire)) {
						ptr->is_reachable = true;
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
					auto next = ptr->next_object;
					if (ptr->weak_ref_counter.load(std::memory_order_acquire)) {
						auto counter = ptr->update_counter.load(std::memory_order_acquire);
						while(!counter &&
									!ptr->update_counter.compare_exchange_weak(counter, ValueExpired, std::memory_order_release, std::memory_order_relaxed));
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

			bool _try_delete(Metadata* p) noexcept {
				p->next_object = nullptr;
				p->prev_object = nullptr;
				if (p->can_remove_object && p->weak_ref_counter.load(std::memory_order_acquire) == 0) {
					assert(p->update_counter.load(std::memory_order_acquire) == 0 ||
								 p->update_counter.load(std::memory_order_acquire) == ValueExpired);
					if (!_abort.load(std::memory_order_acquire)) {
						delete p;
					}
					return true;
				}
				return false;
			}

			void _remove_garbage() noexcept {
				object_list tmp_list;
				auto ptr = _garbage_objects.first;
				while(ptr) {
					auto next = ptr->next_object;
					if (!_try_delete(ptr)) {
						tmp_list.push(ptr);
					}
					ptr = next;
				}
				_garbage_objects = tmp_list;
			}

			size_t _sweep(object_list objects) noexcept {
				size_t destroyed_objects_number = 0;
				object_list tmp_list;
				auto ptr = objects.first;
				while(ptr) {
					auto next = ptr->next_object;
					auto data = ptr->static_metadata();
					for (size_t i = 0; i < data.object_count; ++i) {
						auto offset = data.pointer_offsets;
						while(offset->second) {
							auto child_ptr = (std::atomic<void*>*)((size_t)ptr + data.object_size * i + offset->second);
							child_ptr->store(nullptr, std::memory_order_relaxed);
							++offset;
						}
					}
					ptr->destroy();
					if (!_try_delete(ptr)) {
						tmp_list.push(ptr);
					}
					++destroyed_objects_number;
					ptr = next;
				}
				_garbage_objects.push(tmp_list.first, tmp_list.last);
				return destroyed_objects_number;
			}

			void _main_loop() noexcept {
#if SGCL_LOG_PRINT_LEVEL
				std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
				using namespace std::chrono_literals;
				std::thread destroyer;
				int finalization_counter = 2;
				do {
					_mark_root_objects();

					_last_allocated_objects_number = 0;
					int i = 0;
					do {
						_mark_allocated_objects();
						if (std::max(_last_allocated_objects_number, _last_destroyed_objects_number.load(std::memory_order_acquire)) * 100 / SGCL_TRIGER_PERCENTAGE >= _live_objects_number + 4000 ||
								_abort.load(std::memory_order_acquire)) {
							break;
						}
						std::this_thread::sleep_for(1ms);
					} while(++i < SGCL_MAX_SLEEP_TIME_SEC * 1000);

					Metadata* last_processed_object = nullptr;
					do {
						_mark_child_objects(last_processed_object);
					}
					while(_mark_updated_objects());
					bool garbage = _objects.first != _objects.last;
					_move_objects_to_unreachable();
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
					destroyer = std::thread([this, objects = _unreachable_objects]{
						_remove_garbage();
						auto destroyed_objects_number = _sweep(objects);
						_last_destroyed_objects_number.store(destroyed_objects_number, std::memory_order_release);
					});
					_move_reachable_to_objects();

					if (!_abort.load(std::memory_order_acquire)) {
						std::this_thread::yield();
					}

					if (_abort.load(std::memory_order_acquire) && !garbage) {
						--finalization_counter;
					}
				} while(finalization_counter);

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
			std::atomic<size_t> _last_destroyed_objects_number = {0};
			size_t _live_objects_number = {0};
		};

		inline static Collector collector_instance;

		template<typename T>
		static ptr<T> make_ptr(T* p) noexcept {
			return ptr<T>(p, nullptr);
		}

		template<class T>
		struct Type_data {
			inline static std::atomic<std::pair<Metadata*(*)(void*), size_t>*> pointer_offsets = nullptr;
			struct Metadata : Priv::Metadata {
				Metadata(const T* p) : ptr(p) {}
				Object* clone(const Object* p) override {
					if constexpr (std::is_copy_constructible_v<T>) {
						auto n = dynamic_cast<const T*>(p);
						return make<T>(*n);
					} else {
						std::ignore = p;
						assert(!"clone: no copy constructor");
						return nullptr;
					}
				}
				void destroy() const noexcept override {
					ptr->~T();
				}
				Static static_metadata() const noexcept override {
					return {1, sizeof(T), pointer_offsets.load(std::memory_order_acquire)};
				}
				const T* ptr;
			};
		};

		template<typename T, typename ...A>
		static T* make(A&&... a) {
			using Type = Type_data<T>;
			using M = typename Type::Metadata;

			std::pair<Metadata*(*)(void*), std::atomic<void*>*>* ptrs = nullptr;
			if (!Type::pointer_offsets.load(std::memory_order_acquire)) {
				ptrs = static_cast<std::pair<Metadata*(*)(void*), std::atomic<void*>*>*>(::operator new(sizeof(T)));
			}

			auto last_ptr = Memory::try_lock_last_ptr();
			auto mem = static_cast<Ptr*>(::operator new(sizeof(M) + sizeof(T)));

			auto& state = Memory::local_alloc_state();
			auto old_state = state;
			state = {{mem, mem + (sizeof(M) + sizeof(T)) / sizeof(Ptr*)}, 0, ptrs};

			T* obj;
			M* meta;
			try {
				obj = new((M*)mem + 1) T(std::forward<A>(a)...);
				meta = new(mem) M(obj);
			}
			catch (...) {
				::operator delete(mem);
				if (ptrs) {
					::operator delete(ptrs);
				}
				state = old_state;
				Memory::unlock_last_ptr(last_ptr);
				throw;
			}

			if (ptrs) {
				auto new_offset = new std::pair<Metadata*(*)(void*), size_t>[state.size + 1];
				for (size_t i = 0; i < state.size; ++i) {
					new_offset[i] = {ptrs[i].first, (size_t)ptrs[i].second - (size_t)static_cast<Metadata*>(meta)};
				}
				new_offset[state.size].second = 0;
				::operator delete(ptrs);
				std::pair<Metadata*(*)(void*), size_t>* o = nullptr;
				if (!Type::pointer_offsets.compare_exchange_strong(o, new_offset)) {
					::operator delete(new_offset);
				}
			}

			obj->metadata = meta;

			state = old_state;

			Memory::register_object(meta);
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

	public:
		bool is_managed() const noexcept {
			return metadata != nullptr;
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
		: Ptr((T*)(0)) {
			_ptr().store(nullptr, std::memory_order_relaxed);
		}

		constexpr ptr(std::nullptr_t) noexcept
		: Ptr((T*)(0)) {
			_ptr().store(nullptr, std::memory_order_relaxed);
		}

		explicit ptr(T* p) noexcept
		: Ptr((T*)(0)) {
			_ptr().store(p && p->is_managed() ? (std::remove_cv_t<T>*)p : nullptr, std::memory_order_relaxed);
			assert(!p || p->is_managed());
		}

		ptr(const ptr& p) noexcept
		: Ptr((T*)(0)) {
			_init(p, std::memory_order_relaxed);
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr(const ptr<U>& p) noexcept
		: Ptr((T*)(0)) {
			_init(p, std::memory_order_relaxed);
		}

		ptr(ptr&& p) noexcept
		: Ptr((T*)(0)) {
			_init_move(std::move(p));
		}

		template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
		ptr(ptr<U>&& p) noexcept
		: Ptr((T*)(0)) {
			_init_move(std::move(p));
		}

		~ptr() noexcept {
			_free_ref<T>();
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
				T* r = (T*)p._ptr().load(std::memory_order_relaxed);
				T* l = (T*)_ptr().load(std::memory_order_relaxed);
				if (l != r) {
					p._ptr().store(l, std::memory_order_relaxed);
					_ptr().store(r, std::memory_order_relaxed);
					_update(l);
					_update(r);
				}
			}
		}

	private:
		// for make
		explicit ptr(T* p, std::nullptr_t) noexcept
		: Ptr((T*)(0)) {
			_ptr().store(p, std::memory_order_relaxed);
			_update(p);
		}

		ptr(const ptr& p, std::memory_order m) noexcept
		: Ptr((T*)(0)) {
			_init(p, m);
		}

		template<typename U>
		void _init(const ptr<U>& p, std::memory_order m) noexcept {
			T* rp = (U*)p._ptr().load(m);
			_ptr().store(rp, std::memory_order_relaxed);
			_update(rp);
		}

		template<typename U>
		void _init_move(ptr<U>&& p) noexcept {
			T* rp = (U*)p._ptr().load(std::memory_order_relaxed);
			_ptr().store(rp, std::memory_order_relaxed);
			p._ptr().store(nullptr, std::memory_order_relaxed);
			_update(rp);
		}

		void _set(std::nullptr_t) noexcept {
			T* p = (T*)_ptr().load(std::memory_order_relaxed);
			if (p != nullptr) {
				_ptr().store(nullptr, std::memory_order_relaxed);
			}
		}

		template<typename U>
		void _set(const ptr<U>& p) {
			T* r = (U*)p._ptr().load(std::memory_order_relaxed);
			T* l = (T*)_ptr().load(std::memory_order_relaxed);
			if (l != r) {
				_ptr().store(r, std::memory_order_relaxed);
				_update(r);
			}
		}

		template<typename U>
		void _move(ptr<U>&& p) noexcept {
			T* r = (U*)p._ptr().load(std::memory_order_relaxed);
			T* l = (T*)_ptr().load(std::memory_order_relaxed);
			if (l != r) {
				_ptr().store(r, std::memory_order_relaxed);
				_update(r);
			}
			p._ptr().store(nullptr, std::memory_order_relaxed);
		}

		T* _get() const noexcept {
			return (T*)_ptr().load(std::memory_order_relaxed);
		}

		ptr _load(std::memory_order m = std::memory_order_seq_cst) const noexcept {
			return ptr(*this, m);
		}

		void _store(const ptr& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			if (this != &p) {
				T* r = (T*)p._ptr().load(std::memory_order_relaxed);
				_ptr().store(r, m);
				_update(r);
			}
		}

		ptr _exchange(const ptr& p, std::memory_order m = std::memory_order_seq_cst) noexcept {
			if (this != &p) {
				T* r = (T*)p._ptr().load(std::memory_order_relaxed);
				T* l = (T*)_ptr().exchange(r, m);
				_update(r);
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
				T* r = (T*)n._ptr().load(std::memory_order_relaxed);
				T* l = (T*)o._ptr().load(std::memory_order_relaxed);
				bool result = f(_ptr(), l, r);
				if (result) {
					_update(r);
				} else {
					o._ptr().store(l, std::memory_order_relaxed);
					_update(l);
				}
				return result;
			}
			return true;
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
