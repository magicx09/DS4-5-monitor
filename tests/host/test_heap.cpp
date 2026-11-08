// Counts heap allocations (every operator new), so a test can check that a
// main-loop path doesn't touch the heap (lifetime pass batch 3: the 0x81
// pollers). malloc/free underneath, so ASan still sees every block.
#include <cstdlib>
#include <new>
#include "fakes.h"

long g_heap_allocs = 0;

#if defined(DS5_WINDOWS_ASAN)
#include <sanitizer/allocator_interface.h>
#include "test.h"
namespace {
void count_alloc(const volatile void *, std::size_t) { ++g_heap_allocs; }
void count_free(const volatile void *) {}
const int hooks_installed = __sanitizer_install_malloc_and_free_hooks(count_alloc, count_free);
}

TEST(windows_asan_heap_counter_observes_new) {
    CHECK(hooks_installed > 0);
    const long before = g_heap_allocs;
    void *p = ::operator new(32);
    CHECK(g_heap_allocs > before);
    ::operator delete(p);
}
#else
void *operator new(std::size_t n) {
    g_heap_allocs++;
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
#endif
