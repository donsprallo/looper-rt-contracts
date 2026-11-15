/**
 * @file
 * @brief What an RTOS port needs of looper: contexts with stacks of their own, and when to switch.
 *
 * looper runs a firmware in one thread of the host. An RTOS needs a stack per
 * task and a switch between them; the ports of FreeRTOS for POSIX and Win32
 * do it with real threads and a tick from the wall clock, so a run is never
 * the same twice. looper does it with contexts on its own stacks (fibers on
 * Windows, `ucontext` elsewhere) that switch only where looper lets them, so
 * that two runs give the same events. A port of an RTOS -- FreeRTOS, Zephyr,
 * ThreadX, embOS -- is written against this header alone, outside looper-rt.
 *
 * How a run goes:
 *
 * - The firmware's start, up to the scheduler, runs in a context of its own.
 *   The port's start of the scheduler switches from it into the first task
 *   and never comes back, as on the board.
 * - A task gives the processor away itself -- a yield, a block, a delay -- by
 *   choosing the next task and calling `switch_to`.
 * - The tick is a timer source of the contract; its handler (`irq.handler`)
 *   calls the port's tick, which may make another task due. An ISR does not
 *   switch: it says a switch is wanted, and looper asks the port at the next
 *   interruption point whether to switch (`may_switch`), outside an ISR,
 *   outside the adapter and while interrupts are not masked. A task that
 *   reaches no such point is not interrupted, as with interrupts.
 * - When nothing is ready the port calls `idle`: the pass of the firmware
 *   ends there, looper serves its clients and moves the clock, and `idle`
 *   returns in the next pass.
 * - Every switch says why it happens (looper_switch_reason_t), and the port
 *   lists its tasks when looper asks (`tasks`): a run's events show which
 *   task ran when, and why the next one took over.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef LOOPER_CONTEXT_H
#define LOOPER_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A context with a stack of its own; looper's, opaque to the port. */
typedef struct looper_context looper_context_t;

/** @brief Why a switch happens, as the port knows it. */
typedef enum {
    /** @brief The task gave way and stays ready: a yield. */
    LOOPER_SWITCH_YIELD = 1,
    /** @brief The task waits: a delay, a queue, a semaphore. */
    LOOPER_SWITCH_BLOCK = 2,
    /** @brief The tick made another task due. */
    LOOPER_SWITCH_TICK = 3,
    /** @brief An ISR made another task due. */
    LOOPER_SWITCH_ISR = 4,
    /** @brief The race mode forced the switch (`may_switch` with `forced`). */
    LOOPER_SWITCH_RACE = 5,
    /** @brief The scheduler starts: the switch into the first task. */
    LOOPER_SWITCH_START = 6
} looper_switch_reason_t;

/** @brief What a task is doing, as the port's scheduler says. */
typedef enum {
    /** @brief It runs now. */
    LOOPER_TASK_RUNNING = 1,
    /** @brief It could run and waits for its turn. */
    LOOPER_TASK_READY = 2,
    /** @brief It waits for time or for an object. */
    LOOPER_TASK_BLOCKED = 3,
    /** @brief It is held until another task resumes it. */
    LOOPER_TASK_SUSPENDED = 4,
    /** @brief It was deleted and its context is not freed yet. */
    LOOPER_TASK_DELETED = 5
} looper_task_state_t;

/** @brief One task, as the port lists it for looper_rtos_port_t::tasks. */
typedef struct {
    /** @brief The context it runs in, the one `create` gave the port. */
    looper_context_t *context;
    /** @brief Its name; looper copies it. */
    const char *name;
    /** @brief A looper_task_state_t. */
    uint32_t state;
    /** @brief Its priority now, the RTOS's number. */
    uint32_t priority;
    /** @brief The stack the firmware created it with, in words of the target; 0 when unknown. */
    uint32_t stack_words;
    /** @brief The address of its entry function, to name it; 0 when unknown. */
    uintptr_t entry;
} looper_rtos_task_t;

/** @brief What looper calls in the port, set with looper_rtos_host_t::set_port. */
typedef struct {
    /** @brief Handed back to every member. */
    void *port;
    /**
     * @brief At an interruption point where a switch is allowed: switch if another task is due.
     *
     * The port chooses the next task and calls `switch_to` itself; it returns
     * once this context runs again, or at once when it stays. `forced` asks to
     * give way to another ready task of the same priority even when none is
     * due: the race mode does, so that data two tasks share without a lock
     * shows. Called with interrupts not masked, outside an ISR.
     */
    void (*may_switch)(void *port, bool forced);
    /**
     * @brief Every task the scheduler knows, at most `capacity`; how many were written.
     *
     * Called between two passes of the firmware, from looper's own context,
     * never from a task. NULL when the port lists none.
     */
    size_t (*tasks)(void *port, looper_rtos_task_t *into, size_t capacity);
} looper_rtos_port_t;

/** @brief What looper offers a port, from looper_rtos_host(). */
typedef struct {
    /** @brief Handed back to every member. */
    void *context;
    /**
     * @brief A new context that runs `entry(argument)` when first switched to.
     *
     * Its stack is `stack_bytes` long, with a guard page beyond it, so that
     * an overflow stops the run instead of writing into another task's stack.
     * `name` names it in the run's events. NULL when the host has no room.
     */
    looper_context_t *(*create)(
        void *context, void (*entry)(void *argument), void *argument, size_t stack_bytes, const char *name
    );
    /** @brief The context that runs now. */
    looper_context_t *(*current)(void *context);
    /**
     * @brief Run `next` from where it last stopped; returns when this context is switched to again.
     *
     * `reason` is a looper_switch_reason_t: the run's event of the switch carries it.
     */
    void (*switch_to)(void *context, looper_context_t *next, uint32_t reason);
    /** @brief Free a context that does not run and never will again; its stack goes back to the host. */
    void (*destroy)(void *context, looper_context_t *gone);
    /**
     * @brief Mask interrupts (`enter`), or unmask them.
     *
     * The interrupt lock of the board: looper's findings about how long
     * interrupts are masked count a critical section too. Not nested: the
     * port counts its own nesting and calls this at the outermost level.
     */
    void (*critical)(void *context, bool enter);
    /** @brief Nothing is ready: end the firmware's pass; returns in the next one, after the clock moved. */
    void (*idle)(void *context);
    /** @brief The port's members, called by looper from now on. */
    void (*set_port)(void *context, const looper_rtos_port_t *port);
    /** @brief Whether the running code is an ISR: a switch is then only asked for, at the next point. */
    bool (*in_isr)(void *context);
    /**
     * @brief Name a context anew: its name in the run's events from now on.
     *
     * For an RTOS that knows a task's name only after its stack was made,
     * as FreeRTOS does (`traceTASK_CREATE`).
     */
    void (*name)(void *context, looper_context_t *named, const char *name);
} looper_rtos_host_t;

/**
 * @brief The host of the runtime the firmware runs on; NULL where there is none.
 *
 * Defined by looper-rt in a build with `LOOPER_RTOS`, which runs the firmware's
 * start in a context of its own.
 */
const looper_rtos_host_t *looper_rtos_host(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_CONTEXT_H */
