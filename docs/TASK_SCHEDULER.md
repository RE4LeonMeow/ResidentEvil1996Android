---
name: task-scheduler
description: describes the internal cooperative multitasking system used by the Resident Evil 1 PC port.
---

# Instructions

This document describes the internal cooperative multitasking system used by the Resident Evil 1 PC port.
The system is a direct port of the PlayStation scheduler (PSYQ `ChangeTh`) implemented in x86 through manual stack switching.

The engine contains a miniature operating system that executes game scripts as tasks.

# Memory Layout

## Task Control Blocks

Address: **0x00D1FDE4 – 0x00D1FF58**

Each task occupies **0x7C bytes**.

```
struct TaskControlBlock
{
    int16_t state/flags;  // +0x00
    int16_t sleepCounter; // +0x02
    uint8_t reserved[0x78];
};
```

State values: (bits 0-5)

| Value      | Meaning                     |
| ---------- | --------------------------- |
| b00000000  | dead task                   |
| b00000001  | sleeping (waiting frames)   |
| b00000010  | start execution             |
| b00000100  | yield and resume next frame |
| b01111111  | active/running              |

Flags (bits 6-7)

| Value      | Meaning                     |
| ---------- | --------------------------- |
| b01000000  | TASK_SUSPENDED              |

Max tasks = 3

Each Task has a reserved stack size of 256KB

```
tasksESP[id] = id * 262144 + stackPointer + 262144;
```

---

## Scheduler Globals

| Address    | Name                     | Description               |
| ---------- | ------------------------ | ------------------------- |
| 0x007e0cc8 | g_StackPointer           | stack pointer             |
| 0x004ba0b8 | g_g_SchedulerRunningFlag | Scheduler running flag    |
| 0x00bf09ec | g_CurrentTask            | pointer to current task   |
| 0x00d1fde4 | g_TasksTable[3]          | tasks table               |
| 0x00d91a68 | g_CurrentTaskPtr         | pointer to current task   |
| 0x00d91a70 | g_TasksESP[3]            | tasks ESP                 |
| 0x00d91a7c | g_CurrentTaskID          | current task ID           |
| 0x00d91a80 | g_TasksEIP[3]            | tasks EIP (the tasks PC)  |
| 0x00d91a8c | g_SchedulerESP           | scheduler ESP             |
| 0x00d91a90 | g_AsyncRpcCallback       | async function callback   |

---

# Core Concept

The scheduler does NOT use threads.

Instead it performs context switching by:

1. Saving ESP of current task
2. Restoring ESP of another task
3. Jumping to its saved EIP

This recreates the PSX `ChangeTh()` behavior exactly.

---

# Low Level Switchers

## SwitchToTask

Address: **0x0047575C**

```
save scheduler ESP
load task ESP
jmp task EIP
```

Used when:

* starting a task
* waking a task
* resuming next frame

---

## ReturnToScheduler

Address: **0x0047579C**

```
save task ESP
restore scheduler ESP
ret
```

Called when a task yields, sleeps, or exits.

---

## Yield

Address: **0x0047577C**

Stores EFLAGS then returns to scheduler.

Used by:

* Task_sleep

---

## ReturnToSchedulerAndKillTask

Address: **0x004757a8**

Abort current task and jump to scheduler.

Used by:

* Task_exit
* Task_chain

---

# Scheduler Loop

## TaskScheduler_Update

Address: **0x004200E0**

Executed every frame.

Pseudo:

```
for each task:
    // tasks
    if sleeping:
        decrement counter
        if 0 → run

    else if start:
        run

    else if yield:
        run next frame

   // hardware/events callbacks
   if async event pending
        execute async callback
   else
    process next task
```

After a task yields, execution returns here.

---

# Task API

## Reset all tasks

### TaskScheduler_Reset

Address: **0x004200A0**

Clears every task slot. Called once at startup

Equivalent:

```
 TaskScheduler_Reset()
```

---

## Start a task

### Task_execute

Address: **0x004201C0**

```
g_TasksEIP[id] = function;
tasks[id].state = START;
```

Creates runnable script.

---

## Sleep

### Task_sleep

Address: **0x004201E0**

```
current->sleepCounter = frames;
current->state = SLEEP;
yield();
```

Exact PSX `ChangeTh()` behavior.

---

## Exit

### Task_exit

Address: **0x00420210**

Kills current task and yields.

```
current->state = DEAD
ReturnToSchedulerAndKillTask();
```

---

## Chain

### Task_chain

Address: **0x00420230**

Replaces current task function without destroying its stack.

```
g_TasksEIP[current] = function;
current->state = START
ReturnToSchedulerAndKillTask();
```

---

## Suspend

### Task_suspend

Address: **0x00420260**

Pauses a task

```
tasks[id].state = tasks[id].state | TASK_SUSPENDED
```

---

## Resume

### Task_resume

Address: **0x00420270**

Unpauses a task.

```
tasks[id].state = tasks[id].state & ~TASK_SUSPENDED
```

---

## Async Call

### ExecAsync

Address: **0x00420290**

Runs event driven functions inside scheduler context.

Used by:

* streaming
* sound
* memory cleanup

Implements synchronous RPC:

```
set callback
sleep 1 frame
scheduler executes it
resume caller
```

---

# Execution Model

The engine runs like this:

```
MainLoop:
    Inputs_Update()
    Audio_Update()
    TaskScheduler_Update()
    Render()
Repeat
```

Every enemy, camera, door, animation, and script runs as a cooperative task.

There are NO OS threads.

---

# Important Behavior

The game is deterministic because:

• No preemption
• No timers
• Only frame-based switching
• Exact PSX behavior preserved

This is why PC and PSX logic stay synchronized.

---

# Mental Model

Think of the engine as:

> A PlayStation kernel emulated in software inside the PC executable.

Not a thread scheduler —
a virtual console operating system.
