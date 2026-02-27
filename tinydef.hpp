#pragma once

/* tinydef.hpp - sandwall
 * ======================
 * This is a header that defines shorthand names for common primitives, useful math constants/functions,
 * simple data structures, as well as memory abstraction data structures.
 */

#if defined(_WIN32)
#define USING_WIN32
#elif defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define USING_UNIX
#endif

#include <stdint.h>
#include <assert.h>

// i'm not so keen on these includes, will hopefully find a way to get rid of it later
#include <string.h> // for memset
#include <math.h>   // for exp and expf

#define TINY_BEGIN_NAMESPACE(name) namespace name {
#define TINY_END_NAMESPACE }

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using c8 = char;
using c32 = char32_t;

// TIM = TIny Math
namespace tim {
	constexpr f32 pi = 3.1415926535f;
	constexpr f32 tau = pi * 2.0f;

	constexpr i32 wrap_around(i32 x, i32 max) {
		if (x >= max) return x % max;
		if (x < 0) return max + x - 1;
		return x;
	}

	template <typename T>
	constexpr T min(T a, T b) {
		if (a < b) return a;
		return b;
	}

	template <typename T>
	constexpr T max(T a, T b) {
		if (a > b) return a;
		return b;
	}

	template <typename T>
	constexpr T clamp(T x, T min, T max) {
		if (x >= max) return max;
		if (x <= min) return min;
		return x;
	}

	// This function extends clamp to strictly keep a value in between a range
	// doesn't matter the order of the range arguments
	template <typename T>
	constexpr T between(T x, T side1, T side2) {
		if (side1 == side2) return side1;
		return (side2 < side1) ?
			clamp(x, side2, side1) :
			clamp(x, side1, side2);
	}

	template<typename T>
	constexpr T abs(T x) {
		return x > 0 ? x : -x;
	}

	// takes a position and a length, and just returns an index into a circular buffer
	template<typename T>
	constexpr T circ_idx(T i, T len) {
		T result = i % len;
		return result < 0 ? result + len : result;
	}

	// Frame-independent lerp smoothing - Decay is recommended to be [1,25] from "slow to fast"
	// thx freya holmer https://youtu.be/LSNQuFEDOyQ?si=mk9QMjab57lxyNkM&t=2978
	inline f32 filerpf(f32 current, f32 target, f32 decay, f32 dt) {
		return target + (current - target) * expf(-decay * dt);
	}

	inline f64 filerp(f64 current, f64 target, f64 decay, f64 dt) {
		return target + (current - target) * exp(-decay * dt);
	}
}

// TDS = Tiny Data Structures
namespace tds {
	// this ministruct allows treating data as a circular buffer
	template<typename T>
	struct RingSlice {
		T* data;
		size_t len;

		T& operator[](i64 i) {
			return data[tim::circ_idx(i, len)];
		}
	};

	// this ministruct allows treating data as a T array
	template<typename T>
	struct Slice {
		T* data;
		size_t len;

		T& operator[](i64 i) {
			assert(i >= 0 && i < static_cast<i64>(len));
			return data[i];
		}
	};

	struct StringSlice : public Slice<char> {
		// checks if the start of the string is equal to some other string
		bool starts_with(const char* other) {
			size_t otherLen = strlen(other);
			if (len < otherLen) return false;

			for (int i = 0; i < otherLen; i++) {
				if (data[i] != other[i]) return false;
			}

			return true;
		}

		// basically shifts the start of the string forwards by n characters
		void eat_first(size_t n) {
			size_t actualN = tim::min(n, len);

			data += actualN;
			len -= actualN;
		}

		operator char*() { return data; }
	};

	template<u32 NumBits>
	struct BitSet {
		static constexpr u32 SIZE = ((NumBits + 7u) & ~(7u)) / 8;
		u8 data[SIZE];

		void reset() {
			memset(data, 0, SIZE);
		}

		void set(u32 i, bool value) {
			u32 idx = i / 8;
			u8 mask = 1u << (i % 8);
			
			if (value) data[idx] |= mask;
			else data[idx] &= ~mask;
		}

		bool get(u32 i) const {
			return data[i / 8] & (1 << (i % 8));
		}

		// I'm implementing this data structure to only be able to get bits with an operater overload
		// as setting bits with an operator overload requires making a type that is supposed to get returned as a reference
		// and that reference's constructor will do the settings... a bit complex, so i'll just leave it a regular function
		bool operator[](u32 i) const { return get(i); }
	};

	template<typename T>
	struct LinkedList {
		struct Node {
			Node* next;
			T data;
		};

		Node start;
	};

	// simple range struct
	template<typename T>
	struct Range {
		T start;
		T count;
	};

	template<u32 Capacity, typename T>
	struct Stack {
		static constexpr u32 CAPACITY = Capacity;
		
		u32 size;
		T data[CAPACITY];

		void reset() {
			memset(data, 0, sizeof(T) * CAPACITY);
			size = 0;
		}

		bool push(T t) {
			assert(size < CAPACITY);
			data[size++] = t;
		}

		T pop() {
			assert(size > 0);
			return data[size--];
		}

		T peek(u32 pos) {
			if (size <= 0) return 0;
			return data[size - 1];
		}
	};

	//template<typename T>
	//struct Stack {
	//};

	template<u32 NumStates>
	struct StateMachine {
		static constexpr u32 maxStates = NumStates;
		using StateFunction = void(*)(StateMachine<NumStates>&);

		u32 prevState = 0;
		u32 state = 0;
		u32 nextState = 0;

		// These can be set to true in case we want to tell the state machine
		// to call the enter_state or exit_state functions again for some reason
		bool signalEnter = false;
		bool signalExit = false;

		// [i][0] = enter_state
		// [i][1] = update_state
		// [i][2] = exit_state
		StateFunction stateTable[NumStates][3];
		StateFunction onEnter = nullptr;  // global state enter function
		StateFunction onExit = nullptr;   // global state exit function

#define ASSERT_STATE_VALIDITY \
	assert(state < maxStates); \
	assert(nextState < maxStates);

		void update() {
			// enter_state is called
			ASSERT_STATE_VALIDITY
			if (nextState != state || signalEnter) {
				StateFunction& sf = stateTable[nextState][0];
				if (sf) sf(*this);
				if (onEnter) onEnter(*this);

				signalEnter = false;
				prevState = state;
				state = nextState;
			}

			// update_state (this is expected to change states by setting nextState)
			ASSERT_STATE_VALIDITY
			{
				StateFunction &sf = stateTable[state][1];
				if (sf) sf(*this);
			}

			// exit_state
			// for some reason this isn't getting called when the state is being changed by imgui
			// (it could be possible that it's not just being ignored for statechanges triggered by imgui)
			ASSERT_STATE_VALIDITY
			if (nextState != state || signalExit) {
				StateFunction& sf = stateTable[state][2];
				if (sf) sf(*this);
				if (onExit) onExit(*this);

				signalExit = false;
			}
		}
	};
}

// Memory utilities

namespace mem {
	void init();
	void close();
	struct Arena& get_scratch();

	// Linear allocator used to group together allocations
	// On Memory Arenas:
	// https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
	struct Arena {
#define push_struct(ptr, struc) push_data(ptr, sizeof(struc))
		void alloc(u64 cap = 100000000LL); // 100 megabytes
		void dealloc();

		void clear();
		void clear_decommit();
		void* peek();
		void* push(size_t len);
		void* push_data(void* pData, size_t sizeData);
		void* push_zero(size_t len);
		void pop(size_t len);
		void pop_to(size_t newPos);
		
		template <typename T>
		T* push() { return (T*)push(sizeof(T)); }
		template <typename T>
		T* push_zero() { return (T*)push_zero(sizeof(T)); }

		void* data;
		size_t pos;
		size_t capacity;
	};

	// Helper struct meant to automatically handle temporary allocations
	// Constructor records the current pos of the arena, and the destructor pops to that old pos
	struct ArenaScope {
		ArenaScope(Arena& a, bool automatic = true);
		~ArenaScope();

		void release();

	private:
		size_t startPos;
		Arena& arena;

		bool releaseOnDestruct;
	};

}

#ifdef TINYDEF_IMPLEMENTATION

#include <assert.h>

#if defined(USING_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(USING_UNIX)
#error Memory abstractions not implemented for Unix-type systems!
#else
#error Memory abstractions not implemented for this platform!
#endif

//
// MEMORY ABSTRACTION IMPLEMENTATION
//

namespace mem {

	// 4096 is a common page size, but for it to actually be accurate,
	// we need to set this using information obtained from the OS in mem::init()
	u64 pageSize = 4096;
	u64 round_to_page_size(u64 size) {
		u64 rem = size % pageSize;
		size -= rem;
		return size + pageSize;
	}

#if defined(USING_WIN32)
	inline u64 get_page_size() {
		SYSTEM_INFO si = { 0 };
		GetSystemInfo(&si);
		return si.dwPageSize;
	}

	inline void* _reserve(size_t cap) {
		// maybe round_to_page_size should be used here?
		return VirtualAlloc(nullptr, cap, MEM_RESERVE, PAGE_READWRITE);
	}

	inline void* _commit(void* start, size_t size) {
		return VirtualAlloc(start, size, MEM_COMMIT, PAGE_READWRITE);
	}

	inline bool _release(void* region) {
		return VirtualFree(region, 0, MEM_RELEASE);
	}

#pragma warning(disable : 6250)
	inline bool _decommit(void* region, size_t size) {
		return VirtualFree(region, size, MEM_DECOMMIT);
	}

#elif defined(USING_UNIX)
	inline u64 get_page_size() {
		return 4096;
	}

	inline void* _reserve(size_t cap) {
		return nullptr;
	}

	inline void* _commit(void* start, size_t size) {
		return nullptr;
	}

	inline bool _release(void* region) {
		return false;
	}

	inline bool _decommit(void* region, size_t size) {
		return false;
	}

#endif

	//
	// MEMORY STRUCTURES IMPLEMENTATION
	// NOTE: Platform specific code is above
	//

	Arena scratchArena;

	void init() {
		pageSize = get_page_size();
		scratchArena.alloc();
	}

	void close() {
		scratchArena.dealloc();
	}

	Arena& get_scratch() {
		// we're not clearing here anymore since anyone that calls this function
		// might not specifically want to clear the arena unnecessarily

		// scratchArena.clear();
		return scratchArena;
	}

	void Arena::alloc(u64 cap) {
		// we want to reserve a good spot between "too little" and "holy balls that's too much" memory
		// when we encounter the problem of this actually being too little memory,
		// we can probably either raise this size, or start chaining Arenas
		capacity = round_to_page_size(cap);
		data = mem::_reserve(capacity);

		// We'll commit the first page of memory, so that we can initially make use of it
		mem::_commit(data, pageSize);
	}

	void Arena::dealloc() {
		// added this, not sure if it's really required
		clear_decommit();
		// we can do profiling and testing for that

		capacity = 0;
		data = nullptr;

		mem::_release(data);
	}

	// This function is called "peek", and while it might make sense for it to be called that
	// It's important to remember that the context of this function is to obtain a memory address
	// to the next spot that can be allocated at. It's useful if you want to initialize an array
	// by appending to it (this is a less hacky way of doing "data = Arena::push(1); Arena::pop();")
	void* Arena::peek() {
		return static_cast<u8*>(data) + pos;
	}

	// Returns a pointer to len bytes of memory
	void* Arena::push(size_t len) {
		assert(pos + len < capacity);
		_commit(static_cast<u8*>(data) + pos, len);
		u64 prev = pos;
		pos += len;
		return static_cast<u8*>(data) + prev;
	}

	// Copies pData into the arena and returns a pointer to it
	void* Arena::push_data(void* pData, size_t sizeData) {
		assert(pos + sizeData < capacity);
		_commit(static_cast<u8*>(data) + pos, sizeData);
		u64 prev = pos;
		memcpy(static_cast<u8*>(data) + pos, pData, sizeData);
		pos += sizeData;
		return static_cast<u8*>(data) + prev;
	}

	// Returns a pointer to len zero-initialized bytes
	void* Arena::push_zero(size_t len) {
		assert(pos + len < capacity);
		_commit(static_cast<u8*>(data) + pos, len);
		u64 prev = pos;
		memset(static_cast<u8*>(data) + pos, 0, len);
		pos += len;
		return static_cast<u8*>(data) + prev;
	}

	// Undoes the most recent len bytes of allocation
	void Arena::pop(size_t len) {
		if (len > pos) pos = 0;
		else pos -= len;
	}

	// Instead of deallocating the top x bytes in the arena,
	// we "cut" the allocated bytes to newPos
	void Arena::pop_to(size_t newPos) {
		if (newPos > pos) return;
		pos = newPos;
	}

	void Arena::clear() {
		pos = 0;
	}

	// Decommits all memory except the first page
	void Arena::clear_decommit() {
		_decommit(static_cast<u8*>(data) + pageSize, pos - pageSize);
		clear();
	}

	/* ArenaScope is handy, and the following example might illustrate why:
	*
	*	void bingus(i32 someNumber, Arena& arena) {
	*		ArenaScope arenaScope(arena);
	*		u8* bytes = arena.push(someNumber);
	*
	*		// do something with bytes...
	*
	*		// now, at the end of the scope, arenaScope.~ArenaScope() runs and bytes is freed!
	*	}
	*/
	ArenaScope::ArenaScope(Arena& a, bool automatic)
		: arena(a), releaseOnDestruct(automatic) {
		startPos = a.pos;
	}

	ArenaScope::~ArenaScope() {
		if(releaseOnDestruct)
			release();
	}

	void ArenaScope::release() {
		arena.pop_to(startPos);
	}

}

#endif
