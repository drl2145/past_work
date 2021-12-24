#include <iostream>
#include "crypto_lib/aes.c"
#include "crypto_lib/sha256.c"
#include "cstore_list.h"
#include "cstore_add.h"
#include "cstore_extract.h"
#include "cstore_delete.h"
#include "cstore_utils.h"
#include <pwd.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    // Check correct number of arguments (minimum 3)
    if(argc < 3){
        std::cerr << "Usage: ./cstore <function> [-p password] archivename <files>\n"
            << "<function> can be: list, add, extract, delete.\n"
            << "Options:\n"
            << "\t-p <PASSWORD>\t\t Specify password (plaintext) in console. If not supplied, user will be prompted."
            << std::endl;
        return 1;
    }

    // Check the function that the user wants to perform on the archive
    std::string function = argv[1];
    if(function == "list"){
        std::string archivename = argv[2];
        return cstore_list(archivename); 
    } else if(function == "add" || function == "extract" || function == "delete"){
        std::string password;
        std::string archivename;
        std::vector<std::string> filenames;
        int filenames_len;

        if(strcmp(argv[2], "-p") != 0){
            password = getpass("Enter password: ");
            if(password.size() > 12){
                printf("Password too long\n");
                exit(1);
            }
            archivename = argv[2];
            for(int i = 0; i < argc-3; i++){
                filenames.push_back(argv[i+3]);
            }
            filenames_len = argc-3;
        } else{
            password = argv[3];
            if(password.size() > 12){
                printf("Password too long\n");
                exit(1);
            }
            archivename = argv[4];
            for(int i = 0; i < argc-5; i++){
                filenames.push_back(argv[i+5]);
            }
            filenames_len = argc-5;
        }

        if(function == "add"){
            return cstore_add(password, archivename, filenames, filenames_len);
        }

        if(function == "extract"){
            return cstore_extract(password, archivename, filenames, filenames_len);
        }

        if(function == "delete"){
            return cstore_delete(password, archivename, filenames);
        }
    } else{
        std::cerr << "ERROR: cstore <function> must have <function> in: {list, add, extract, delete}.\n";
        return 1;
    }
}
