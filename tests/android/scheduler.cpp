// Exercise the actual scheduler and wasm fiber backend without game assets.
#include "Globals.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <emscripten.h>
void* g_StackPointer=nullptr;
TaskControlBlock g_TasksTable[3]={};
TaskControlBlock* g_CurrentTask=nullptr;
TaskControlBlock* g_CurrentTaskPtr=nullptr;
uintptr_t g_TasksESP[3]={};
void* g_TasksEIP[3]={};
DWORD g_CurrentTaskID=0;
uintptr_t g_SchedulerESP=0;
void* g_AsyncRpcCallback=nullptr;
DWORD g_SchedulerRunningFlag=0;
void* plat_alloc_guarded_stacks(int count,size_t stack,size_t guard) {
    return (char*)calloc(1,count*(stack+guard)+guard)+guard;
}
void plat_fatal(const char*) { abort(); }
static int a=0,b=0,c=0,callback=0;
static void rpc(){callback++;}
static void chained(){c++;Task_exit();assert(false);}
static void taskA(){int local=123;a++;Task_sleep(2);assert(local==123);a++;Task_chain((void*)chained);assert(false);}
static void taskB(){for(int i=0;i<3;i++){b++;Task_sleep(1);}ExecAsync((void*)rpc);Task_exit();assert(false);}
int main(){
    TaskScheduler_Init();
    // Match the application: first frame yields before dispatching a task.
    emscripten_sleep(1);
    Task_execute(0,(void*)taskA);Task_execute(1,(void*)taskB);
    TaskScheduler_Update();assert(a==1 && b==1 && c==0);
    Task_suspend(1);TaskScheduler_Update();assert(a==1 && b==1);
    Task_Resume(1);
    for(int i=0;i<12;i++){TaskScheduler_Update();emscripten_sleep(1);}
    assert(a==2 && b==3 && c==1 && callback==1);
    Task_execute(2,(void*)chained);TaskScheduler_Update();assert(c==2);
    TaskScheduler_Reset();Task_execute(0,(void*)chained);TaskScheduler_Update();assert(c==3);
    puts("PASS: scheduler sleep, stack locals, suspend/resume, chain, exit, async callback, restart/reset");
}
