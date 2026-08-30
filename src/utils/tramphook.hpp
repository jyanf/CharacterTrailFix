#pragma once
#include <Windows.h>
#include <list>
#include <algorithm>
#include <cstdint>
namespace {
// ============================================================
// option: #define TRAMPOLINE_POOL_NO_LOCK to disable thread stuff
// for explicit use in single thread and avoid mutex costs
// ============================================================
#ifdef TRAMPOLINE_POOL_NO_LOCK
struct NullMutex {
    void lock() noexcept {}
    void unlock() noexcept {}
    bool try_lock() noexcept { return true; }
};
using PoolMutex = NullMutex;
#else
#include <mutex>
using PoolMutex = std::mutex;
#endif

template <typename Mutex>
struct _LockGuard {
    Mutex& m;
    explicit _LockGuard(Mutex& mtx) : m(mtx) { m.lock(); }
    ~_LockGuard() { m.unlock(); }
    _LockGuard(const _LockGuard&) = delete;
    _LockGuard& operator=(const _LockGuard&) = delete;
};

class TrampolinePool {
    struct Page {
        static size_t getDefaultPageSize() {
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            return si.dwPageSize;
        }
        byte* base;
        size_t size;
        //std::vector<BlockInfo> blocks;
        //std::vector<size_t>    freeOffsets;
        size_t cursor;
        size_t allocationCount;
    };

    // block 16 bytes align
    static size_t alignSize(size_t size) {
        static constexpr size_t alignment = 16U;
        return (size + alignment-1) & ~(alignment-1U);
    }
    
    Page* newPage(size_t minSize) {
        const size_t size = ((minSize + defaultPageSize - 1) / defaultPageSize) * defaultPageSize;

        auto* base = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!base) return nullptr;

		pages.push_back(Page{ (decltype(Page::base))base, size, 0, 0 });
        auto it = std::prev(pages.end());
        current = it;
        return &*it;
    }
        
    const size_t defaultPageSize;
    std::list<Page> pages;
    decltype(pages)::iterator current = pages.end();
    PoolMutex mtx;
    using LockGuard = _LockGuard<PoolMutex>;
public:
    explicit TrampolinePool(size_t defaultPageSize = Page::getDefaultPageSize()) 
        : defaultPageSize(defaultPageSize) {
    }

    ~TrampolinePool() {
        LockGuard guard(mtx);
        for (auto& page : pages) {
            VirtualFree(page.base, 0, MEM_RELEASE);
        }
        pages.clear();
        current = pages.end();
    }

    TrampolinePool(const TrampolinePool&) = delete;
    TrampolinePool& operator=(const TrampolinePool&) = delete;

    void* allocate(size_t size) {
        LockGuard guard(mtx);
        size = alignSize(size);
        if (current != pages.end()) {
            Page& page = *current;
            if (page.cursor + size <= page.size) {
                void* result = page.base + page.cursor;
                page.cursor += size;
                ++page.allocationCount;
                return result;
            }
        }
        Page* page = newPage(size);
        if (!page) return nullptr;
        void* result = page->base;
        page->cursor = size;
        page->allocationCount = 1;
        return result;
    }

    /*void* allocateNear(void* target, size_t size) {
        return allocate(size);
    }*/

	void free(void* ptr) {
		if (!ptr) return;
		LockGuard guard(mtx);

		const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

		for (auto it = pages.begin(); it != pages.end(); ++it) {
			Page& page = *it;

			const uintptr_t begin = reinterpret_cast<uintptr_t>(page.base);
			const uintptr_t end = begin + page.cursor;

			// find page
			if (address < begin || address >= end)
				continue;
			if (!page.allocationCount)
				return;

			--page.allocationCount;

			if (page.allocationCount == 0) {
				VirtualFree(page.base, 0, MEM_RELEASE);
				if (it == current) {
					current = pages.end();
				}
				pages.erase(it);
			}
			return;
		}
	}

	void shrink() {
		LockGuard guard(mtx);
		for (auto it = pages.begin(); it != pages.end(); ) {
			if (it->allocationCount == 0) {
				if (it == current) {
					current = pages.end();
				}
				VirtualFree(it->base, 0, MEM_RELEASE);
				it = pages.erase(it);
			} else {
				++it;
			}
		}
	}
};
static TrampolinePool g_trampolinePool;

}

#include <memoryapi.h>
#include <Tamper.hpp>
namespace utils {

struct CPUState {
	int edi;       // +0x00
	int esi;       // +0x04
	int ebp;       // +0x08
	int esp;       // +0x0C, old ESP
	int ebx;       // +0x10
	int edx;       // +0x14
	int ecx;       // +0x18
	int eax;       // +0x1C
};
template <int oprSize>
class TrampTamper {//credit enebe shady/memory.cpp
	static_assert(oprSize >= 5);
	static constexpr int size = oprSize + 14;
	//std::array<byte, size> shim;
	byte* shim = nullptr;
	DWORD addr = NULL;

	TrampTamper(const TrampTamper&) = delete;
	TrampTamper& operator=(const TrampTamper&) = delete;
public:
	inline auto alloc(int sz = size) {//TODO: pool allocator in a single page
		return reinterpret_cast<decltype(shim)>(
			//VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)
			g_trampolinePool.allocate(sz)
		);
	}
	inline void free() {
		//if (shim) VirtualFree(shim, 0, MEM_RELEASE);
        g_trampolinePool.free(shim);
		shim = nullptr;
	}
	inline void hook(void* target) {
		if (!addr) return;
		shim = alloc(size);
		if (!shim) return;
		int i = -1;
		shim[++i] = 0x60;// 0;pushad
		shim[++i] = 0x89; shim[++i] = 0xE1;// 1;mov ecx, esp
		auto& calla = shim[++i]; calla = 0xE8; //	3;call hook
		i += 4;//	4~7;to target
		shim[++i] = 0x61;// 8;popad
		auto& opra = shim[++i];// 9~size-6;org opr
		auto& jmpa = shim[i += oprSize]; jmpa = 0xE9; // size-5;jmp org
		i += 4;// size-4~size-1; 
#ifdef _DEBUG
		assert(i == size - 1);
#endif // _DEBUG

		DWORD old;
		if (VirtualProtect((PVOID)addr, oprSize, PAGE_EXECUTE_READWRITE, &old)) {
			memcpy((void*)&opra, (const void*)addr, oprSize);
			memset((void*)addr, 0x90, oprSize);
			SokuLib::TamperNearJmp(addr, shim);
			VirtualProtect((PVOID)addr, oprSize, old, &old);

			SokuLib::TamperNearCall((DWORD)&calla, target);
			SokuLib::TamperNearJmp((DWORD)&jmpa, addr + oprSize);
		}
		else {
			free();
		}
	}
	inline void restore() {
		if (!addr || !shim) return;
		auto& opra = shim[9];
		DWORD old;
		VirtualProtect((PVOID)addr, oprSize, PAGE_EXECUTE_READWRITE, &old);
		memcpy((void*)addr, &opra, oprSize);
		VirtualProtect((PVOID)addr, oprSize, old, &old);
		free();
	}
	TrampTamper(DWORD source) : addr(source) {}
	~TrampTamper() {
		restore();
	}
};


}