#include <oqs/oqs.h>

#include <iostream>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

namespace {

// Fixed-size storage: no copies or reallocations leave secret bytes behind.
class SecretBuffer {
public:
    explicit SecretBuffer(std::size_t size) : bytes_(size) {}
    ~SecretBuffer() { OQS_MEM_cleanse(bytes_.data(), bytes_.size()); }

    SecretBuffer(const SecretBuffer&) = delete;
    SecretBuffer& operator=(const SecretBuffer&) = delete;

    std::uint8_t* data() { return bytes_.data(); }

private:
    std::vector<std::uint8_t> bytes_;
};

int run_demo()
{
    // Use liboqs's matching release function when the owner leaves scope.
    const std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(OQS_KEM_alg_ml_kem_768), &OQS_KEM_free);

    if (!kem) {
        std::cerr << "Failed to create ML-KEM-768. Check that it is enabled "
                     "in your liboqs build.\n";
        return 1;
    }

    // These are buffer sizes; creating this object does not generate keys.
    std::cout << "Algorithm: " << kem->method_name << '\n'
              << "Public key: " << kem->length_public_key << " bytes\n"
              << "Secret key: " << kem->length_secret_key << " bytes\n"
              << "Ciphertext: " << kem->length_ciphertext << " bytes\n"
              << "Shared secret: " << kem->length_shared_secret << " bytes\n";

    std::vector<std::uint8_t> public_key(kem->length_public_key);
    std::vector<std::uint8_t> ciphertext(kem->length_ciphertext);
    SecretBuffer secret_key(kem->length_secret_key);
    SecretBuffer client_secret(kem->length_shared_secret);
    SecretBuffer server_secret(kem->length_shared_secret);
    SecretBuffer tampered_secret(kem->length_shared_secret);

    // Server: keep the secret key locally and give the public key to the client.
    if (OQS_KEM_keypair(kem.get(), public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Key generation failed.\n";
        return 1;
    }
    std::cout << "[PASS] Server key generation\n";

    // Client: encapsulation produces BOTH a ciphertext and a shared secret.
    if (OQS_KEM_encaps(kem.get(), ciphertext.data(), client_secret.data(),
                       public_key.data()) != OQS_SUCCESS) {
        std::cerr << "Encapsulation failed.\n";
        return 1;
    }
    std::cout << "[PASS] Client encapsulation\n";

    // Server: recover the shared secret using the received ciphertext.
    if (OQS_KEM_decaps(kem.get(), server_secret.data(), ciphertext.data(),
                       secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Decapsulation failed.\n";
        return 1;
    }
    std::cout << "[PASS] Server decapsulation\n";

    // Both secrets are accessible only because this is a single-process demo.
    if (OQS_MEM_secure_bcmp(client_secret.data(), server_secret.data(),
                            kem->length_shared_secret) != 0) {
        std::cerr << "Shared secrets do not match.\n";
        return 1;
    }
    std::cout << "[PASS] Shared secrets match\n";

    // Negative check: preserve the ciphertext length but flip one bit.
    auto tampered_ciphertext = ciphertext;
    tampered_ciphertext[0] ^= 0x01;
    // ML-KEM uses implicit rejection: an invalid ciphertext yields a fallback
    // secret rather than an explicit authentication-error return value.
    if (OQS_KEM_decaps(kem.get(), tampered_secret.data(), tampered_ciphertext.data(),
                       secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Decapsulation API failed during the tampering check.\n";
        return 1;
    }
    if (OQS_MEM_secure_bcmp(client_secret.data(), tampered_secret.data(),
                            kem->length_shared_secret) == 0) {
        std::cerr << "Tampering check failed: the original shared secret was reproduced.\n";
        return 1;
    }
    std::cout << "[PASS] Tampered ciphertext produces a different shared secret\n";
    return 0;
}

} // namespace

int main()
{
    OQS_init();
    int result = 1;
    try {
        result = run_demo();
    } catch (const std::exception& error) {
        std::cerr << "Demo failed: " << error.what() << '\n';
    }
    // run_demo releases its KEM object before the library is cleaned up.
    OQS_destroy();
    return result;
}
