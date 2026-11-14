/**
 * @file
 * @brief What a time model is, as types and nothing else.
 *
 * The scheduler runs every event at its own time, whatever a time model
 * says. What a model decides is the rest: how far the clock moves after a
 * pass of the firmware's loop, and what it costs the firmware to read the
 * microsecond clock.
 *
 * A model is pure arithmetic over what the runtime tells it. It is given no
 * host time, and must not read any: simulated time is the simulation's own,
 * and a model that read the computer's clock would make a run depend on how
 * busy the computer was. The runtime keeps its guarantees whatever a model
 * returns: a step is at least a microsecond and at most
 * @ref LOOPER_TIME_STEP_LIMIT_MICROSECONDS, every event on the way runs at
 * its own time, and a read costs at most
 * @ref LOOPER_TIME_READ_COST_LIMIT_NANOSECONDS.
 *
 * This header declares no function and no object, so a model kept in a
 * repository of its own is written and tested against it alone. Offering it
 * to a simulation is the runtime's part: `LOOPER_TIME_MODEL_REGISTER` in the
 * runtime's `looper_time_model.h`, written in a file of the project.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef LOOPER_TIME_MODEL_TYPES_H
#define LOOPER_TIME_MODEL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The longest step the runtime takes, whatever a model asks for: one simulated second. */
#define LOOPER_TIME_STEP_LIMIT_MICROSECONDS 1000000u

/** @brief The most one reading of the clock may cost: one simulated millisecond. */
#define LOOPER_TIME_READ_COST_LIMIT_NANOSECONDS 1000000u

/** @brief A parameter's default that is the application's own pass time, `idle_microseconds`. */
#define LOOPER_TIME_PARAM_PASS_TIME UINT64_MAX

/** @brief What @ref looper_pass_facts_t::next_due_ns says when nothing is scheduled. */
#define LOOPER_TIME_NOTHING_DUE UINT64_MAX

/** @brief Most parameters one model takes. */
#define LOOPER_TIME_MODEL_PARAMETERS_MAX 8u

/** @brief One parameter of a model: its name, what it means, and the values it takes. */
typedef struct {
    /** @brief As `--time-param` and looper.toml's `[time]` name it. */
    const char *name;
    /** @brief What it sets, in a sentence. */
    const char *description;
    /** @brief The value when none is given, or @ref LOOPER_TIME_PARAM_PASS_TIME. */
    uint64_t default_value;
    /** @brief The smallest value taken. */
    uint64_t minimum;
    /** @brief The largest value taken. */
    uint64_t maximum;
} looper_time_param_spec_t;

/**
 * @brief The parameters a model begins with.
 *
 * Checked and complete: one value for each of the model's parameters, in
 * the order the model lists them, its default where none was given.
 */
typedef struct {
    /** @brief The application's own pass time, `idle_microseconds`. */
    uint64_t pass_us;
    /** @brief How many values there are: the model's parameter count. */
    size_t count;
    /** @brief The values, in the order of the model's parameters. */
    const uint64_t *values;
} looper_time_params_t;

/** @brief What the runtime knows about the pass that just ended, and the wait before it. */
typedef struct {
    /**
     * @brief Whether something happened since the last pass ended.
     *
     * An event ran, an interrupt was taken, a pin changed, a bus carried
     * something, an analog output took another value, a client changed the
     * simulation -- anything that is recorded as having happened. A pass that
     * only read, and read what it read before, had none.
     */
    bool had_effect;
    /** @brief How often the firmware read the microsecond clock in the pass. */
    uint32_t time_reads;
    /** @brief Nanoseconds until the next scheduled event, or @ref LOOPER_TIME_NOTHING_DUE. */
    uint64_t next_due_ns;
} looper_pass_facts_t;

/** @brief A time model. */
typedef struct {
    /** @brief @ref LOOPER_CONTRACT_ABI as the model was built; the runtime refuses another. */
    uint32_t abi_version;
    /** @brief The name `--time-model` selects it by: lower case, no spaces. */
    const char *name;
    /** @brief What it does, in a sentence. */
    const char *description;
    /** @brief Its parameters; NULL when it takes none. */
    const looper_time_param_spec_t *parameters;
    /** @brief How many there are, at most @ref LOOPER_TIME_MODEL_PARAMETERS_MAX. */
    size_t parameter_count;
    /** @brief Take the parameters, before the firmware starts. May be NULL. */
    void (*begin)(const looper_time_params_t *params);
    /** @brief How long, in nanoseconds, until the next pass, after a pass with these facts. */
    uint64_t (*after_pass)(const looper_pass_facts_t *pass);
    /** @brief What one reading of the microsecond clock costs, in nanoseconds; 0 for nothing. May be NULL. */
    uint64_t (*time_read_cost)(void);
} looper_time_model_t;

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_TIME_MODEL_TYPES_H */
