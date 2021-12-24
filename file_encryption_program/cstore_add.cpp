#include "cstore_add.h"
#include "cstore_utils.h"
#include "crypto_lib/sha256.h"
#include "crypto_lib/aes.h"
#include <unistd.h>

int cstore_add(std::string password, std::string archivename, std::vector<std::string> filenames, int filenames_len){	
    // 1. Create encryption key
    // 2. Check for existing archive
    // 3. If existing archive exists
    // 		Read old HMAC, recompute HMAC and compare...
    // 4. If HMAC ok
    // 		Read each file, get new IV, encrypt, append to arhive
    // 5. Compute new HMAC of updated archive and store it.

    // Check if we can open all files or if file is empty
    for(int i = 0; i < filenames_len; i++){
        FILE *fp = fopen(&(filenames[i][0]), "rb");
        if(fp == NULL){
            fclose(fp);
            die("Can't open a file");
        }

        fseek (fp, 0, SEEK_END);
        int size = ftell(fp);

        if (size == 0){
            fclose(fp);
            die("A file is empty");
        }
        fclose(fp);
    }

    // Make encryption key from password
    BYTE key[SHA256_BLOCK_SIZE];
    iterate_sha256(password, key, 10000);

    // Make HMAC key
    BYTE hmac_key[SHA256_BLOCK_SIZE];
    BYTE key_to_hash[SHA256_BLOCK_SIZE+1];
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        key_to_hash[i] = key[i];
    }
    key_to_hash[SHA256_BLOCK_SIZE] = 'I';
    hash_sha256(key_to_hash, hmac_key, SHA256_BLOCK_SIZE+1);

    // Check if archive exists
    if(access(&archivename[0], 0) == 0){
        // Check if files exist in archive already
        std::string archive_contents_name = archivename + "_contents";
        FILE *fp = fopen(&archive_contents_name[0], "rb");

        fseek(fp, 0, SEEK_END);
        int contents_length = ftell(fp);
        rewind(fp);

        // Read contents into buffer
        BYTE contents_buf[contents_length];
        fread(contents_buf, contents_length, 1, fp);

        std::string haystack(contents_buf, contents_buf+contents_length);
        for(int i = 0; i < filenames_len; i++){
            size_t n = haystack.find(filenames[i]);
            if(n != std::string::npos){
                fclose(fp);
                die("A file already exists in archive");
            }
        }

        // Get hmac and compare
        int hmac_compare = compare_mac_archive_overwrite(archivename, hmac_key);
        if(hmac_compare == 1){
            fclose(fp);
            die("HMACs are different");
        }
        fclose(fp);
    }

    // File pointers
    FILE *archive = fopen(&archivename[0], "ab");
    std::string archive_contents_name = archivename + "_contents";
    FILE *archive_contents = fopen(&archive_contents_name[0], "ab");

    for(int j = 0; j < filenames_len; j++){
        FILE *file = fopen(&(filenames[j][0]), "rb");

        fseek(file, 0, SEEK_END);
        int file_length = ftell(file);
        rewind(file);

        // Write file name into archive and archive contents
        std::vector<BYTE> name;
        for(int i = 0; i < filenames[j].size(); i++){
            name.push_back(filenames[j][i]);
        }
        name.push_back('\0');
        int n = 21-name.size();
        for(int i = 0; i < n; i++){
            name.push_back('\0');
        }
        fwrite(&name[0], 21, 1, archive);
        fwrite(&name[0], 21, 1, archive_contents);

        // Read from file into buffer
        BYTE file_buf[file_length];
        fread(file_buf, file_length, 1, file);

        std::vector<BYTE> file_data;
        for(int i = 0; i < file_length; i++){
            file_data.push_back(file_buf[i]);
        }

        // Encryption:
        // Get random IV
        BYTE IV[16];
        sample_urandom(IV, 16);

        std::vector<BYTE> cipher = encrypt_cbc(file_data, IV, key);
        int cipher_length = cipher.size();

        // Write cipher length to archive
        fwrite(&cipher_length, sizeof(int), 1, archive);

        // Write cipher to archive
        for(int i = 0; i < cipher.size(); i++){
            fwrite(&cipher[i], 1, 1, archive);
        }

        fclose(file);
    }
    fclose(archive);
    fclose(archive_contents);

    // HMAC
    FILE *archive_read = fopen(&archivename[0], "rb");
    fseek(archive_read, 0, SEEK_END);
    int archive_length = ftell(archive_read);
    rewind(archive_read);

    // Read archive into buffer
    BYTE archive_buf[archive_length];
    fread(archive_buf, archive_length, 1, archive_read);

    // Compute HMAC
    BYTE hmac[SHA256_BLOCK_SIZE];
    get_hmac(archive_buf, hmac_key, hmac, archive_length);

    // Append HMAC to archive
    archive = fopen(&archivename[0], "ab");
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        fwrite(&hmac[i], 1, 1, archive);
    }

    fclose(archive);
    fclose(archive_read);
    return 0;
}
