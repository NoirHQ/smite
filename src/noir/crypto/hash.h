// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <span>
#include <vector>

/// \defgroup crypto Crypto
/// \brief Cryptography

/// \brief crypto namespace
/// \ingroup crypto
namespace noir::crypto {

struct hash {
  /// \brief calculates and stores the hash value of input data to output buffer
  /// \param in input data
  /// \param out output buffer
  void operator()(std::span<const char> in, std::span<char> out) {
    init().update(in).final(out);
  }

  /// \brief calculates and returns the hash value of input data
  /// \param in input data
  /// \return byte array containing hash
  std::vector<char> operator()(std::span<const char> in) {
    return init().update(in).final();
  }

  /// \brief resets hash object context
  virtual hash& init() = 0;

  /// \brief updates the hash object with byte array
  virtual hash& update(std::span<const char> in) = 0;

  /// \brief stores hash value to output buffer
  virtual void final(std::span<char> out) = 0;

  /// \brief returns hash value
  /// \return byte array containing hash
  std::vector<char> final() {
    std::vector<char> out(digest_size());
    final(out);
    return out;
  }

  /// \brief returns the size of hash value
  virtual size_t digest_size() = 0;
};

namespace unsafe {
  void blake2b_256(std::span<const char> in, std::span<char> out);
  void keccak256(std::span<const char> in, std::span<char> out);
  void ripemd160(std::span<const char> in, std::span<char> out);
  void sha256(std::span<const char> in, std::span<char> out);
} // namespace unsafe

/// \addtogroup crypto
/// \{

/// \breif generates blake2b_256 hash
struct blake2b_256 : public hash {
  blake2b_256();
  ~blake2b_256();

  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class blake2b_256_impl;
  std::unique_ptr<blake2b_256_impl> impl;
};

/// \breif generates keccak256 hash
struct keccak256 : public hash {
  keccak256();
  ~keccak256();

  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class keccak256_impl;
  std::unique_ptr<keccak256_impl> impl;
};

/// \breif generates ripemd160 hash
struct ripemd160 : public hash {
  ripemd160();
  ~ripemd160();

  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class ripemd160_impl;
  std::unique_ptr<ripemd160_impl> impl;
};

/// \breif generates sha256 hash
struct sha256 : public hash {
  sha256();
  ~sha256();

  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class sha256_impl;
  std::unique_ptr<sha256_impl> impl;
};

/// \}

} // namespace noir::crypto

#define NOIR_CRYPTO_HASH(name) \
  namespace noir::crypto { \
    name::name(): impl(new name##_impl) {} \
    name::~name() = default; \
    hash& name::init() { \
      impl->init(); \
      return *this; \
    } \
    hash& name::update(std::span<const char> in) { \
      impl->update(in); \
      return *this; \
    } \
    void name::final(std::span<char> out) { \
      check(digest_size() <= out.size(), "insufficient output buffer"); \
      impl->final(out); \
    } \
    size_t name::digest_size() { \
      return impl->digest_size(); \
    } \
  }
