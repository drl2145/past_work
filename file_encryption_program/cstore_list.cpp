#include "cstore_list.h"
#include "cstore_utils.h"
#include "crypto_lib/sha256.h"
#include <unistd.h>
#include <cstring>

typedef unsigned char BYTE;

// Change argument as needed
int cstore_list(std::string archivename)
{
    if(access(&archivename[0], 0) != 0){
        die("Archive does not exist");
    }

    std::string archive_contents = archivename + "_contents";
    FILE *fp = fopen(&archive_contents[0], "rb");
    FILE *list = fopen("list.txt", "wb");
    
    fseek(fp, 0, SEEK_END);
	int contents_length = ftell(fp);
	rewind(fp);

	// read contents into buffer
	BYTE contents_buf[contents_length];
	fread(contents_buf, contents_length, 1, fp);

    for(int i = 0; i < contents_length; i+=21){
        int len = strlen((char *)&contents_buf[i]);
        for(int j = i; j < i+len; j++){
            fwrite(&contents_buf[j], 1, 1, list);
        }
        char newline = '\n';
        fwrite(&newline, 1, 1, list);
    }

    fclose(fp);
    fclose(list);

    return 0;
}
