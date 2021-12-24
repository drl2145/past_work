#include "cstore_add.h"
#include "cstore_utils.h"
#include "crypto_lib/sha256.h"
#include <unistd.h>

typedef unsigned char BYTE;

int cstore_delete(std::string password, std::string archivename, std::vector<std::string> filenames){	
    // 1. Create encryption key
    // 2. Compute HMAC
    // 3. Iterate through archive and see which files to delete
    // 4. Recompute HMAC, etc.

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
        // check if files exist in archive
        std::string archive_contents_name = archivename + "_contents";
        FILE *fp = fopen(&archive_contents_name[0], "rb");

        fseek(fp, 0, SEEK_END);
        int contents_length = ftell(fp);
        rewind(fp);

        // Read contents into buffer
        BYTE contents_buf[contents_length];
        fread(contents_buf, contents_length, 1, fp);

        std::string haystack(contents_buf, contents_buf+contents_length);
        for(int i = 0; i < filenames.size(); i++){
            size_t n = haystack.find(filenames[i]);
            if(n == std::string::npos){
                fclose(fp);
                die("A file does not exist in archive");
            }
        }

        // Get hmac and compare
        int hmac_compare = compare_mac_archive_overwrite(archivename, hmac_key);
        if(hmac_compare == 1){
            fclose(fp);
            die("HMACs are different");
        }
        fclose(fp);
    } else{
        die("Archive does not exist");
    }

    // Read archive into buffer
    FILE *archive = fopen(&archivename[0], "rb");
    std::string archive_contents_name = archivename + "_contents";
    FILE *archive_contents = fopen(&archive_contents_name[0], "rb");

    fseek(archive, 0, SEEK_END);
    int archive_length = ftell(archive);
    rewind(archive);

    fseek(archive_contents, 0, SEEK_END);
    int contents_length = ftell(archive_contents);
    rewind(archive_contents);

    // Read contents into buffer
    BYTE archive_buf[archive_length];
    fread(archive_buf, archive_length, 1, archive);
    
    BYTE contents_buf[contents_length];
    fread(contents_buf, contents_length, 1, archive_contents);

    // Transfer buffer to vector
    std::vector<BYTE> archive_vector;
    for(int i = 0; i < archive_length; i++){
        archive_vector.push_back(archive_buf[i]);
    }

    std::vector<BYTE> contents_vector;
    for(int i = 0; i < contents_length; i++){
        contents_vector.push_back(contents_buf[i]);
    }

    // Loop through files
    for(int i = 0; i < filenames.size(); i++){
        std::string filename = filenames[i];

        // Find start and end index of file contents in archive
        std::string haystack(&archive_vector[0], &archive_vector[archive_vector.size()]);
        int start_index = haystack.find(filename);
        char *filename_ptr = (char *)(&archive_vector[start_index]);
        int file_length = *(int *)(filename_ptr+21);
        int end_index = start_index + 21 + 4 + file_length;

        archive_vector.erase(archive_vector.begin()+start_index, archive_vector.begin()+end_index);

        // Remove file from archive_contents
        std::string contents_haystack(&contents_vector[0], &contents_vector[contents_vector.size()]);
        int contents_start_index = contents_haystack.find(filename);
        int contents_end_index = contents_start_index + 21;

        contents_vector.erase(contents_vector.begin()+contents_start_index, contents_vector.begin()+contents_end_index);
    }
    fclose(archive);
    fclose(archive_contents);

    // Compute new HMAC
    BYTE computed_hmac[SHA256_BLOCK_SIZE];
    get_hmac(&archive_vector[0], hmac_key, computed_hmac, archive_vector.size());

    // Add hmac to archive_vector
    for(int i = 0; i < SHA256_BLOCK_SIZE; i++){
        archive_vector.push_back(computed_hmac[i]);
    }

    FILE *archive_write = fopen(&archivename[0], "wb");
    FILE *archive_contents_write = fopen(&archive_contents_name[0], "wb");

    // Write archive_vector to archive
    for(int i = 0; i < archive_vector.size(); i++){
        fwrite(&archive_vector[i], 1, 1, archive_write);
    }

    // Write to archive_contents
    for(int i = 0; i < contents_vector.size(); i++){
        fwrite(&contents_vector[i], 1, 1, archive_contents_write);
    }

    fclose(archive_write);
    fclose(archive_contents_write);

    return 0;
}
