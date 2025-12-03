#include "mbedtls/aes.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "TLS";
static const uint8_t key[32] = {
    0x74, 0xce, 0x71, 0x4c, 0x34, 0x1e, 0xb7, 0x17,
    0x2d, 0x3b, 0xf9, 0x77, 0xcc, 0x85, 0xb2, 0x81,
    0xdd, 0xa4, 0xc3, 0xa4, 0x35, 0x36, 0x2b, 0x1a,
    0x20, 0xf8, 0x34, 0x19, 0x38, 0x08, 0xa8, 0xc5
};

int encrypt_message(uint8_t* plaintext, uint8_t plaintext_len,
            uint8_t* output, uint8_t *out_len) {
    
    mbedtls_aes_context aes_ctx; //context needed for AES

    uint8_t iv[16]; //new iv for each encrypt
    esp_fill_random(iv, sizeof(iv)); //create iv 

    uint8_t pad_len = 16 - (plaintext_len % 16); //message must be mult of 16 for cbc
    // if (pad_len == 16) {
    //     pad_len = 0;
    // }
    uint8_t padded_len = plaintext_len + pad_len;

    uint8_t buffer[padded_len];
    memcpy(buffer, plaintext, plaintext_len);
    memset(buffer + plaintext_len, pad_len, pad_len); //pad with hex value = number of bytes needed

    memcpy(output, iv, 16); 
    uint8_t* cipher_ptr = output + 16; //write cipher text at 16 byte offset from output

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, key, 256);

    int ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, padded_len, iv, buffer, cipher_ptr);

    mbedtls_aes_free(&aes_ctx);
    if (ret != 0) {
        ESP_LOGE(TAG, "Encryption failed (%d)", ret);
        return ret;
    }

    *out_len = 16 + padded_len; //iv len = 16
    return 0;
}

int decrypt_message(uint8_t *input, uint8_t input_len,
            uint8_t *plaintext_out) {
    
    if (input_len < 16) {
        ESP_LOGE(TAG, "Input too short, invalid ciphertext");
        return -1;
    }

    mbedtls_aes_context aes_ctx;

    uint8_t iv[16];
    memcpy(iv, input, 16);  //extract iv from start of message 

    uint8_t* cipher_ptr = input + 16; //move pointer to start of ctx
    uint8_t cipher_len = input_len - 16; //subtract len(iv)

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, key, 256);

    int ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, cipher_len, iv, cipher_ptr, plaintext_out); //decode ctx into ptx_out

    mbedtls_aes_free(&aes_ctx);
    if (ret != 0) {
        ESP_LOGE(TAG, "Decryption failed (%d)", ret);
        return ret;
    }

    //depad
    uint8_t pad_len = plaintext_out[cipher_len - 1];
    if (pad_len == 0 || pad_len > 16) {
        ESP_LOGE(TAG, "Invalid padding");
        return -1;
    }

    uint8_t plain_len = cipher_len - pad_len;
    plaintext_out[plain_len] = '\0'; //set null terminator for string

    return 0;
}
