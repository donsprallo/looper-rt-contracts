/**
 * @file
 * @brief The version of the contract every header of looper-rt-contracts belongs to.
 *
 * One number for all of them: chip models and time models are compiled from
 * source with every build and come from one tag of this repository, so a
 * number per header would protect nothing a consistent tag does not. The
 * runtime refuses a time model whose `abi_version` is another; a chip model
 * can say what it was written for with
 * `_Static_assert(LOOPER_CONTRACT_ABI == 1u, ...)`.
 *
 * It rises when a member changes its meaning or its signature. While the
 * repository's major version is 0 the contract is still being shaped and
 * may change with any minor version; from 1.0 on, the major version is this
 * number.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef LOOPER_CONTRACT_H
#define LOOPER_CONTRACT_H

/** @brief The version of the contract. */
#define LOOPER_CONTRACT_ABI 1u

#endif /* LOOPER_CONTRACT_H */
