// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/common/types.h>
#include <noir/p2p/conn/merlin.h>
#include <noir/p2p/conn/secret_connection.h>

#include <cppcodec/base64_default_rfc4648.hpp>

extern "C" {
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <sodium.h>
}

using namespace noir;

TEST_CASE("secret_connection: derive_secrets", "[noir][p2p]") {
  auto priv_key_str =
    base64::decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  Bytes loc_priv_key(priv_key_str.begin(), priv_key_str.end());
  auto c = p2p::secret_connection::make_secret_connection(loc_priv_key);

  auto tests = std::to_array<std::tuple<std::string, std::string, std::string, std::string>>({
    {"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03",
      "80a83ad6afcb6f8175192e41973aed31dd75e3c106f813d986d9567a4865eb2f",
      "96362a04f628a0666d9866147326898bb0847b8db8680263ad19e6336d4eed9e",
      "2632c3fd20f456c5383ed16aa1d56dc7875a2b0fc0d5ff053c3ada8934098c69"},
    {"0716764b370d543fee692af03832c16410f0a56e4ddb79604ea093b10bb6f654",
      "cba357ae33d7234520d5742102a2a6cdb39b7db59c14a58fa8aadd310127630f",
      "84f2b1e8658456529a2c324f46c3406c3c6fecd5fbbf9169f60bed8956a8b03d",
      "576643a8fcc1a4cf866db900f4a150dbe35d44a1b3ff36e4911565c3fa22fc32"},
    {"6104474c791cda24d952b356fb41a5d273c0ce6cc87d270b1701d0523cd5aa13",
      "1cb4397b9e478430321af4647da2ccbef62ff8888542d31cca3f626766c8080f",
      "673b23318826bd31ad1a4995c6e5095c4b092f5598aa0a96381a3e977bc0eaf9",
      "4a25a25c5f75d6cc512f2ba8c1546e6263e9ef8269f0c046c37838cc66aa83e6"},
  });
  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    Bytes32 dh_secret{from_hex(std::get<0>(t))};
    auto key = c->derive_secrets(dh_secret);
    CHECK(to_hex(std::span(key.data(), 32)) == std::get<1>(t));
    CHECK(to_hex(std::span(key.data() + 32, 32)) == std::get<2>(t));
    CHECK(to_hex(std::span(key.data() + 64, 32)) == std::get<3>(t));
  });
}

TEST_CASE("secret_connection: verify key exchanges", "[noir][p2p]") {
  auto priv_key_str_peer1 =
    base64::decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  Bytes loc_priv_key_peer1(priv_key_str_peer1.begin(), priv_key_str_peer1.end());
  auto c_peer1 = p2p::secret_connection::make_secret_connection(loc_priv_key_peer1);

  auto priv_key_str_peer2 =
    base64::decode("x1eX2WKe+mhZwO7PLVgLdMZ4Ucr4NfdBxMtD/59mOfmk8GO0T1p8YNpObegcTLZmqnK6ffVtjvWjDSSVgVwGAw==");
  Bytes loc_priv_key_peer2(priv_key_str_peer2.begin(), priv_key_str_peer2.end());
  auto c_peer2 = p2p::secret_connection::make_secret_connection(loc_priv_key_peer2);

  Bytes32 eph_pub_key_peer1 = c_peer1->loc_eph_pub;
  Bytes32 eph_pub_key_peer2 = c_peer2->loc_eph_pub;
  c_peer1->shared_eph_pub_key(eph_pub_key_peer2);
  c_peer2->shared_eph_pub_key(eph_pub_key_peer1);
  CHECK(c_peer1->send_secret == c_peer2->recv_secret);
  CHECK(c_peer1->chal_secret == c_peer2->chal_secret);

  auto auth_sig_msg1 = p2p::auth_sig_message{c_peer1->loc_pub_key, c_peer1->loc_signature};
  auto auth_sig_msg2 = p2p::auth_sig_message{c_peer2->loc_pub_key, c_peer2->loc_signature};
  CHECK(!c_peer1->shared_auth_sig(auth_sig_msg2).has_value());
  CHECK(!c_peer2->shared_auth_sig(auth_sig_msg1).has_value());
}

std::string crypto_box_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  crypto_scalarmult_curve25519_base(public_key, secret_key);
  auto enc_pub_key = base64::encode(public_key, crypto_sign_PUBLICKEYBYTES);
  return enc_pub_key;
}

std::string crypto_sign_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  memcpy(public_key, secret_key + crypto_sign_PUBLICKEYBYTES, crypto_sign_PUBLICKEYBYTES);
  auto enc_pub_key = base64::encode(public_key, crypto_sign_PUBLICKEYBYTES);
  return enc_pub_key;
}

TEST_CASE("secret_connection: libsodium - key gen : crypto_box", "[noir][p2p]") {
  uint8_t public_key[crypto_box_PUBLICKEYBYTES];
  uint8_t secret_key[crypto_box_SECRETKEYBYTES];
  crypto_box_keypair(public_key, secret_key);
  auto rec_pub_key = crypto_box_recover_public_key(secret_key);
  CHECK(rec_pub_key == base64::encode(public_key, crypto_box_PUBLICKEYBYTES));
}

TEST_CASE("secret_connection: libsodium - key gen : crypto_sign", "[noir][p2p]") {
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  crypto_sign_keypair(pk, sk);
  auto rec_pub_key = crypto_sign_recover_public_key(sk);
  CHECK(rec_pub_key == base64::encode(pk, crypto_sign_PUBLICKEYBYTES));
}

TEST_CASE("secret_connection: libsodium - derive pub_key from priv_key", "[noir][p2p]") {
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  auto priv_key =
    base64::decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  Bytes raw(priv_key.begin(), priv_key.end());
  auto rec_pub_key = crypto_sign_recover_public_key(raw.data());
  CHECK(rec_pub_key == "UXs1p10Mp60h62I+tNg7eruoGu0+VHNaYnp8O2+SQG0=");
}

// TEST_CASE("secret_connection: derive address from pub_key", "[noir][p2p]") {
//  auto pub_key = fc::base64_decode("UXs1p10Mp60h62I+tNg7eruoGu0+VHNaYnp8O2+SQG0=");
//  auto h = crypto::Sha256()(pub_key);
//  noir::bytes address = bytes(h.begin(), h.begin() + 20);
//  CHECK(to_hex(address) == "cbc837aced724b22dc0bff1821cdbdd96164d637");
// }

int sign(uint8_t sm[], const uint8_t m[], const int mlen, const uint8_t sk[]) {
  unsigned long long smlen;
  if (crypto_sign(sm, &smlen, m, mlen, sk) == 0) {
    return smlen;
  } else {
    return -1;
  }
}

int verify(uint8_t m[], const uint8_t sm[], const int smlen, const uint8_t pk[]) {
  unsigned long long mlen;
  if (crypto_sign_open(m, &mlen, sm, smlen, pk) == 0) {
    return mlen;
  } else {
    return -1;
  }
}

constexpr int MAX_MSG_LEN = 64;
TEST_CASE("secret_connection: libsodium - sign", "[noir][p2p]") {
  uint8_t sk[crypto_sign_SECRETKEYBYTES];
  uint8_t pk[crypto_sign_PUBLICKEYBYTES];
  uint8_t sm[MAX_MSG_LEN + crypto_sign_BYTES];
  uint8_t m[MAX_MSG_LEN + crypto_sign_BYTES + 1];

  memset(m, '\0', MAX_MSG_LEN);
  std::string msg{"Hello World"};
  strcpy((char*)m, msg.data());
  int mlen = msg.size();

  int rc = crypto_sign_keypair(pk, sk);
  CHECK(rc >= 0);
  int smlen = sign(sm, m, mlen, sk);
  CHECK(smlen >= 0);
  mlen = verify(m, sm, smlen, pk);
  CHECK(mlen >= 0);
}

TEST_CASE("secret_connection: openssl - hkdf", "[noir][p2p]") {
  EVP_KDF* kdf;
  EVP_KDF_CTX* kctx;
  static char const* digest = "SHA256";
  unsigned char key[32 + 32 + 32];
  OSSL_PARAM params[5], *p = params;
  kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
  kctx = EVP_KDF_CTX_new(kdf);
  EVP_KDF_free(kdf);

  *p++ = OSSL_PARAM_construct_utf8_string("digest", (char*)digest, strlen(digest));
  Bytes32 secret{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  *p++ = OSSL_PARAM_construct_octet_string("key", (void*)secret.data(), secret.size());
  *p++ = OSSL_PARAM_construct_octet_string("info", (void*)"TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN",
    strlen("TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN"));
  *p = OSSL_PARAM_construct_end();
  if (EVP_KDF_derive(kctx, key, sizeof(key), params) <= 0) {
    std::cout << "Error: "
              << "EVP_KDF_derive" << std::endl;
  }
  EVP_KDF_CTX_free(kctx);
  // std::cout << "key = " << fc::base64_encode(key, sizeof(key)) << std::endl;
  // std::cout << "key1 = " << fc::base64_encode(key, 32) << std::endl;
  // std::cout << "key2 = " << fc::base64_encode(key + 32, 32) << std::endl;
  // std::cout << "challenge = " << fc::base64_encode(key + 64, 32) << std::endl;

  // std::cout << "key1_hex = " << to_hex(std::span((const byte_type*)key, 32)) << std::endl;
  // std::cout << "key2_hex = " << to_hex(std::span((const byte_type*)(key+32), 32)) << std::endl;
  // std::cout << "challenge_hex = " << to_hex(std::span((const byte_type*)(key+64), 32)) << std::endl;
  CHECK(to_hex(std::span(key, 32)) == "80a83ad6afcb6f8175192e41973aed31dd75e3c106f813d986d9567a4865eb2f");
  CHECK(to_hex(std::span(key + 32, 32)) == "96362a04f628a0666d9866147326898bb0847b8db8680263ad19e6336d4eed9e");
  CHECK(to_hex(std::span(key + 64, 32)) == "2632c3fd20f456c5383ed16aa1d56dc7875a2b0fc0d5ff053c3ada8934098c69");
}

TEST_CASE("secret_connection: libsodium - chacha20", "[noir][p2p]") {
  Bytes32 key{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  uint8_t nonce[crypto_stream_chacha20_IETF_NONCEBYTES];
  randombytes_buf(nonce, sizeof nonce);
  std::string msg = "hello";
  unsigned char ciphertext[1024];
  unsigned char decrypted[1024];
  crypto_stream_chacha20_ietf_xor(ciphertext, reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), nonce,
    reinterpret_cast<const unsigned char*>(key.data()));
  crypto_stream_chacha20_ietf_xor(decrypted, reinterpret_cast<const unsigned char*>(ciphertext), msg.size(), nonce,
    reinterpret_cast<const unsigned char*>(key.data()));
  CHECK(std::string(decrypted, decrypted + 5) == msg);
}

TEST_CASE("secret_connection: libsodium - chacha20 with authentication", "[noir][p2p]") {
  Bytes32 key{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  p2p::nonce96 nonce;
  constexpr auto message_size{1028};
  Bytes msg(message_size);
  Bytes ciphertext(message_size + 16);
  unsigned long long ciphertext_len;
  unsigned char decrypted[message_size];
  unsigned long long decrypted_len;

  auto r1 = crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext.data(), &ciphertext_len,
    reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), nullptr, 0, nullptr, nonce.get(),
    reinterpret_cast<const unsigned char*>(key.data()));
  CHECK(r1 >= 0);
  CHECK(ciphertext_len == 1044);
  CHECK(ciphertext ==
    Bytes(
      "cd823f72ba10dc00b9f85218e0fa9837419d4b1f31870390c5f9ebdd18830eff0708bff49edb2ce94bcf4a4d1091cd8d4ad63d060b75a62f"
      "088434479ba2314520556e39de38b8b7d2961bba497b8b7e578fcad41e2257ead76a50e3745dd62669e4cadb6ed4821f77a3603da98f4160"
      "eda6d479c6e430a09fa96bafc189d17c4ad8d402f0caea282ae12404b8a0eacb2f13dd116e7ed08d902e217e704f1719e63e8466b18edafc"
      "3f87486c71ae61e6a97a883f9be9d9f20571b8756d1bfe27998ca589d14cb29bb52f27eba708c30183ac1ce9bcc7b2a16e1cfcd5d43487c9"
      "c71f9934909e73beece76a87a099b820a73ccb6e6da73883dd954ccce456b696ec1ce75a0bd8742824ac71efd0f000c1986ffd744facc536"
      "4962338cd6599bd110fc1812fe04f85a9fd7bd09067e291a2231b30fde2c1fed7ff765a92f25aaa16ab6e3bff0ea2864713a5d497d9ee556"
      "a873de5318268de7ecdc60a07383ea0f2a1423c449d18db4216e9921efbdf2f12eed701e2acb5cd48f83f87f4e3b463a04f175b4bc35b6c0"
      "f6a27df26ced0ec6ed426d99bb9ac446dddd46bd2caf818fc7ec7c8f8efd3919a604e6c54845a681fe1ae277d491a81bfc2857cc34b9b27a"
      "ea482d63e78ae24fd5b42e074c462706de6f03d1b6e410e246a3998ae74820252360f17d09f31ca3b3fc49f52f2a2794cee8551047ebf852"
      "7302e051f74636b8f96d6d541c9ca0bd0a68faa62e008f0c90d7dbf7eeef40b2786fb43426eb89aa46d7af9c5a928a6f4b8213a9466c4f52"
      "97a08f942ffe84e73e012e02f5585f974e705430589084d4d96ce572230405f1a47f1cd0d451a73286681414d516c448781b71c8b259f8e1"
      "e24c294a7309d9a07c99ce091b47640e4a5b3092239566cf846e9d4897dc97273e992feb23f66405c0cac11bf5f1aa957caddc458249d4d6"
      "2f0995adcda69af09a7a9e5b298ae92bd450e5ff1ea95ffacba9dca274c296c2285037db8806b6e256935b79d10ffa50b9e58a0356733e4d"
      "27939c77e0de8cbbdaa62e67007fcfddf36a6e11666361080f384ab76692232ca26bdc3e5c287098597d1b4c56d7f656047f46dc38970368"
      "f4a4b0567ee4583d2999ce342d75e35e8d8425094c702eb87a7b034f05feaca35fa137616c2886f3aa005fca3c15af737d0b3db4a77c889d"
      "dba3631bc6a7921cf4b11ada3df8cc6fd2cd270f110bfd9e57d79d4eccce40d3a64677728b9d47a95f75680d2d3f63c13aa0ebe26d19d2ee"
      "2068b9d99695608ca052ca73eccea0eefa7e77091a8c7b9f17cfab58306aa7c335d30da832278d60a16ae6735c3251c5d9186155c73a42de"
      "016b10214732a62b40abace24f7069ebed846e00a380fc880a270c0e8d1c4f360abc756aff821873d5c820879cbee8ad56f9c5f5dca7750a"
      "4a4769b0e1df4eb851374b515235b95bdf94ff2d08c76122778ac7ee917be6f6bf3d996e")); // matches golang implementation

  auto r2 = crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, &decrypted_len, nullptr, ciphertext.data(),
    ciphertext_len, nullptr, 0, nonce.get(), reinterpret_cast<const unsigned char*>(key.data()));
  CHECK(r2 >= 0);

  CHECK(std::string(decrypted, decrypted + message_size) == std::string(msg.begin(), msg.end()));
}

TEST_CASE("secret_connection: merlin transcript", "[noir][p2p]") {
  p2p::merlin_transcript mctx{};
  static char const* init = "TEST";
  static char const* item = "TEST";
  static char const* challenge = "challenge";
  Bytes32 challenge_buf;
  merlin_transcript_init(&mctx, reinterpret_cast<const uint8_t*>(init), strlen(init));
  merlin_transcript_commit_bytes(
    &mctx, reinterpret_cast<const uint8_t*>(item), strlen(item), reinterpret_cast<const uint8_t*>(item), strlen(item));
  merlin_transcript_challenge_bytes(&mctx, reinterpret_cast<const uint8_t*>(challenge), strlen(challenge),
    reinterpret_cast<uint8_t*>(challenge_buf.data()), challenge_buf.size());
  CHECK(to_hex(challenge_buf) == "c8cc8d7b4b3320f6a7a813c480d8f1ebd9cfb6873417eb69a44b4ed91b27af10");
}

TEST_CASE("secret_connection: write and read", "[noir][p2p]") {
  auto priv_key_str =
    base64::decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  Bytes loc_priv_key(priv_key_str.begin(), priv_key_str.end());
  auto c = p2p::secret_connection::make_secret_connection(loc_priv_key);
  c->send_secret = Bytes32{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  c->recv_secret = Bytes32{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};

  SECTION("simple") {
    Bytes bz("abcd");
    auto w_ok = c->write(bz);
    auto w_buff = *w_ok.value().second[0];
    // std::cout << to_hex(w_buff) << std::endl;

    auto r_ok = c->read(w_buff);
    auto r_buff = r_ok.value().second;
    // std::cout << to_hex(r_buff) << std::endl;
    CHECK(r_buff == bz);
  }

  SECTION("big") {
    uint8_t buff[1000];
    randombytes_buf(buff, sizeof(buff));
    Bytes bz(1000);
    std::memcpy(bz.data(), buff, sizeof(buff));
    auto w_ok = c->write(bz);
    auto w_buff = *w_ok.value().second[0];

    auto r_ok = c->read(w_buff);
    auto r_buff = r_ok.value().second;
    CHECK(r_buff == bz);
  }

  SECTION("bigger") {
    size_t data_len = 3000;
    uint8_t buff[data_len];
    randombytes_buf(buff, sizeof(buff));
    Bytes bz(data_len);
    std::memcpy(bz.data(), buff, sizeof(buff));
    auto w_ok = c->write(bz);
    auto w_buffs = w_ok.value().second;
    Bytes restored(data_len);
    int i{};
    for (auto& b : w_buffs) {
      auto r_ok = c->read(*b);
      auto r_buff = r_ok.value().second;
      std::copy(r_buff.begin(), r_buff.end(), restored.begin() + i);
      i += r_ok->first;
      // std::cout << i << " " << r_ok->first <<std::endl;
    }
    CHECK(restored == bz);
  }
}

Result<Bytes> sign_ed25519(const Bytes& msg, const Bytes& key) {
  Bytes sig(64);
  if (crypto_sign_detached(reinterpret_cast<unsigned char*>(sig.data()), nullptr,
        reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
        reinterpret_cast<const unsigned char*>(key.data())) == 0)
    return sig;
  return Error::format("unable to sign");
}

bool verify_ed25519(const Bytes& sig, const Bytes& msg, const Bytes& key) {
  if (crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(sig.data()),
        reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
        reinterpret_cast<const unsigned char*>(key.data())) == 0)
    return true;
  return false;
}

TEST_CASE("secret_connection: libsodium - ed25519", "[noir][p2p]") {
  auto priv_key_str =
    base64::decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  Bytes loc_priv_key(priv_key_str.begin(), priv_key_str.end());
  auto c = p2p::secret_connection::make_secret_connection(loc_priv_key);

  Bytes org("abcd");

  auto r_sign = sign_ed25519(org, c->loc_priv_key);
  CHECK(r_sign);
  auto sig = r_sign.value();
  auto r = verify_ed25519(sig, org, c->loc_pub_key);
  CHECK(r);
}
