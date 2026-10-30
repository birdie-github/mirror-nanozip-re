// nz_xp_compat.cpp -- the five Vista entry points the 32-bit Windows build's
// C++ runtime imports, for Windows XP (only built with NZ_XP_COMPAT, which
// tools/release/build.sh sets for the x86 .exe).
//
// mingw-w64's libstdc++ in the win32 thread model implements std::thread,
// std::mutex and std::condition_variable on CONDITION_VARIABLE and GetThreadId,
// which XP does not have, so the loader refuses the .exe. The POSIX-threads
// compiler avoids them, but its emulated TLS runs thread_local destructors over
// storage it has already released (measured: 1199 of 1200 destructors saw a
// clobbered object), which made `t` of a 4-stream -cD archive fail about once in
// 25 runs. So the x86 build keeps the win32 model and gets these instead: the
// import slots libstdc++ calls through (__imp__Name@N) are defined here, so the
// linker never pulls the kernel32 entries. On Vista and later the real functions
// are used (looked up once); on XP, condition variables are a queue of waiters
// each with its own event -- a wake goes to a waiter that was already waiting,
// so it cannot be lost or taken by a later one -- and GetThreadId asks ntdll.
#if defined(_WIN32) && !defined(_WIN64) && defined(NZ_XP_COMPAT)
#include <windows.h>

namespace {

struct Waiter {
    HANDLE ev;
    Waiter* next;
    bool signaled;
};
struct XpCond {
    CRITICAL_SECTION lock;
    Waiter* head;
    Waiter* tail;
};

typedef void(WINAPI* InitFn)(void**);
typedef BOOL(WINAPI* SleepCsFn)(void**, CRITICAL_SECTION*, DWORD);
typedef void(WINAPI* WakeFn)(void**);
typedef DWORD(WINAPI* GetThreadIdFn)(HANDLE);

struct Native {
    InitFn init;
    SleepCsFn sleep_cs;
    WakeFn wake;
    WakeFn wake_all;
    GetThreadIdFn get_thread_id;
};

// 0 = not looked up yet, 1 = native (Vista+), 2 = ours (XP)
volatile LONG g_mode = 0;
Native g_native;

void Resolve() {
    if (g_mode != 0) return;
    Native n{};
    HMODULE k = ::GetModuleHandleW(L"kernel32.dll");
    if (k != nullptr) {
        n.init = reinterpret_cast<InitFn>(::GetProcAddress(k, "InitializeConditionVariable"));
        n.sleep_cs = reinterpret_cast<SleepCsFn>(::GetProcAddress(k, "SleepConditionVariableCS"));
        n.wake = reinterpret_cast<WakeFn>(::GetProcAddress(k, "WakeConditionVariable"));
        n.wake_all = reinterpret_cast<WakeFn>(::GetProcAddress(k, "WakeAllConditionVariable"));
        n.get_thread_id = reinterpret_cast<GetThreadIdFn>(::GetProcAddress(k, "GetThreadId"));
    }
    // NZ_TEST_XP_CONDVAR=1 (tests): XP's way even where the real ones exist
    char force[4] = {0};
    const bool forced = ::GetEnvironmentVariableA("NZ_TEST_XP_CONDVAR", force, sizeof(force)) != 0;
    if (forced) n.get_thread_id = nullptr;
    const bool native = !forced && n.init && n.sleep_cs && n.wake && n.wake_all;
    if (native) g_native = n;
    else g_native.get_thread_id = n.get_thread_id;
    // the table is written before the mode is published; any thread that loses
    // the race resolves the same values
    ::InterlockedCompareExchange(&g_mode, native ? 1 : 2, 0);
}

XpCond* CondOf(void** slot) {
    void* p = ::InterlockedCompareExchangePointer(slot, nullptr, nullptr);
    if (p != nullptr) return static_cast<XpCond*>(p);
    XpCond* c = static_cast<XpCond*>(::HeapAlloc(::GetProcessHeap(), 0, sizeof(XpCond)));
    if (c == nullptr) return nullptr;
    ::InitializeCriticalSection(&c->lock);
    c->head = c->tail = nullptr;
    void* prev = ::InterlockedCompareExchangePointer(slot, c, nullptr);
    if (prev != nullptr) {   // another thread made it first
        ::DeleteCriticalSection(&c->lock);
        ::HeapFree(::GetProcessHeap(), 0, c);
        return static_cast<XpCond*>(prev);
    }
    return c;
}

void WINAPI XpInitializeConditionVariable(void** cv) {
    Resolve();
    if (g_mode == 1) { g_native.init(cv); return; }
    *cv = nullptr;   // CONDITION_VARIABLE_INIT; the state is made on first use
}

BOOL WINAPI XpSleepConditionVariableCS(void** cv, CRITICAL_SECTION* cs, DWORD ms) {
    Resolve();
    if (g_mode == 1) return g_native.sleep_cs(cv, cs, ms);
    XpCond* c = CondOf(cv);
    if (c == nullptr) { ::SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    Waiter w{::CreateEventW(nullptr, FALSE, FALSE, nullptr), nullptr, false};
    if (w.ev == nullptr) return FALSE;
    ::EnterCriticalSection(&c->lock);
    if (c->tail != nullptr) c->tail->next = &w; else c->head = &w;
    c->tail = &w;
    ::LeaveCriticalSection(&c->lock);
    ::LeaveCriticalSection(cs);
    ::WaitForSingleObject(w.ev, ms);
    ::EnterCriticalSection(&c->lock);
    const bool woken = w.signaled;
    if (!woken) {   // timed out: still queued, take it out
        Waiter** pp = &c->head;
        Waiter* prev = nullptr;
        while (*pp != nullptr && *pp != &w) { prev = *pp; pp = &(*pp)->next; }
        if (*pp == &w) {
            *pp = w.next;
            if (c->tail == &w) c->tail = prev;
        }
    }
    ::LeaveCriticalSection(&c->lock);
    ::CloseHandle(w.ev);   // a waker only touches it under c->lock, before `signaled` is set
    ::EnterCriticalSection(cs);
    if (!woken) { ::SetLastError(ERROR_TIMEOUT); return FALSE; }
    return TRUE;
}

void WakeSome(void** cv, bool all) {
    XpCond* c = CondOf(cv);
    if (c == nullptr) return;
    ::EnterCriticalSection(&c->lock);
    while (c->head != nullptr) {
        Waiter* w = c->head;
        c->head = w->next;
        if (c->head == nullptr) c->tail = nullptr;
        ::SetEvent(w->ev);
        w->signaled = true;
        if (!all) break;
    }
    ::LeaveCriticalSection(&c->lock);
}

void WINAPI XpWakeConditionVariable(void** cv) {
    Resolve();
    if (g_mode == 1) { g_native.wake(cv); return; }
    WakeSome(cv, false);
}

void WINAPI XpWakeAllConditionVariable(void** cv) {
    Resolve();
    if (g_mode == 1) { g_native.wake_all(cv); return; }
    WakeSome(cv, true);
}

// THREAD_BASIC_INFORMATION, class 0 of NtQueryInformationThread (XP has it).
struct ThreadBasic {
    LONG exit_status;
    void* teb;
    void* unique_process;
    void* unique_thread;
    ULONG_PTR affinity;
    LONG priority;
    LONG base_priority;
};
typedef LONG(WINAPI* NtQitFn)(HANDLE, int, void*, ULONG, ULONG*);

DWORD WINAPI XpGetThreadId(HANDLE h) {
    Resolve();
    if (g_native.get_thread_id != nullptr) return g_native.get_thread_id(h);
    static NtQitFn q = reinterpret_cast<NtQitFn>(
        ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    if (q == nullptr) return 0;
    ThreadBasic tb{};
    if (q(h, 0, &tb, sizeof(tb), nullptr) < 0) return 0;
    return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(tb.unique_thread));
}

}  // namespace

// The import slots, by their decorated names (stdcall: @ bytes of arguments).
extern "C" {
void* nz_imp_init_cv __asm__("__imp__InitializeConditionVariable@4") = reinterpret_cast<void*>(&XpInitializeConditionVariable);
void* nz_imp_sleep_cv __asm__("__imp__SleepConditionVariableCS@12") = reinterpret_cast<void*>(&XpSleepConditionVariableCS);
void* nz_imp_wake_cv __asm__("__imp__WakeConditionVariable@4") = reinterpret_cast<void*>(&XpWakeConditionVariable);
void* nz_imp_wake_all_cv __asm__("__imp__WakeAllConditionVariable@4") = reinterpret_cast<void*>(&XpWakeAllConditionVariable);
void* nz_imp_get_thread_id __asm__("__imp__GetThreadId@4") = reinterpret_cast<void*>(&XpGetThreadId);
}
#endif
