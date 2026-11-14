# looper-rt-contracts

The contracts between the looper runtime and the models it runs -- chip
models and time models -- as C headers and nothing else.

A chip model is the device and nothing around it: a register file, what
reading or writing a register does, and -- for a converter -- how often it
samples and what a sample makes of its inputs. How the firmware reaches the
registers over SPI or I2C, when a sample is taken and where its inputs come
from is the runtime's part. The runtime binds any model that implements this
contract; a component repository therefore needs no glue code of its own.

A time model decides how far simulated time moves after a pass of the
firmware's loop and what a reading of the microsecond clock costs. It is pure
arithmetic over what the runtime tells it.

The headers declare no function and no object. A component builds and is
tested against them alone, without the runtime.

## What is in it

`include/looper_contract.h`: `LOOPER_CONTRACT_ABI`, the version of all of
them.

`include/looper_device.h`, a chip model:

- `looper_device_ops_t` -- read and write a register (with whatever the chip
  does on a read, such as a status register that clears), the interval
  between samples, taking a sample of the inputs handed in, and `attached`
  (v0.2.0), called with the host when the runtime binds the model, and
  `requested` (v0.3.0), called when the board's request paces a stream.
- `looper_spi_frame_t`, `looper_i2c_frame_t` -- how a part frames an access,
  as data a component ships beside its model.
- `looper_device_host_t` -- what the runtime hands a model when it binds it:
  `set_line` for an output line (INT, DRDY, ALERT), `push` for an element of
  a receiving DMA stream, `pull` (v0.3.0) for one of a transmitting stream,
  `now_us` for the simulated time. The model keeps it from
  `attached` and calls it when the chip acts; the board decides which pin a
  line drives and which DMA channel a stream fills. Lines and streams are
  numbered by the chip from zero (a chip has at most
  `LOOPER_DEVICE_LINE_CAPACITY`, 8, lines and `LOOPER_DEVICE_STREAM_CAPACITY`, 4,
  streams), and a line or stream the board did not wire goes nowhere. The model
  sets its lines to their reset levels in `attached`, because a pin wired later
  takes the level its line has then. `push` says whether anything took the
  element: it answers `false` when the stream is not wired, the channel does
  not run or its width differs.

`include/looper_time_model_types.h`, a time model:

- `looper_time_model_t` -- its name, its parameters, what it takes before the
  firmware starts, the step after a pass and the cost of a clock read.
- `looper_time_param_spec_t`, `looper_time_params_t`, `looper_pass_facts_t`
  and the limits the runtime holds every model to.

## Using it

```cmake
include(FetchContent)
FetchContent_Declare(looper_rt_contracts
    GIT_REPOSITORY https://github.com/donsprallo/looper-rt-contracts.git
    GIT_TAG        v0.3.0)
FetchContent_MakeAvailable(looper_rt_contracts)

target_link_libraries(my_chip PUBLIC looper::device)
```

A chip model then implements the operations:

```c
#include "looper_device.h"

_Static_assert(LOOPER_CONTRACT_ABI == 1u, "written against version 1 of the contract");

const looper_device_ops_t my_chip_ops = {
    .register_read = my_chip_read,
    .register_write = my_chip_write,
    .interval_us = my_chip_interval_us,
    .sample = my_chip_sample,
};

const looper_spi_frame_t MY_CHIP_FRAME = {
    .read_mask = 0x80u, .read_value = 0x80u, .address_mask = 0x7Fu,
    .header_length = 1u, .auto_increment = true,
};
```

A chip that acts by itself keeps the host it is given and calls it:

```c
static void my_chip_attached(void *chip, const looper_device_host_t *host)
{
    struct my_chip *self = chip;

    self->host = host;
    host->set_line(host->context, MY_CHIP_LINE_INT, true); /* active low: idle high */
}

static bool my_chip_sample(void *chip, const double *inputs, size_t count)
{
    struct my_chip *self = chip;
    const uint8_t element[2] = {self->counts & 0xFFu, self->counts >> 8u};

    /* ... convert inputs ... */
    self->host->push(self->host->context, MY_CHIP_STREAM_SAMPLES, element, sizeof(element));
    self->host->set_line(self->host->context, MY_CHIP_LINE_INT, false);
    return true;
}
```

A time model fills a `looper_time_model_t`:

```c
#include "looper_time_model_types.h"

static uint64_t my_after_pass(const looper_pass_facts_t *pass)
{
    return pass->had_effect ? 1000u : 100000u; /* 1 us after an effect, else 100 us */
}

const looper_time_model_t my_time_model = {
    .abi_version = LOOPER_CONTRACT_ABI,
    .name = "my_time",
    .description = "1 us after an effect, 100 us otherwise.",
    .after_pass = my_after_pass,
};
```

Offering it to a simulation is the runtime's part. The project registers it
in a file of its own, named under `time_models` in its `looper.toml`:

```c
#include "looper_time_model.h"

extern const looper_time_model_t my_time_model;
LOOPER_TIME_MODEL_REGISTER(my_time_model)
```

Give your own names your own prefix: `looper_` belongs to the runtime and the
components it ships.

## Versions

One number, `LOOPER_CONTRACT_ABI`, covers every header: models are compiled
from source with every build and come from one tag. The runtime refuses a
time model built against another number.

While the major version is 0 the contracts are still being shaped and may
change with any minor version. From 1.0 on, `LOOPER_CONTRACT_ABI` rises when
a member changes its meaning or its signature and the major version is that
number; adding something an existing model does not notice is a minor
version.

v0.2.0 adds `looper_device_host_t`, the member `attached` at the end of
`looper_device_ops_t`, `LOOPER_DEVICE_LINE_CAPACITY` and
`LOOPER_DEVICE_STREAM_CAPACITY`; `LOOPER_CONTRACT_ABI` stays 1. A model written
for v0.1.0 leaves `attached` unset (`NULL`) and works unchanged. The looper-rt
of a setup carries one version of these headers in `rt/runtime/contract/include`:
that is the one a component of a project is built against.

v0.3.0 adds `pull` at the end of `looper_device_host_t`, for a stream the
chip takes from a transmitting DMA channel, and `requested` at the end of
`looper_device_ops_t`, called when the board paces a stream's channel by a
request -- whatever source the project's contract names -- so that the chip
answers with one `push` or one `pull`. A model without transmit streams or
requests leaves `requested` unset; `LOOPER_CONTRACT_ABI` stays 1.

## Checks

```console
$ cmake -S . -B build
$ cmake --build build
$ ctest --test-dir build --output-on-failure
```

They compile the headers as C11 and as C++17 with every warning an error,
write a chip model and a time model against them, and check that they define
no symbol.

## Licence

MIT, see `LICENSE`.
