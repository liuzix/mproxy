//
// Created by zixiong on 11/27/17.
//

#include "cert.h"

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <stdexcept>
#include <cstring>
#include <sys/stat.h>

static int file_exists (const char * fileName) {
    struct stat buf;
    int i = stat (fileName, &buf);
    /* File found */
    if (i == 0) {
        return 1;
    }
    return 0;

}

void openSSLInit() {
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    ERR_load_crypto_strings();
    srand((unsigned)time(nullptr));
}

static int do_X509_sign(X509 *cert, EVP_PKEY *pkey, const EVP_MD *md) {

    int rv;
    EVP_MD_CTX mctx;
    EVP_PKEY_CTX *pkctx = NULL;

    EVP_MD_CTX_init(&mctx);
    rv = EVP_DigestSignInit(&mctx, &pkctx, md, NULL, pkey);

    if (rv > 0)
        rv = X509_sign_ctx(cert, &mctx);
    EVP_MD_CTX_cleanup(&mctx);
    return rv > 0 ? 1 : 0;
}

#define addName(field, value) X509_NAME_add_entry_by_txt(name, field,  MBSTRING_ASC, (unsigned char *)value, -1, -1, 0)


// reference https://dev.to/ecnepsnai/pragmatically-generating-a-self-signed-certificate-and-private-key-usingopenssl
void generateCertForDomain(const char* domain) {
    if (file_exists(("certs/" + std::string(domain) + ".pem").c_str())) {
        return;
    }

    FILE* CAFile = fopen("certs/ca.crt", "r");

    X509* cacert = PEM_read_X509(CAFile,NULL,NULL,NULL);

    if (!cacert) {
        throw std::runtime_error("Could not read CA pem file");
    }

    FILE* privateKeyFile = fopen("certs/server.key", "r");

    RSA* keyPair = nullptr;

    PEM_read_RSAPrivateKey(privateKeyFile, &keyPair, nullptr, nullptr);

    fclose(privateKeyFile);

    EVP_PKEY *pkey = EVP_PKEY_new();
    if (!pkey) {
        throw std::runtime_error("Could not create EVP object");
    }

    if (!EVP_PKEY_set1_RSA(pkey, keyPair)) {
        throw std::runtime_error("Could not assign RSA key to EVP object");
    }


    X509* x = X509_new();
    X509_set_version (x, 2);
    if (!X509_set_pubkey(x, pkey)) {
        throw std::runtime_error("Couldn't set public key!");
    }


    auto random_serial_number = (unsigned long)rand();
    ASN1_INTEGER_set(X509_get_serialNumber(x), random_serial_number);


    // set expiration time
    X509_gmtime_adj((ASN1_TIME *)X509_get_notBefore(x), 0);
    X509_gmtime_adj((ASN1_TIME *)X509_get_notAfter(x), 7776000);

    X509_NAME * name = X509_NAME_new();

    addName("CN", domain);
    addName("OU", "Certificate Authority");
    addName("O",  "Zixiong Liu");
    addName("L",  "Chicago");
    addName("S",  "Illinois");
    addName("C",  "US");

    X509_set_issuer_name(x, X509_get_issuer_name(cacert));
    X509_set_subject_name(x, name);

    std::string sanStr = "DNS.1:" + std::string(domain);
    auto sanCstr = (char*)malloc(sanStr.length() + 1);
    strcpy(sanCstr, sanStr.c_str());

    X509V3_CTX ctx;
    X509V3_set_ctx(&ctx, cacert, x, NULL, NULL, 0);


    X509_EXTENSION * extension = X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, sanCstr);

    if (X509_add_ext(x, extension, -1) == 0) {
        X509_EXTENSION_free(extension);
        throw std::runtime_error("Fail to add extension");
    }
    X509_EXTENSION_free(extension);

    if (do_X509_sign(x, pkey, EVP_sha512()) < 0) {
        throw std::runtime_error("Fail to sign");
    }

    FILE* f = fopen(("certs/" + std::string(domain) + ".pem").c_str(), "wb");
    if (PEM_write_X509(f, x) < 0) {
        fclose(f);
        throw std::runtime_error("Could not save cert!");
    }
    fclose(f);

    //X509_print_fp(stdout, x);

    // TODO: free stuff

}