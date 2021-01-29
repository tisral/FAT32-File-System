// #define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens \
                           // so we need to define what delimits our tokens.   \
                           // In this case  white space                        \
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5 // Mav shell only supports five arguments
#define rootAddress 1049600
int History[5];
int historyCounter = 0;

char BS_OEMName[8];
uint16_t BPB_BytesPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATs;
int16_t BPB_RootEntCnt;
char BS_VolLab[11];
uint32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

FILE *fp = NULL;

int directoryAddress;

struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

int LBAToOffset(int32_t sector)
{
    return ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) +
           (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

int16_t NextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

void compare(char *token, char expanded_name[])
{

    strncpy(expanded_name, token, strlen(token));

    token = strtok(NULL, ".");

    if (token)
    {
        strncpy((char *)(expanded_name + 8), token, strlen(token));
    }

    expanded_name[11] = '\0';

    int i;
    for (i = 0; i < 11; i++)
    {
        expanded_name[i] = toupper(expanded_name[i]);
    }
}

bool searchFile(int *index, char *token)
{
    char fileOrFolder[12];
    memset(fileOrFolder, ' ', 12);
    char *tok = strtok(token, ".");
    compare(tok, fileOrFolder);

    for (int i = 0; i < 16; i++)
    {

        // Trim the directory folder or file
        char trimedStr[12];
        memset(trimedStr, ' ', 12);
        strncpy(trimedStr, dir[i].DIR_Name, 12);
        trimedStr[strlen(trimedStr) - 1] = '\0';

        // printf("%ld\n", strlen(fileOrFolder));

        if ((dir[i].DIR_Attr == 16 || dir[i].DIR_Attr == 32) &&
            strcmp(fileOrFolder, trimedStr) == 0)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

int main()
{

    char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);

    char fileLine[101];

    bool found = false;
    char currentDirectory[30];

    while (1)
    {
        // Print out the mfs prompt
        printf("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
            ;

        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];

        int token_count = 0;

        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;

        char *working_str = strdup(cmd_str);

        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;

        // Tokenize the input stringswith whitespace used as the delimiter
        while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
               (token_count < MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
            if (strlen(token[token_count]) == 0)
            {
                token[token_count] = NULL;
            }
            token_count++;
        }

        if (strcmp(token[0], "open") == 0)
        {
            if (fp)
            {
                printf("Error: File system image already open.\n\n");
            }

            else
            {
                fp = fopen(token[1], "r");

                if (fp == NULL)
                {
                    printf("Error: File system image not found.\n\n");
                }
                else
                {
                    // openning fat32 file

                    fseek(fp, 3, SEEK_SET);
                    fread(&BS_OEMName, 8, 1, fp);

                    fseek(fp, 11, SEEK_SET);
                    fread(&BPB_BytesPerSec, 2, 1, fp);

                    fseek(fp, 11, SEEK_SET);
                    fread(&BPB_BytesPerSec, 2, 1, fp);

                    fseek(fp, 13, SEEK_SET);
                    fread(&BPB_SecPerClus, 1, 1, fp);

                    fseek(fp, 14, SEEK_SET);
                    fread(&BPB_RsvdSecCnt, 2, 1, fp);

                    fseek(fp, 16, SEEK_SET);
                    fread(&BPB_NumFATs, 1, 1, fp);

                    fseek(fp, 17, SEEK_SET);
                    fread(&BPB_RootEntCnt, 2, 1, fp);

                    fseek(fp, 36, SEEK_SET);
                    fread(&BPB_FATSz32, 4, 1, fp);

                    fseek(fp, 44, SEEK_SET);
                    fread(&BPB_RootClus, 4, 1, fp);

                    fseek(fp, 71, SEEK_SET);
                    fread(&BS_VolLab, 11, 1, fp);

                    directoryAddress = rootAddress;
                    History[0] = rootAddress;
                    fseek(fp, directoryAddress, SEEK_SET);

                    for (int i = 0; i < 16; i++)
                    {
                        memset(&dir[i], 0, 32);
                        fread(&dir[i], 32, 1, fp);
                    }
                }
            }
        }

        // closing file
        else if (strcmp(token[0], "close") == 0 && fp)
        {
            fclose(fp);
            fp = NULL;
        }

        // Getting file info
        else if ((strcmp(token[0], "info") == 0) && fp)
        {
            printf("BPB_BytsPerSec: %d\n", BPB_BytesPerSec);
            printf("BPB_BytsPerSec: %x\n\n", BPB_BytesPerSec);

            printf("BPB_SecPerClus: %d\n", BPB_SecPerClus);
            printf("BPB_SecPerClus: %x\n\n", BPB_SecPerClus);

            printf("BPB_RsvdSecCnt: %d\n", BPB_RsvdSecCnt);
            printf("BPB_RsvdSecCnt: %x\n\n", BPB_RsvdSecCnt);

            printf("BPB_NumFATs: %d\n", BPB_NumFATs);
            printf("BPB_NumFATs: %x\n\n", BPB_NumFATs);

            printf("BPB_FATSz32: %d\n", BPB_FATSz32);
            printf("BPB_FATSz32: %x\n\n", BPB_FATSz32);
        }

        // Getting file stat
        else if (strcmp(token[0], "stat") == 0 && token[1] != NULL && fp)
        {
            int index = 0;

            printf("%-20s %-15s %-10s\n", "Attribute", "Size", "Starting Cluster Number");

            // Checking if the file or folder name entered is in the directory
            // and return the index of it
            if (searchFile(&index, token[1]))
            {
                printf("%-20d %-15d %-10d\n\n", dir[index].DIR_Attr, dir[index].DIR_FileSize,
                       dir[index].DIR_FirstClusterLow);
            }

            else
            {
                printf("Error: File not found\n\n");
            }
        }

        // Navagating to differnt dierctories
        else if (strcmp(token[0], "cd") == 0 && token[1] != NULL && fp)
        {
            // Transform tne entered file in uppercase file format display
            char temp1[10];
            strncpy(temp1, token[1], 4);
            temp1[3] = '\0';

            char temp2[30];
            strcpy(temp2, token[1]);

            if (strcmp(token[1], ".") == 0)
            {
            }

            else if (strcmp(token[1], "~") == 0)
            {
                directoryAddress = rootAddress;
                History[0] = rootAddress;
                historyCounter = 0;
            }
            else if (strcmp(token[1], "..") == 0 && historyCounter != 0)
            {
                historyCounter--;
                directoryAddress = History[historyCounter];
            }
            else if (strcmp(temp1, "../") == 0 && historyCounter != 0)
            {
                char *myToken = strtok(token[1], "../");

                historyCounter--;
                directoryAddress = History[historyCounter];
                fseek(fp, directoryAddress, SEEK_SET);

                for (int i = 0; i < 16; i++)
                {
                    memset(&dir[i], 0, 32);
                    fread(&dir[i], 32, 1, fp);
                }

                int index = 0;

                // Checking if the folder name entered is in the directory
                // and return the index of it
                if (!searchFile(&index, myToken))
                {
                    printf("There is no such directory\n\n");

                    searchFile(&index, currentDirectory);
                }

                directoryAddress = LBAToOffset(dir[index].DIR_FirstClusterLow);
                historyCounter++;
                History[historyCounter] = directoryAddress;
            }

            else
            {
                int index = 0;

                // Checking if the folder name entered is in the directory
                // and return the index of it
                if (searchFile(&index, token[1]))
                {
                    strcpy(currentDirectory, temp2);

                    directoryAddress = LBAToOffset(dir[index].DIR_FirstClusterLow);
                    historyCounter++;
                    History[historyCounter] = directoryAddress;
                }

                else
                {
                    printf("There is no such directory\n\n");
                }
            }
            fseek(fp, directoryAddress, SEEK_SET);

            for (int i = 0; i < 16; i++)
            {
                memset(&dir[i], 0, 32);

                fread(&dir[i], 32, 1, fp);
            }
        }

        // List the content of the current directory

        else if (strcmp(token[0], "ls") == 0 && fp)
        {
            for (int i = 0; i < 16; i++)
            {
                if ((dir[i].DIR_Attr == 16 || dir[i].DIR_Attr == 32) &&
                    dir[i].DIR_Name[0] != (char)0xE5 &&
                    dir[i].DIR_Name[0] != (char)0x05 && dir[i].DIR_Name[0] != (char)0x00)
                {
                    // Trim the directory folder or file
                    char trimedStr[12];
                    memset(trimedStr, ' ', 12);
                    strncpy(trimedStr, dir[i].DIR_Name, 12);
                    trimedStr[strlen(trimedStr) - 1] = '\0';

                    if (strcmp(trimedStr, "..         ") == 0)
                    {
                        printf(".\n%s\n", trimedStr);
                    }
                    else
                    {
                        printf("%s\n", trimedStr);
                    }
                }
            }

            printf("\n");
        }

        // extracting the selected file or folder
        else if (strcmp(token[0], "get") == 0 && fp && token[1] != NULL)
        {
            int index = 0;

            char fileToExport[30];
            strcpy(fileToExport, token[1]);

            // Checking if the file or folder name entered is in the directory
            // and return the index of it
            if (searchFile(&index, token[1]))
            {
                int lowClusterNumber = dir[index].DIR_FirstClusterLow;
                int fileSize = dir[index].DIR_FileSize;

                fseek(fp, LBAToOffset(lowClusterNumber), SEEK_SET);

                char buffer[fileSize];

                for (int i = 0; i < fileSize; i++)
                {
                    fread(&buffer[i], 1, 1, fp);
                }

                FILE *output = fopen(fileToExport, "w");

                for (int i = 0; i < fileSize; i++)
                {
                    fwrite(&buffer[i], 1, 1, output);
                }

                fclose(output);
            }

            else
            {
                printf("Error: Cannot get directory\n\n");
            }
        }

        // reading the selected file or folder

        else if (strcmp(token[0], "read") == 0 && token[1] != NULL && token[2] != NULL &&
                 token[3] != NULL && fp)
        {
            int index = 0;

            // Checking if the file or folder name entered is in the directory
            // and return the index of it
            if (searchFile(&index, token[1]))
            {
                int position = atoi(token[2]);
                int numOfBytes = atoi(token[3]);

                int lowClusterNumber = dir[index].DIR_FirstClusterLow;
                int fileSize = dir[index].DIR_FileSize;

                fseek(fp, LBAToOffset(lowClusterNumber), SEEK_SET);

                uint8_t buffer[position + numOfBytes];

                fread(&buffer, position + numOfBytes, 1, fp);

                for (int i = position; i < position + numOfBytes; i++)
                {
                    printf("%d ", buffer[i]);
                }
                printf("\n\n");
            }

            else
            {
                printf("Error: File not found\n\n");
            }
        }

        else if (strcmp(token[0], "quit") == 0)
        {
            if (fp)
            {

                fclose(fp);
                fp = NULL;
            }
            exit(0);
        }

        free(working_root);
    }

    return 0;
}
