#include "common/crypto/utility.h"

#include "common/common/assert.h"
#include "common/common/stack_array.h"

#include "extensions/common/crypto/crypto_impl.h"

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "openssl/evp.h"

#include "boringssl_compat/cbs.h"

namespace Envoy {
namespace Common {
namespace Crypto {

namespace Utility {

const EVP_MD* getHashFunction(absl::string_view name);

const VerificationOutput verifySignature(absl::string_view hash, CryptoObject& key,
                                         const std::vector<uint8_t>& signature,
                                         const std::vector<uint8_t>& text) {
  // Step 1: initialize EVP_MD_CTX
  EVP_MD_CTX *ctx;
  ctx = EVP_MD_CTX_new();

  // Step 2: initialize EVP_MD
  const EVP_MD* md = getHashFunction(hash);

  if (md == nullptr) {
    EVP_MD_CTX_free(ctx);
    return {false, absl::StrCat(hash, " is not supported.")};
  }
  // Step 3: initialize EVP_DigestVerify
  auto pkeyWrapper = Common::Crypto::Access::getTyped<Common::Crypto::PublicKeyObject>(key);
  EVP_PKEY* pkey = pkeyWrapper->getEVP_PKEY();

  if (pkey == nullptr) {
    free(pkeyWrapper);
    EVP_MD_CTX_free(ctx);
    return {false, "Failed to initialize digest verify."};
  }

  int ok = EVP_DigestVerifyInit(ctx, nullptr, md, nullptr, pkey);
  if (!ok) {
    free(pkeyWrapper);
    EVP_MD_CTX_free(ctx);
    return {false, "Failed to initialize digest verify."};
  }

  // Step 4: verify signature
  ok = EVP_DigestVerify(ctx, signature.data(), signature.size(), text.data(), text.size());

  // Step 5: check result
  if (ok == 1) {
    EVP_MD_CTX_free(ctx);
    return {true, ""};
  }

  EVP_MD_CTX_free(ctx);
  return {false, absl::StrCat("Failed to verify digest. Error code: ", ok)};
}

CryptoObjectPtr importPublicKey(const std::vector<uint8_t>& key) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(key.data());
  EVP_PKEY* pkey = d2i_PUBKEY(nullptr, &data, key.size());
  
  auto publicKeyWrapper = new PublicKeyObject();
  publicKeyWrapper->setEVP_PKEY(pkey);

  std::unique_ptr<PublicKeyObject> publicKeyPtr = std::make_unique<PublicKeyObject>();
  publicKeyPtr.reset(publicKeyWrapper);

  return publicKeyPtr;

}

const EVP_MD* getHashFunction(absl::string_view name) {
  const std::string hash = absl::AsciiStrToLower(name);

  // Hash algorithms set refers
  // https://github.com/google/boringssl/blob/master/include/openssl/digest.h
  if (hash == "sha1") {
    return EVP_sha1();
  } else if (hash == "sha224") {
    return EVP_sha224();
  } else if (hash == "sha256") {
    return EVP_sha256();
  } else if (hash == "sha384") {
    return EVP_sha384();
  } else if (hash == "sha512") {
    return EVP_sha512();
  } else {
    return nullptr;
  }
}

} // namespace Utility
} // namespace Crypto
} // namespace Common
} // namespace Envoy