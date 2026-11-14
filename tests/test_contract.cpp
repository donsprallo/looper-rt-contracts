/*
 * The contract from C++17: a model in C++ fills the same operations, with C
 * linkage where the runtime calls them.
 */
#include <cstdio>

#include "looper_device.h"
#include "looper_time_model_types.h"

static_assert(LOOPER_CONTRACT_ABI == 1u, "written against version 1 of the contract");

namespace {

struct DemoChip {
    uint8_t registers[4];
};

extern "C" uint8_t demo_read(void *chip, uint16_t address)
{
    const auto *self = static_cast<const DemoChip *>(chip);
    return (address < 4u) ? self->registers[address] : 0u;
}

extern "C" void demo_write(void *chip, uint16_t address, uint8_t value)
{
    auto *self = static_cast<DemoChip *>(chip);
    if (address < 4u) {
        self->registers[address] = value;
    }
}

}  // namespace

int main()
{
    looper_device_ops_t ops{};
    looper_spi_frame_t frame{};
    DemoChip chip{};

    ops.register_read = demo_read;
    ops.register_write = demo_write;
    frame.header_length = 2u;
    ops.register_write(&chip, 1u, 0x42u);
    looper_time_model_t time_model{};
    time_model.abi_version = LOOPER_CONTRACT_ABI;
    const bool passed = (ops.register_read(&chip, 1u) == 0x42u) && (ops.interval_us == nullptr)
                        && (frame.header_length == 2u) && (time_model.after_pass == nullptr);
    std::printf("contract_cpp: %d failed\n", passed ? 0 : 1);
    return passed ? 0 : 1;
}
