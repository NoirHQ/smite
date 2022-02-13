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

namespace unsafe {
  void blake2b_256(std::span<const char> in, std::span<char> out);
  void keccak256(std::span<const char> in, std::span<char> out);
  void ripemd160(std::span<const char> in, std::span<char> out);
  void sha256(std::span<const char> in, std::span<char> out);
} // namespace unsafe

/// \addtogroup crypto
/// \{

/// \brief calculates and stores the blake2b_256 hash value of input data to output buffer
/// \param in input data
/// \param out output buffer
void blake2b_256(std::span<const char> in, std::span<char> out);

/// \brief calculates and returns the blake2b_256 hash value of input data
/// \param in input data
std::vector<char> blake2b_256(std::span<const char> in);

/// \brief calculates and stores the keccak256 hash value of input data to output buffer
/// \param in input data
/// \param out output buffer
void keccak256(std::span<const char> in, std::span<char> out);

/// \brief calculates and returns the keccak256 hash value of input data
/// \param in input data
std::vector<char> keccak256(std::span<const char> in);

/// \brief calculates and stores the ripemd160 hash value of input data to output buffer
/// \param in input data
/// \param out output buffer
void ripemd160(std::span<const char> in, std::span<char> out);

/// \brief calculates and returns the ripemd160 hash value of input data
/// \param in input data
std::vector<char> ripemd160(std::span<const char> in);

/// \brief calculates and stores the sha256 hash value of input data to output buffer
/// \param in input data
/// \param out output buffer
void sha256(std::span<const char> in, std::span<char> out);

/// \brief calculates and returns the sha256 hash value of input data
/// \param in input data
std::vector<char> sha256(std::span<const char> in);

/// \}

} // namespace noir::crypto
