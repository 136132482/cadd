#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

class CryptoUtil {
public:
    // ==================== 哈希算法 ====================

    // MD5哈希
    static std::string md5(const std::string& input) {
        unsigned char digest[MD5_DIGEST_LENGTH];
        MD5((const unsigned char*)input.c_str(), input.size(), digest);

        return hexEncode(digest, MD5_DIGEST_LENGTH);
    }

    // SHA256哈希
    static std::string sha256(const std::string& input) {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char*)input.c_str(), input.size(), digest);

        return hexEncode(digest, SHA256_DIGEST_LENGTH);
    }

    // PBKDF2密钥派生
    static std::string pbkdf2(const std::string& password, const std::string& salt,
                              int iterations = 10000, int keyLength = 64) {
        unsigned char key[keyLength];

        PKCS5_PBKDF2_HMAC(
                password.c_str(), password.length(),
                (const unsigned char*)salt.c_str(), salt.length(),
                iterations,
                EVP_sha512(),
                keyLength, key
        );

        return hexEncode(key, keyLength);
    }

    // ==================== 对称加密(AES) ====================

    // AES加密 (CBC模式)
    static std::string aesEncrypt(const std::string& plaintext, const std::string& key) {
        // 生成随机IV
        unsigned char iv[AES_BLOCK_SIZE];
        RAND_bytes(iv, AES_BLOCK_SIZE);

        // 设置加密上下文
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           (const unsigned char*)key.c_str(), iv);

        // 加密
        int len;
        int ciphertextLen = plaintext.size() + AES_BLOCK_SIZE;
        unsigned char* ciphertext = new unsigned char[ciphertextLen];

        EVP_EncryptUpdate(ctx, ciphertext, &len,
                          (const unsigned char*)plaintext.c_str(), plaintext.size());
        ciphertextLen = len;

        EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
        ciphertextLen += len;

        // 组合IV和密文
        std::string result = hexEncode(iv, AES_BLOCK_SIZE) + ":" +
                             hexEncode(ciphertext, ciphertextLen);

        // 清理
        delete[] ciphertext;
        EVP_CIPHER_CTX_free(ctx);

        return result;
    }

    // AES解密 (CBC模式)
    static std::string aesDecrypt(const std::string& ciphertext, const std::string& key) {
        // 分离IV和密文
        size_t pos = ciphertext.find(':');
        std::string ivHex = ciphertext.substr(0, pos);
        std::string cipherHex = ciphertext.substr(pos + 1);

        unsigned char iv[AES_BLOCK_SIZE];
        hexDecode(ivHex, iv, AES_BLOCK_SIZE);

        std::vector<unsigned char> cipher = hexDecode(cipherHex);

        // 设置解密上下文
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           (const unsigned char*)key.c_str(), iv);

        // 解密
        int len;
        int plaintextLen = cipher.size();
        unsigned char* plaintext = new unsigned char[plaintextLen];

        EVP_DecryptUpdate(ctx, plaintext, &len, cipher.data(), cipher.size());
        plaintextLen = len;

        EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
        plaintextLen += len;

        std::string result((char*)plaintext, plaintextLen);

        // 清理
        delete[] plaintext;
        EVP_CIPHER_CTX_free(ctx);

        return result;
    }

    // ==================== 非对称加密(RSA) ====================

    // 生成RSA密钥对
    static void generateRSAKeyPair(std::string& privateKey, std::string& publicKey,
                                   int keySize = 2048) {
        RSA* rsa = RSA_new();
        BIGNUM* bn = BN_new();

        BN_set_word(bn, RSA_F4);
        RSA_generate_key_ex(rsa, keySize, bn, NULL);

        // 写入内存BIO
        BIO* pri = BIO_new(BIO_s_mem());
        BIO* pub = BIO_new(BIO_s_mem());

        PEM_write_bio_RSAPrivateKey(pri, rsa, NULL, NULL, 0, NULL, NULL);
        PEM_write_bio_RSAPublicKey(pub, rsa);

        // 获取字符串
        size_t priLen = BIO_pending(pri);
        size_t pubLen = BIO_pending(pub);

        privateKey.resize(priLen);
        publicKey.resize(pubLen);

        BIO_read(pri, &privateKey[0], priLen);
        BIO_read(pub, &publicKey[0], pubLen);

        // 清理
        BIO_free_all(pri);
        BIO_free_all(pub);
        RSA_free(rsa);
        BN_free(bn);
    }

    // RSA公钥加密
    static std::string rsaEncrypt(const std::string& plaintext, const std::string& publicKey) {
        BIO* bio = BIO_new_mem_buf(publicKey.c_str(), publicKey.size());
        RSA* rsa = PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL);

        if (!rsa) {
            BIO_free(bio);
            throw std::runtime_error("Invalid public key");
        }

        int keySize = RSA_size(rsa);
        std::vector<unsigned char> ciphertext(keySize);

        int result = RSA_public_encrypt(
                plaintext.size(), (const unsigned char*)plaintext.c_str(),
                ciphertext.data(), rsa, RSA_PKCS1_PADDING
        );

        if (result == -1) {
            BIO_free(bio);
            RSA_free(rsa);
            throw std::runtime_error("RSA encryption failed");
        }

        BIO_free(bio);
        RSA_free(rsa);

        return hexEncode(ciphertext.data(), result);
    }

    // RSA私钥解密
    static std::string rsaDecrypt(const std::string& ciphertext, const std::string& privateKey) {
        BIO* bio = BIO_new_mem_buf(privateKey.c_str(), privateKey.size());
        RSA* rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);

        if (!rsa) {
            BIO_free(bio);
            throw std::runtime_error("Invalid private key");
        }

        std::vector<unsigned char> cipher = hexDecode(ciphertext);
        int keySize = RSA_size(rsa);
        std::vector<unsigned char> plaintext(keySize);

        int result = RSA_private_decrypt(
                cipher.size(), cipher.data(),
                plaintext.data(), rsa, RSA_PKCS1_PADDING
        );

        if (result == -1) {
            BIO_free(bio);
            RSA_free(rsa);
            throw std::runtime_error("RSA decryption failed");
        }

        BIO_free(bio);
        RSA_free(rsa);

        return std::string((char*)plaintext.data(), result);
    }

    // ==================== 实用工具函数 ====================

    // 生成随机盐
    static std::string generateSalt(size_t length = 16) {
        unsigned char* salt = new unsigned char[length];
        RAND_bytes(salt, length);

        std::string result = hexEncode(salt, length);
        delete[] salt;

        return result;
    }

    // 十六进制编码
    static std::string hexEncode(const unsigned char* data, size_t length) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        for(size_t i = 0; i < length; ++i) {
            ss << std::setw(2) << (int)data[i];
        }

        return ss.str();
    }

    // 十六进制解码
    static std::vector<unsigned char> hexDecode(const std::string& hex) {
        std::vector<unsigned char> bytes;

        for(size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            unsigned char byte = (unsigned char)strtol(byteString.c_str(), NULL, 16);
            bytes.push_back(byte);
        }

        return bytes;
    }

    // 十六进制解码到现有缓冲区
    static void hexDecode(const std::string& hex, unsigned char* output, size_t outputLength) {
        std::vector<unsigned char> bytes = hexDecode(hex);
        if (bytes.size() != outputLength) {
            throw std::runtime_error("Hex decode length mismatch");
        }
        memcpy(output, bytes.data(), outputLength);
    }
};