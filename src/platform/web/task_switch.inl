// Android/Web port addition. Included only in TaskScheduler.cpp's wasm branch.
// Emscripten fibers retain separate C and Asyncify stacks for each task.
#include <emscripten/fiber.h>
#include <emscripten.h>
static emscripten_fiber_t s_schedFiber;
static emscripten_fiber_t s_taskFiber[TASK_MAX];
static unsigned char s_schedAsync[131072];
static unsigned char s_taskAsync[TASK_MAX][131072];
static bool s_fiberReady = false;
void Task_exit(void);
static void WebTaskEntry(void*)
{
    ((void (*)(void))g_TasksEIP[g_CurrentTaskID])();
    Task_exit();
}
static void TaskSwitch_Start(void)
{
    if (!s_fiberReady) {
        emscripten_fiber_init_from_current_context(&s_schedFiber,
            s_schedAsync, sizeof(s_schedAsync));
        s_fiberReady = true;
    }
    const int id = g_CurrentTaskID;
    emscripten_fiber_init(&s_taskFiber[id], WebTaskEntry, NULL,
        TaskStackBase(id), TASK_STACK_SIZE,
        s_taskAsync[id], sizeof(s_taskAsync[id]));
    emscripten_fiber_swap(&s_schedFiber, &s_taskFiber[id]);
}
static void TaskSwitch_Resume(void)
{
    emscripten_fiber_swap(&s_schedFiber, &s_taskFiber[g_CurrentTaskID]);
}
static void TaskYield(void)
{
    emscripten_fiber_swap(&s_taskFiber[g_CurrentTaskID], &s_schedFiber);
}
