#include <string>
#include <cstring>
#include "cstore_add.h"
#include "cstore_utils.h"
#include "crypto_lib/sha256.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <unistd.h>

typedef unsigned char BYTE;

int cstore_extract(std::string password, std::string archivename, std::vector<std::string> filenames, int filenames_len){
    // 1. Build key
    // 2. Compare HMAC
    // 3. Loop over archive for files to extract

    // Check if files exist in archive
    std::string archive_contents_name = archivename + "_contents";
    FILE *fp = fopen(&archive_contents_name[0], "rb");

    fseek(fp, 0, SEEK_END);
    int contents_length = ftell(fp);
    rewind(fp);

    // Read contents into buffer
    BYTE contents_buf[contents_length];
    fread(contents_buf, contents_length, 1, fp);

    std::string haystack_contents(contents_buf, contents_buf+contents_length);
    for(int i = 0; i < filenames_len; i++){
        size_t n = haystack_contents.find(filenames[i]);
        if(n == std::string::npos){
            fclose(fp);
            die("A file does not exist in archive");
        }
    }

    BYTE key[SHA256_BLOCK_SIZE];
    iterate_sha256(password, key, 10000);
    WORD key_schedule[60];
    aes_key_setup(key, key_schedule, 256);

    FILE *archive = fopen(&archivename[0], "rb");

    fseek(archive, 0, SEEK_END);
    int archive_length = ftell(archive);
    rewind(archive);

    // Read archive into buffer
    BYTE archive_buf[archive_length];
    fread(archive_buf, archive_length, 1, archive);

    // Get HMAC from archive
    BYTE hmac_from_archive[SHA256_BLOCK_SIZE];
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        hmac_from_archive[i] = archive_buf[archive_length-SHA256_BLOCK_SIZE+i];
    }

    // Remove HMAC from archive
    BYTE archive_buf_nohmac[archive_length-SHA256_BLOCK_SIZE];
    for(int i = 0; i < archive_length-SHA256_BLOCK_SIZE; i++){
        archive_buf_nohmac[i] = archive_buf[i];
    }

    // HMAC:
    // Make key
    BYTE computed_hmac[SHA256_BLOCK_SIZE];
    BYTE hmac_key[SHA256_BLOCK_SIZE];
    BYTE key_to_hash[SHA256_BLOCK_SIZE+1];
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        key_to_hash[i] = key[i];
    }
    key_to_hash[SHA256_BLOCK_SIZE] = 'I';
    hash_sha256(key_to_hash, hmac_key, SHA256_BLOCK_SIZE+1);

    // Compute HMAC
    get_hmac(archive_buf_nohmac, hmac_key, computed_hmac, archive_length-SHA256_BLOCK_SIZE);

    // Compare HMACs
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        if(hmac_from_archive[i] != computed_hmac[i]){
            fclose(archive);
            die("HMACs are different");
        }
    }

    // Loop through all files
    for(int j = 0; j < filenames_len; j++){
        // find filename and length
        std::string filename = filenames[j];
        std::string haystack(archive_buf, archive_buf+archive_length);
        size_t n = haystack.find(filename);
        char *filename_ptr = (char *)(archive_buf + n);
        int file_length = *(int *)(filename_ptr+21);
        int file_begin_index = filename_ptr + 25 - (char *)archive_buf;
        
        // Find cipher
        std::vector<BYTE> cipher;
        for(int i = file_begin_index; i < file_begin_index+file_length; i++){
            cipher.push_back(archive_buf[i]);
        }
        
        // Decrypt
        std::vector<BYTE> plaintext = decrypt_cbc(cipher, key);

        FILE *file_output = fopen(&filename[0], "wb");

        for(int i = 0; i < plaintext.size(); i++){
            fwrite(&plaintext[i], 1, 1, file_output);
        }

        fclose(file_output);
    }
    
    fclose(archive);
    fclose(fp);
    
    return 0;
}
