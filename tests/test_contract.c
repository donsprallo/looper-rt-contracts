/*
 * A chip model and a time model written against the contracts in C11, the
 * way a component writes them: operations, framing, a sample handed its
 * inputs, and a step after a pass.
 */
#include <stdio.h>

#include "looper_device.h"
#include "looper_time_model_types.h"

/* From 1.0 on the major version is the contract's version; 0.x is still being shaped. */
_Static_assert(
    (LOOPER_CONTRACTS_MAJOR == 0) || (LOOPER_CONTRACT_ABI == LOOPER_CONTRACTS_MAJOR),
    "from 1.0 on the major version is LOOPER_CONTRACT_ABI"
);

typedef struct {
    uint8_t registers[4];
    double last_input;
    const looper_device_host_t *host;
} demo_chip_t;

static uint8_t demo_read(void *chip, uint16_t address)
{
    const demo_chip_t *self = chip;

    return (address < 4u) ? self->registers[address] : 0u;
}

static void demo_write(void *chip, uint16_t address, uint8_t value)
{
    demo_chip_t *self = chip;

    if (address < 4u) {
        self->registers[address] = value;
    }
}

static uint64_t demo_interval_us(const void *chip)
{
    (void)chip;
    return 1000u;
}

static void demo_attached(void *chip, const looper_device_host_t *host)
{
    demo_chip_t *self = chip;

    self->host = host;
    host->set_line(host->context, 0u, true);
}

static bool demo_sample(void *chip, const double *inputs, size_t count)
{
    demo_chip_t *self = chip;
    const uint8_t element[1] = {(uint8_t)(inputs[0] * 10.0)};

    if (count < 1u) {
        return false;
    }
    self->last_input = inputs[0];
    (void)self->host->push(self->host->context, 0u, element, sizeof(element));
    self->host->set_line(self->host->context, 0u, false);
    return true;
}

static const looper_device_ops_t demo_ops = {
    .register_read = demo_read,
    .register_write = demo_write,
    .interval_us = demo_interval_us,
    .sample = demo_sample,
    .attached = demo_attached,
};

/* A host as a test plays it: it keeps the last line level and the last element. */
typedef struct {
    bool line;
    uint8_t element;
} demo_host_state_t;

static void host_set_line(void *context, uint8_t line, bool high)
{
    (void)line;
    ((demo_host_state_t *)context)->line = high;
}

static bool host_push(void *context, uint8_t stream, const uint8_t *element, size_t width)
{
    (void)stream;
    ((demo_host_state_t *)context)->element = (width > 0u) ? element[0] : 0u;
    return true;
}

static uint64_t host_now_us(void *context)
{
    (void)context;
    return 0u;
}

static const looper_spi_frame_t demo_spi_frame = {
    .read_mask = 0x80u, .read_value = 0x80u, .address_mask = 0x7Fu,
    .header_length = 1u, .auto_increment = true,
};

static const looper_i2c_frame_t demo_i2c_frame = {.pointer_length = 1u, .auto_increment = true};

static uint64_t demo_step_us;

static const looper_time_param_spec_t demo_parameters[1] = {
    {.name = "step_us", .description = "The step after a pass without an effect.",
     .default_value = 100u, .minimum = 1u, .maximum = LOOPER_TIME_STEP_LIMIT_MICROSECONDS},
};

static void demo_begin(const looper_time_params_t *params)
{
    demo_step_us = (params->count > 0u) ? params->values[0] : 100u;
}

static uint64_t demo_after_pass(const looper_pass_facts_t *pass)
{
    return pass->had_effect ? 1000u : demo_step_us * 1000u;
}

static const looper_time_model_t demo_time_model = {
    .abi_version = LOOPER_CONTRACT_ABI,
    .name = "demo",
    .description = "1 us after an effect, step_us otherwise.",
    .parameters = demo_parameters,
    .parameter_count = 1u,
    .begin = demo_begin,
    .after_pass = demo_after_pass,
    .time_read_cost = NULL,
};

static int time_model_failures(void)
{
    const uint64_t values[1] = {250u};
    const looper_time_params_t params = {.pass_us = 100u, .count = 1u, .values = values};
    const looper_pass_facts_t quiet = {.had_effect = false, .time_reads = 0u, .next_due_ns = LOOPER_TIME_NOTHING_DUE};
    const looper_pass_facts_t busy = {.had_effect = true, .time_reads = 3u, .next_due_ns = 5000u};
    int failures = 0;

    demo_time_model.begin(&params);
    failures += (demo_time_model.after_pass(&quiet) == 250000u) ? 0 : 1;
    failures += (demo_time_model.after_pass(&busy) == 1000u) ? 0 : 1;
    failures += (demo_time_model.parameter_count <= LOOPER_TIME_MODEL_PARAMETERS_MAX) ? 0 : 1;
    return failures;
}

int main(void)
{
    demo_chip_t chip = {{0u}, 0.0, NULL};
    const double inputs[1] = {2.5};
    demo_host_state_t state = {false, 0u};
    const looper_device_host_t host = {&state, host_set_line, host_push, host_now_us};
    int failures = 0;

    demo_ops.attached(&chip, &host);
    failures += state.line ? 0 : 1;

    demo_ops.register_write(&chip, 2u, 0x5Au);
    failures += (demo_ops.register_read(&chip, 2u) == 0x5Au) ? 0 : 1;
    failures += (demo_ops.interval_us(&chip) == 1000u) ? 0 : 1;
    failures += demo_ops.sample(&chip, inputs, 1u) ? 0 : 1;
    failures += (chip.last_input == 2.5) ? 0 : 1;
    failures += (!state.line && state.element == 25u) ? 0 : 1;
    failures += (demo_spi_frame.header_length == 1u) && (demo_i2c_frame.pointer_length == 1u) ? 0 : 1;
    failures += (LOOPER_DEVICE_INPUT_CAPACITY >= 1u) ? 0 : 1;
    failures += time_model_failures();
    printf("contract_c: %d failed\n", failures);
    return (failures == 0) ? 0 : 1;
}
