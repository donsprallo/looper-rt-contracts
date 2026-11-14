/**
 * @file
 * @brief What a chip model offers the runtime, as types and nothing else.
 *
 * A chip model is the device and nothing around it: a register file, what
 * reading or writing a register does, and -- for a converter -- how often it
 * samples and what a sample makes of its inputs. How the firmware reaches the
 * registers, when a sample is taken and where its inputs come from is the
 * runtime's part: its register map and its sampler bind any model that
 * implements this. How a device frames an access is a property of the part,
 * so its description is here, as data a component can ship beside its model.
 *
 * This header declares no function and no object, and it is a repository of
 * its own, looper-rt-contracts, with a CMake target of its own,
 * `looper::device`. So a component kept in a repository of its own can
 * implement it and be built and tested without the runtime. Its version is
 * @ref LOOPER_CONTRACT_ABI in `looper_contract.h`, which tells such a
 * component that it was written against the contract the runtime binds it
 * with.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef LOOPER_DEVICE_H
#define LOOPER_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Most inputs one sample of a chip model can take. */
#define LOOPER_DEVICE_INPUT_CAPACITY 8u

/**
 * @brief What a chip model implements.
 *
 * `chip` is the model's own state, owned by whoever attaches it. A device
 * with no conversion leaves `interval_us` and `sample` NULL.
 */
typedef struct {
    /**
     * @brief Read one register, with whatever reading it does on the chip.
     *
     * Side effects belong here, not in the binding: a status register that
     * clears on read clears because the chip says so, whichever bus asked.
     * An address the chip does not have reads as zero.
     */
    uint8_t (*register_read)(void *chip, uint16_t address);

    /**
     * @brief Write one register, with whatever writing it does on the chip.
     *
     * An address the chip does not have is ignored.
     */
    void (*register_write)(void *chip, uint16_t address, uint8_t value);

    /**
     * @brief Microseconds from one sample to the next, as the registers say.
     *
     * Zero means the chip does not convert at the moment, for example
     * because the firmware has not started it.
     */
    uint64_t (*interval_us)(const void *chip);

    /**
     * @brief Take one sample of the given inputs.
     *
     * The inputs are what the chip sees, in the chip's own order and units.
     * They are handed in rather than fetched, which is what keeps the model
     * free of the runtime.
     *
     * @retval true The chip reports a new sample, so its ready line goes.
     */
    bool (*sample)(void *chip, const double *inputs, size_t count);
} looper_device_ops_t;

/**
 * @brief How a device frames an SPI transfer.
 *
 * The header comes first, then one data byte per register. A transfer whose
 * header is shorter than `header_length`, or whose select bits do not match,
 * is not answered: that is a device ignoring a command not meant for it.
 *
 * The AS8510: `{.read_mask = 0x80, .read_value = 0x80, .address_mask = 0x7F,
 * .header_length = 1, .auto_increment = true}`. The MCP23S08: an opcode
 * `0100 0 A1 A0 R/W` and then the register, so `{.select_mask = 0xF8,
 * .select_value = 0x40, .read_mask = 0x01, .read_value = 0x01,
 * .address_byte = 1, .address_mask = 0xFF, .header_length = 2,
 * .auto_increment = true}`.
 */
typedef struct {
    /** @brief Bits of the first byte that must match `select_value`; zero for none. */
    uint8_t select_mask;
    /** @brief What those bits are for this device. */
    uint8_t select_value;
    /** @brief Bits of the first byte that say read or write. */
    uint8_t read_mask;
    /** @brief What those bits are on a read; anything else is a write. */
    uint8_t read_value;
    /** @brief Which header byte carries the register address. */
    uint8_t address_byte;
    /** @brief The bits of that byte that are the address. */
    uint8_t address_mask;
    /** @brief How far the address sits above bit zero within those bits. */
    uint8_t address_shift;
    /** @brief Bytes before the data; at least one, at most four. */
    uint8_t header_length;
    /**
     * @brief Whether each data byte addresses the register after the last.
     *
     * The address wraps within `address_mask`, as a device's counter does.
     * When false every data byte addresses the same register.
     */
    bool auto_increment;
} looper_spi_frame_t;

/**
 * @brief How a device frames an I2C transaction.
 *
 * The first bytes written are the register pointer, high byte first; the
 * rest of a write is stored from there on. A read that writes nothing
 * carries on from where the pointer stands, and a read before any pointer
 * was ever set is not acknowledged.
 */
typedef struct {
    /** @brief Bytes of the register pointer: one or two. */
    uint8_t pointer_length;
    /** @brief Whether the pointer advances across a burst. */
    bool auto_increment;
} looper_i2c_frame_t;

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_DEVICE_H */
