// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <span>
#include <vector>

/// \defgroup crypto Crypto
/// \brief Cryptography

/// \brief crypto namespace
/// \ingroup crypto
namespace noir::crypto {

/// \addtogroup crypto
/// \{

/// \brief calculates and stores the keccak256 hash value of input data to output buffer
/// \param input input data
/// \param output output buffer
void keccak256(std::span<const char> input, std::span<char> output);

/// \brief calculates and returns the keccak256 hash value of input data
/// \param input input data
std::vector<char> keccak256(std::span<const char> input);

/// \brief calculates and stores the sha256 hash value of input data to output buffer
/// \param input input data
/// \param output output buffer
void sha256(std::span<const char> input, std::span<char> output);

/// \brief calculates and returns the sha256 hash value of input data
/// \param input input data
std::vector<char> sha256(std::span<const char> input);

/// \}

} // namespace noir::crypto