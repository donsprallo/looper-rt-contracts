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
 * A chip also acts on the board by itself: it pulls an interrupt line, it
 * hands data to a DMA channel or takes data from one. For that the runtime
 * gives the model a
 * @ref looper_device_host_t when it binds it -- functions to call, so that
 * the model still knows nothing of the runtime. Which pin a line drives, and
 * which DMA channel a stream fills, the board decides when it wires the chip.
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

/** @brief Most output lines one chip model may have: interrupt, ready, alert. */
#define LOOPER_DEVICE_LINE_CAPACITY 8u

/** @brief Most DMA streams one chip model may feed. */
#define LOOPER_DEVICE_STREAM_CAPACITY 4u

/**
 * @brief What the runtime offers a chip model, handed to it when it is bound.
 *
 * Every function takes `context` first, which the model passes back as it
 * was given. Lines and streams are numbered by the chip, from zero, as its
 * own header says (an MCP23008's INT is its line 0); a line or a stream the
 * board did not wire is accepted and goes nowhere, as a pin that is not
 * connected.
 */
typedef struct {
    /** @brief Passed back to every function below. */
    void *context;

    /**
     * @brief Set an output line of the chip to a level, as its pin would be.
     *
     * The level is the electrical one: an active-low INT that asserts goes
     * low. Wired to a pin of the microcontroller, the pin follows, and an
     * interrupt on that pin's edge becomes due as on the board.
     */
    void (*set_line)(void *context, uint8_t line, bool high);

    /**
     * @brief Hand one element of a stream to the DMA channel wired to it.
     *
     * `width` bytes, little-endian, as wide as the channel's elements. The
     * element goes into the firmware's buffer at the simulated time of the
     * call, and half, complete and idle follow as for any channel.
     *
     * @retval true The channel runs and took the element.
     * @retval false Nothing took it: not wired, not started, or another width.
     */
    bool (*push)(void *context, uint8_t stream, const uint8_t *element, size_t width);

    /** @brief The simulated time, in microseconds. */
    uint64_t (*now_us)(void *context);

    /**
     * @brief Take the next element of a stream from the DMA channel wired to it.
     *
     * The other way round from `push`: the channel transmits from the
     * firmware's buffer, and `element` receives `width` bytes, little-endian,
     * at the simulated time of the call. Half and complete follow as for any
     * channel, so a chip that pulls on its own clock -- a DAC, a codec --
     * paces the transfer as the part would.
     *
     * @retval true The channel runs and handed over an element.
     * @retval false Nothing to take: not wired, not started, ended, or another width.
     */
    bool (*pull)(void *context, uint8_t stream, uint8_t *element, size_t width);
} looper_device_host_t;

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

    /**
     * @brief The chip is bound: keep `host` to act on the board later. May be NULL.
     *
     * `host` lives as long as the binding does. The model sets its lines to
     * their reset levels here, since a pin wired later takes the level the
     * line has then.
     */
    void (*attached)(void *chip, const looper_device_host_t *host);

    /**
     * @brief The DMA channel wired to a stream was requested: move one element now. May be NULL.
     *
     * A channel the board paces by a request -- a timer, a pin, a line of this
     * or another chip, the end of another channel; whatever the project's
     * contract names -- does not wait for the chip. At each request the
     * runtime calls this, and the chip answers with one `push` (a receiving
     * channel) or one `pull` (a transmitting one) on that stream, as a
     * peripheral answers a DMA request. Which request it is the chip does not
     * know and need not: the same model serves any microcontroller.
     */
    void (*requested)(void *chip, uint8_t stream);
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
