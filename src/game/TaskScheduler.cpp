// TaskScheduler.cpp - Cooperative multitasking scheduler
// Exact replica of the original assembly-based stack switching:
//   - Tasks entered via JMP (no return address on task stack)
//   - Yield saves task ESP, restores scheduler ESP, RETs to scheduler
//   - Scheduler resumes by restoring task ESP, RETs into task
//   - No setjmp/longjmp — pure ESP save/restore like the original
//
// Memory Layout (matching original):
//   g_TasksTable[3] at 0x00D1FDE4 (0x7C bytes each)
//   g_TasksESP[3]   at 0x00D91A70
//   g_TasksEIP[3]   at 0x00D91A80
//   g_SchedulerESP  at 0x00D91A8C
//   g_CurrentTaskID at 0x00D91A7C
//   g_AsyncRpcCallback at 0x00D91A90
//
// Original entry point: TaskScheduler_Update at 0x004200E0
// Original helpers:
//   SwitchToTask at 0x00475740
//   Yield at 0x00475768
//   ReturnToScheduler at 0x00475788
//   ReturnToSchedulerAndKillTask at 0x004757a8
//   Task_execute at 0x004201C0
//   Task_sleep at 0x004201E0
//   Task_exit (FUN_00420210) at 0x00420210
//   Task_chain at 0x00420230
//
// Two switch implementations, selected by compiler:
//   - MSVC x86: the original's naked-asm ESP switching (below).
//   - Everything else (Linux, and later ARM): ucontext makecontext/swapcontext,
//     which expresses the same "run until yield, resume where you left off"
//     contract without hand-written assembly. See docs/LINUX_PORT.md Phase 1.
// Both share the public API and the scheduling loop.
//
// IMPORTANT (MSVC path): /RTCs (Runtime Stack Check) MUST be disabled in this
// file. The manual ESP switching (g_SchedulerESP save/restore via naked asm and
// inline asm) is fundamentally incompatible with the compiler-inserted
// ESP verification that /RTCs adds after every CALL instruction. The
// original game was built with MSVC 4.x which did not have this feature.
// The ucontext path has no such constraint.

#ifdef _MSC_VER
// Disable "Run-Time Check Failure #0" ESP verification for this TU
#pragma runtime_checks("s", off)

// Disable frame pointer generation (/Oy). The task resume path only
// switches ESP; EBP remains the scheduler's value from PUSHAD.
// If the task functions use EBP-based stack frames, mov-esp-ebp
// in the epilogue would jump to the scheduler's frame, corrupting
// the resume. The original game was compiled with frame pointer omission.
#pragma optimize("y", on)
#endif

#include "../Globals.h"
#include "../platform/platform.h"

#define TASK_STACK_SIZE   262144      // 256KB per task
#define TASK_MAX           3
#define TASK_SUSPENDED     0x40       // Bit 6: suspend flag
#define TASK_DEAD          0x00
#define TASK_SLEEPING      0x01
#define TASK_START         0x02
#define TASK_YIELD         0x04
#define TASK_ACTIVE        0x7F

#define TASK_SIZE          0x7C       // sizeof(TaskControlBlock)

// Task stacks: allocated with GUARD PAGES between each 256KB slot. A task
// that overflows its slot faults immediately on the guard page with the
// culprit's instruction pointer in crash.log, instead of silently corrupting
// a neighbouring task's saved registers (which killed standalone Release
// runs at room load as POPAD-restored garbage).
#define TASK_GUARD_SIZE   4096
static BYTE* g_TaskStackBase = NULL;   // start of slot 0's usable area

// Base of a task slot's usable stack. The layout is
// [G0][S0][G1][S1][G2][S2][G3], and g_TaskStackBase already skips G0, so
// slot id starts at g_TaskStackBase + id*(stack+guard).
static BYTE* TaskStackBase(int id)
{
    return g_TaskStackBase + (size_t)id * (TASK_STACK_SIZE + TASK_GUARD_SIZE);
}

#if defined(_MSC_VER) && defined(_M_IX86)

// Initial ESP for a task START dispatch. The original layout had contiguous
// stacks, so tasks were entered with ESP exactly at slot_top and a read at
// [slot_top] hit the next slot's memory. With the port's guard pages,
// slot_top IS the first PAGE_NOACCESS byte, and the MSVC Release stack-
// alignment prologue (push ebx / mov ebx,esp / and esp,-16 / mov ebp,[ebx+4])
// reads [entry_esp] — e.g. options_menu+0xD faulting on its movaps alignment
// prologue (0xC0000005 at options_menu+0xd, EBX=slot_top-4, ESP=slot_top-0x10).
// Enter tasks 16 bytes below the guard so [entry_esp] stays readable and the
// ESP stays 16-byte aligned like the original dispatch.
static uintptr_t TaskStackTop(int id)
{
    return (uintptr_t)(TaskStackBase(id) + TASK_STACK_SIZE) - 16;
}

#pragma warning(disable: 4731)  // frame pointer register modified by inline asm

// ============================================================================
// Naked assembly: switch to task (initial dispatch)
// Original two-level structure:
//   0x0047575c (SwitchToTask):  PUSHAD; PUSHFD; CALL 0x00475740; POPFD; POPAD; RET
//   0x00475740 (inner core):    save sched ESP; load task ESP; JMP task EIP
// ============================================================================

// Inner core: actual stack switch (0x00475740)
__declspec(naked) static void SwitchToTask_Core(void) {
    __asm {
        mov  eax, [g_CurrentTaskID]
        mov  [g_SchedulerESP], esp      ; save scheduler ESP (points to wrapper return addr)
        mov  esp, [g_TasksESP + eax*4]  ; load task's ESP
        jmp  [g_TasksEIP + eax*4]       ; jump to task (no return address on task stack)
    }
}

// Outer wrapper: save registers/flags, call inner core, restore on yield (0x0047575c)
__declspec(naked) static void SwitchToTask_Asm() {
    __asm {
        pushad                          ; save EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
        pushfd                          ; save EFLAGS
        call SwitchToTask_Core          ; CALL pushes return addr -> captured by g_SchedulerESP
        popfd                           ; restore EFLAGS (reached when task yields back)
        popad                           ; restore all registers
        ret                             ; return to TaskScheduler_Update
    }
}

// ============================================================================
// Naked assembly: resume task after sleep
// Original two-level structure:
//   0x0047579c (ReturnToScheduler): PUSHAD; PUSHFD; CALL 0x00475788; POPFD; POPAD; RET
//   0x00475788 (inner core):        save sched ESP; load task ESP; RET to task
// ============================================================================

// Inner core: actual stack switch (0x00475788)
// TaskYield pushed all task registers onto the task stack via PUSHAD.
// This function must POPAD them back before RETurning to the task.
__declspec(naked) static void ReturnToScheduler_Core(void) {
    __asm {
        mov  eax, [g_CurrentTaskID]
        mov  [g_SchedulerESP], esp      ; save scheduler ESP (points to wrapper return addr)
        mov  esp, [g_TasksESP + eax*4]  ; load task's saved ESP (points to PUSHAD block)
        popad                           ; restore task's EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
        ret                             ; return to task (pops from task's restored stack)
    }
}

// Outer wrapper: save registers/flags, call inner core, restore on yield (0x0047579c)
__declspec(naked) static void ReturnToScheduler_Resume() {
    __asm {
        pushad                          ; save EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
        pushfd                          ; save EFLAGS
        call ReturnToScheduler_Core     ; CALL pushes return addr -> captured by g_SchedulerESP
        popfd                           ; restore EFLAGS (reached when task yields back)
        popad                           ; restore all registers
        ret                             ; return to TaskScheduler_Update
    }
}

// ============================================================================
// Naked assembly: TaskYield — saves task ESP+registers, returns to scheduler (0x00475768)
// PUSHAD saves all task registers (EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI) onto the
// task stack so that ReturnToScheduler_Core can POPAD them back on resume.
// Without this, the task's EBP/ESI/EDI/EBX get corrupted by scheduler values.
// ============================================================================
__declspec(naked) static void TaskYield(void) {
    __asm {
        pushad                              ; save all task registers to task stack
        mov  eax, [g_CurrentTaskID]
        mov  [g_TasksESP + eax*4], esp      ; save task ESP (points to PUSHAD block)
        mov  esp, [g_SchedulerESP]          ; restore scheduler's ESP
        ret                                 ; return to scheduler wrapper
    }
}

static void TaskSwitch_Start(void)
{
    g_TasksESP[g_CurrentTaskID] = TaskStackTop(g_CurrentTaskID);
    SwitchToTask_Asm();
}

static void TaskSwitch_Resume(void)
{
    ReturnToScheduler_Resume();
}

#elif defined(__EMSCRIPTEN__)

// Port-added: fibers preserve stackful cooperative yields in wasm32.
#include "../platform/web/task_switch.inl"

#else  // !(MSVC x86)

// ============================================================================
// Portable switch primitives (ucontext).
//
// Contract, identical to the assembly above: a task runs until it calls one of
// the yield points; the scheduler regains control at the statement after the
// switch call; resuming re-enters the task exactly where it left off.
//
// Each slot has its own ucontext and uses its guarded stack from
// plat_alloc_guarded_stacks, so an overflow still faults on a guard page.
// ============================================================================
#include <ucontext.h>

static ucontext_t s_SchedContext;
static ucontext_t s_TaskContext[TASK_MAX];

void Task_exit(void);   // forward: a task function that returns has ended

// Task entry trampoline. Runs on the task's own stack.
static void TaskEntry(void)
{
    void (*fn)(void) = (void (*)(void))g_TasksEIP[g_CurrentTaskID];
    fn();
    // The original tasks never return (they end with Task_exit/Task_chain),
    // but a plain return must not run off the stack.
    Task_exit();
}

static void TaskSwitch_Start(void)
{
    int id = g_CurrentTaskID;
    ucontext_t* ctx = &s_TaskContext[id];

    getcontext(ctx);
    ctx->uc_stack.ss_sp = TaskStackBase(id);
    ctx->uc_stack.ss_size = TASK_STACK_SIZE;
    ctx->uc_link = &s_SchedContext;
    makecontext(ctx, TaskEntry, 0);

    swapcontext(&s_SchedContext, ctx);
}

static void TaskSwitch_Resume(void)
{
    swapcontext(&s_SchedContext, &s_TaskContext[g_CurrentTaskID]);
}

static void TaskYield(void)
{
    swapcontext(&s_TaskContext[g_CurrentTaskID], &s_SchedContext);
}

#endif  // _MSC_VER && _M_IX86

// ============================================================================
// TaskScheduler_Update (0x004200E0)
// ============================================================================
void TaskScheduler_Update(void)
{
    g_CurrentTask = g_TasksTable;
    g_CurrentTaskPtr = g_TasksTable;
    g_CurrentTaskID = 0;
    g_SchedulerRunningFlag = 1;

    TaskControlBlock* endPtr = &g_TasksTable[TASK_MAX];

    do {
        int state = g_CurrentTask->state;
        if (state == TASK_SLEEPING) {
            g_CurrentTask->sleepCounter -= 1;
            if (g_CurrentTask->sleepCounter == 0) {
_resume_task:
                g_CurrentTask->state = TASK_ACTIVE;
                TaskSwitch_Resume();
            }
        }
        else if (state == TASK_START) {
            TaskSwitch_Start();
        }
        else if (state == TASK_YIELD) {
            goto _resume_task;
        }

        if (g_AsyncRpcCallback == NULL) {
            ++g_CurrentTaskID;
            ++g_CurrentTask;

            if (g_CurrentTask >= endPtr) {
                g_SchedulerRunningFlag = 0;
                return;
            }
        }
        else {
            void (*callback)() = (void (*)())g_AsyncRpcCallback;
            g_AsyncRpcCallback = NULL;
            callback();
        }
    } while(1);
}

// ============================================================================
// Task_execute (0x004201C0)
// ============================================================================
void Task_execute(int id, void* func)
{
    if (id < 0 || id >= TASK_MAX) return;
    if (func == NULL) return;

    g_TasksEIP[id] = func;
    g_TasksTable[id].state = TASK_START;
    g_TasksTable[id].sleepCounter = 0;
}

// ============================================================================
// Task_sleep (0x004201E0)
// Original: sets sleep counter + state, then calls Yield (naked function).
// ============================================================================
void Task_sleep(int frames)
{
    int id = g_CurrentTaskID;
    if (id < 0 || id >= TASK_MAX) return;

    g_TasksTable[id].sleepCounter = (short)frames;
    g_TasksTable[id].state = TASK_SLEEPING;

    TaskYield();  // switch back to the scheduler
}

// ============================================================================
// Task_chain (0x00420230)
// ============================================================================
void Task_chain(void* func)
{
    int id = g_CurrentTaskID;
    if (id < 0 || id >= TASK_MAX) return;

    g_TasksEIP[id] = func;
    g_TasksTable[id].state = TASK_START;
    g_TasksTable[id].sleepCounter = 0;

    // The slot is being restarted, so its saved context is discarded; yielding
    // returns to the scheduler, which will re-enter it as TASK_START.
    TaskYield();
}

// ============================================================================
// Task_exit (0x00420210)
// ============================================================================
void Task_exit(void)
{
    int id = g_CurrentTaskID;
    if (id < 0 || id >= TASK_MAX) return;

    g_TasksTable[id].state = TASK_DEAD;
    g_TasksTable[id].sleepCounter = 0;

    TaskYield();
}

// ============================================================================
// Task_suspend (0x00420260)
// ============================================================================
void Task_suspend(int id)
{
    if (id < 0 || id >= TASK_MAX) return;
    g_TasksTable[id].state = (short)(g_TasksTable[id].state | TASK_SUSPENDED);
}

// ============================================================================
// Task_Resume (0x00420270)
// ============================================================================
void Task_Resume(int id)
{
    if (id < 0 || id >= TASK_MAX) return;
    g_TasksTable[id].state = (short)(g_TasksTable[id].state & ~TASK_SUSPENDED);
}

// ============================================================================
// ExecAsync (0x004202a0)
// Executes a callback asynchronously via the task scheduler.
// If the scheduler is not running, calls the callback directly.
// If running, waits for any pending async callback, registers the new one,
// yields via Task_sleep(1) so the scheduler fires it in the advance block,
// then clears it after the task wakes up.
// ============================================================================
void ExecAsync(void* callback)
{
    if (g_SchedulerRunningFlag == 0) {
        ((void(*)())callback)();
        return;
    }
    while (g_AsyncRpcCallback != NULL) {
        Task_sleep(1);
    }
    g_AsyncRpcCallback = callback;
    Task_sleep(1);
    g_AsyncRpcCallback = NULL;
}

// ============================================================================
// TaskScheduler_Reset (0x004200A0)
// ============================================================================
void TaskScheduler_Reset(void)
{
    for (int i = 0; i < TASK_MAX; i++) {
        g_TasksTable[i].state = TASK_DEAD;
        g_TasksTable[i].sleepCounter = 0;
        g_TasksESP[i] = 0;
        g_TasksEIP[i] = NULL;
    }
    g_CurrentTaskID = 0;
    g_SchedulerESP = 0;
    g_SchedulerRunningFlag = 0;
    g_AsyncRpcCallback = NULL;
    g_CurrentTask = NULL;
    g_CurrentTaskPtr = NULL;
}

// ============================================================================
// TaskScheduler_Init
// ============================================================================
void TaskScheduler_Init(void)
{
    if (g_TaskStackBase == NULL) {
        // Guard-paged stacks, layout [G0][S0][G1][S1][G2][S2][G3]. The platform
        // layer owns the reservation and poisons the guards (VirtualAlloc +
        // VirtualProtect on Windows, mmap + mprotect elsewhere).
        BYTE* mem = (BYTE*)plat_alloc_guarded_stacks(TASK_MAX, TASK_STACK_SIZE,
                                                     TASK_GUARD_SIZE);
        if (mem != NULL) {
            g_TaskStackBase = mem;  // already past G0 -> start of S0
        } else {
            plat_fatal("Failed to allocate task stacks");
        }
    }
    g_StackPointer = g_TaskStackBase;
    TaskScheduler_Reset();
}
