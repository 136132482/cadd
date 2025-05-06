//
// Created by Administrator on 2025/4/28 0028.
//

#include "CryptoUtil.h"
#include <iostream>


int main(){
    // MD5哈希
    std::string md5Hash = CryptoUtil::md5("hello world");

// SHA256哈希
    std::string sha256Hash = CryptoUtil::sha256("hello world");

// PBKDF2密钥派生
    std::string salt = CryptoUtil::generateSalt();
    std::string derivedKey = CryptoUtil::pbkdf2("password", salt, 10000);

   //AES
// 生成随机AES密钥
    std::string aesKey = CryptoUtil::generateSalt(32); // 256位密钥

// 加密
    std::string plaintext = "This is a secret message";
    std::string ciphertext = CryptoUtil::aesEncrypt(plaintext, aesKey);

// 解密
    std::string decrypted = CryptoUtil::aesDecrypt(ciphertext, aesKey);

    //RSA
    // 生成RSA密钥对
    std::string privateKey, publicKey;
    CryptoUtil::generateRSAKeyPair(privateKey, publicKey);

// 公钥加密
    std::string secretMessage = "Confidential data";
    std::string rsa_encrypted = CryptoUtil::rsaEncrypt(secretMessage, publicKey);

// 私钥解密
    std::string rsa_decrypted = CryptoUtil::rsaDecrypt(rsa_encrypted, privateKey);


    // 添加输出
    std::cout << "MD5: " << md5Hash << std::endl;
    std::cout << "SHA256: " << sha256Hash << std::endl;
    std::cout << "PBKDF2: " << derivedKey << std::endl;

    std::cout << "AES加密结果: " << ciphertext << std::endl;
    std::cout << "AES解密结果: " << decrypted << std::endl;

    std::cout << "RSA公钥:\n" << publicKey << std::endl;
    std::cout << "RSA私钥:\n" << privateKey << std::endl;
    std::cout << "RSA加密结果: " << rsa_encrypted << std::endl;
    std::cout << "RSA解密结果: " << rsa_decrypted << std::endl;

    return 0;







}
