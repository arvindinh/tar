#include "inodemap.h"
#include<getopt.h>
#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<sys/stat.h>
#include<dirent.h>
#include<unistd.h>
#include<sys/time.h>


//2d array to store inodes
char const ** Map;

//DFS recursively traverses the directory tree with two inputs, the tar file name that's being written into, and the name of the current directory being explored
void DFS (FILE *fp, char *dirname) {
    struct stat buf;
    DIR *dir;
    struct dirent* file;

    //2d array to store pointers of each name malloc'd(which is then stored in the inode map)
    //used to keep track and free later
    char **names = (char **) malloc(sizeof(char*));
    int storageSize = 1;
    int numStored = 0;

    if ((dir = opendir(dirname)) != NULL) {
        while ((file = readdir(dir)) != NULL) {
            if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
                //get path by appending appending current directory to name
                unsigned int dirlen = strlen(dirname);
                unsigned int fnamelen = strlen(file->d_name);
                unsigned int totlen = dirlen+fnamelen+1;
                char *name = (char*) malloc(sizeof(char) * totlen + 1);
                strcpy(name, dirname);
                strcat(name, "/");
                strcat(name, file->d_name);
                name[totlen] = '\0';

                //dynamically update size of storage arr
                names[numStored] = name;
                numStored++;
                if (numStored == storageSize) {
                    storageSize *= 2;
                    names = realloc(names, storageSize * sizeof(char*));
                }
                
                //check to see what type of file is encountered
                if (lstat(name, &buf) < 0) {
                    perror("stat");
                    free(name);
                    closedir(dir);
                    fclose(fp);
                    exit(-1);
                }

                //cases- regular(hard link falls under this category), or directory
                if (S_ISDIR(buf.st_mode) || S_ISREG(buf.st_mode)) {
                    //write file inode number, name length, name no matter what kind of file it is
                    if (fwrite(&buf.st_ino, sizeof(buf.st_ino), 1, fp) != 1) {
                        perror("fwrite");
                        free(name);
                        closedir(dir);
                        fclose(fp);
                        exit(-1);
                    }
                    if (fwrite(&totlen, sizeof(totlen), 1, fp) != 1) {
                        perror("fwrite");
                        free(name);
                        closedir(dir);
                        fclose(fp);
                        exit(-1);
                    }
                    if (fprintf(fp, "%s", name) < 0) {
                        perror("fprintf");
                        free(name);
                        closedir(dir);
                        fclose(fp);
                        exit(-1);
                    }

                    //start narrowing down. is it a directory?
                    if (S_ISDIR(buf.st_mode)) {
                        //write the file mode
                        if (fwrite(&buf.st_mode, sizeof(buf.st_mode), 1, fp) != 1) {
                            perror("fwrite");
                            free(name);
                            closedir(dir);
                            fclose(fp);
                            exit(-1);
                        }
                        //write the file modification time
                        if (fwrite(&buf.st_mtime, sizeof(buf.st_mtime), 1, fp) != 1) {
                            perror("fwrite");
                            free(name);
                            closedir(dir);
                            fclose(fp);
                            exit(-1);
                        }
                        //after writing current directory info, traverse it
                        DFS(fp, name);
                    } else {
                        //case for regular file
                        if (get_inode(buf.st_ino) == NULL) {
                            set_inode(buf.st_ino, file->d_name);
                            if (fwrite(&buf.st_mode, sizeof(buf.st_mode), 1, fp)  != 1) {
                                perror("fwrite");
                                free(name);
                                closedir(dir);
                                fclose(fp);
                                exit(-1);
                            }

                            if (fwrite(&buf.st_mtime, sizeof(buf.st_mtime), 1, fp) != 1) {
                                perror("fwrite");
                                free(name);
                                closedir(dir);
                                fclose(fp);
                                exit(-1);
                            }

                            if (fwrite(&buf.st_size, sizeof(buf.st_size), 1, fp) != 1) {
                                perror("fwrite");
                                free(name);
                                closedir(dir);
                                fclose(fp);
                                exit(-1);
                            }

                            //write contents by reading it in, store in buffer, and writing it
                            FILE *curr = fopen(name, "r");
                            char c;
                            if (curr == NULL) {
                                perror("fopen");
                                free(name);
                                closedir(dir);
                                fclose(fp);
                                exit(-1);
                            }
                            char buffer[buf.st_size];
                            fread(buffer, 1, buf.st_size, curr);
                            fwrite(buffer, 1, buf.st_size, fp);
                            fclose(curr);
                        }
                    }

                }

            }


        }
        for (int i = 0; i < numStored; i++) {
            free(names[i]);
        }
        free(names);
        closedir(dir);
    } else {
        perror("opendir");
        exit(-1);
    }
}

//write relevant info to specified file
void createArchive(char *filename, char *dirname) {
    //get info about the directory(root) before traversing its children
    struct stat buf;
    if (stat(dirname, &buf) < 0) {
        fprintf(stderr, "Specified target(\"%s\") does not exist.\n", dirname);
        exit(-1);
    }
    if (!S_ISDIR(buf.st_mode)) {
        fprintf(stderr, "Specified target(\"%s\" is not a directory.\n)", dirname);
        exit(-1);
    }

    //write magic number before traversing
    FILE *fd; 
    fd = fopen(filename, "wb");
    unsigned int magic = 0x7261746D;

    //if fopen fails, do not continue
    if (fd == NULL) {
        perror("fopen");
        exit(-1);
    }

    if (fwrite(&magic, sizeof(magic), 1, fd) != 1) {
        perror("fwrite");
        fclose(fd);
        exit(-1);
    }

    //write the info about the directory first before traversing
    if (fwrite(&buf.st_ino, sizeof(buf.st_ino), 1, fd) != 1) {
        perror("fwrite");
        fclose(fd);
        exit(-1);
    }
    unsigned int len = strlen(dirname);
    if (fwrite(&len, sizeof(len), 1, fd) != 1) {
        perror("fwrite");
        fclose(fd);
        exit(-1);
    }
    if (fprintf(fd, "%s", dirname) < 0) {
        perror("fprintf");
        fclose(fd);
        exit(-1);
    }
    
    if (fwrite(&buf.st_mode, sizeof(buf.st_mode), 1, fd) != 1) {
        perror("fwrite");
        fclose(fd);
        exit(-1);
    }
    //write the file modification time
    if (fwrite(&buf.st_mtime, sizeof(buf.st_mtime), 1, fd) != 1) {
        perror("fwrite");
        fclose(fd);
        exit(-1);
    }

    DFS(fd, dirname);
    free(Map);
    fclose(fd);
}

//prints info about specified tar file
void printArchive(char *filename) {
    FILE *fd; 
    fd = fopen(filename, "rb");
    if (fd == NULL) {
        perror("fopen");
        exit(-1);
    }

    //magic number first 4 bytes in file, so read it first and see if correct
    unsigned int magic;
    unsigned int correctmagic = 0x7261746D;
    if (fread(&magic, sizeof(magic), 1, fd) == 1) {
        if (magic != correctmagic) {
            fprintf(stderr, "Bad magic number (%d), should be: %d.\n", magic, correctmagic);
        }
    } else {
        perror("fread");
        fclose(fd); 
        exit(-1);
    }

    //keep track of allocated names
    char **names = (char **) malloc(sizeof(char*));
    int storageSize = 1;
    int numStored = 0;

    while (!feof(fd)) {
        //all files will have inode, name length, and name, so read and store
        unsigned long long inode;
        if (fread(&inode, sizeof(inode), 1, fd) != 1) {
            //after reaching end, if you try to read, it will hit eof, so break
            if (feof(fd)) {
                break;
            }
            perror("fread");
            fclose(fd);
            exit(-1);
        }
        
        unsigned int namelength;
        if (fread(&namelength, sizeof(namelength), 1, fd) != 1) {
            perror("fread");
            fclose(fd);
            exit(-1);
        }

        char *name = (char *) malloc(sizeof(char) * (namelength + 1));
        if (fread(name, sizeof(char), namelength, fd) != namelength) {
            perror("fread");
            fclose(fd);
            exit(-1);
        }
        name[namelength] = '\0';
        names[numStored] = name;
        numStored++;
        if (numStored == storageSize) {
            storageSize *= 2;
            names = realloc(names, storageSize * sizeof(char*));
        }

        //if inode has not been encountered before, it is a not hard link
        if (get_inode(inode) == NULL) {
            set_inode(inode, name);
            unsigned int mode;
            //get the octal representation of the mode
            char octalString[10];
            if (fread(&mode, sizeof(mode), 1, fd) == 1) {
                sprintf(octalString, "%o", mode);
            } else {
                perror("fread");
                free(name);
                fclose(fd);
                exit(-1);
            }

            //get modification time
            unsigned long long modtime;
            if (fread(&modtime, sizeof(modtime), 1, fd) != 1) {
                perror("fread");
                free(name);
                fclose(fd);
                exit(-1);
            }
            //regular file if first digit of mode is 1
            if (octalString[0] == '1') {
                unsigned long long sz;
                if (fread(&sz, sizeof(sz), 1, fd) != 1) {
                    perror("fread");
                    free(name);
                    fclose(fd);
                    exit(-1);
                }
                //increment pointer past the file contents
                if (fseek(fd, sz, SEEK_CUR) != 0) {
                    perror("fread");
                    free(name);
                    fclose(fd);
                    exit(-1);
                }
                char lastdigit = octalString[strlen(octalString) - 1];
                //any combination including executable permission for everyone(last digit)
                if (lastdigit == '1' || lastdigit == '3' || lastdigit == '5' || lastdigit == '7') {
                    //print executable file info
                    printf("%s* -- inode: %llu, mode: %o, mtime: %llu, size: %llu\n", name, inode, mode% 01000, modtime, sz);
                } else {
                    //print regular file info
                    printf("%s -- inode: %llu, mode: %o, mtime: %llu, size: %llu\n", name, inode, mode% 01000, modtime, sz);
                }
            } else {
                //print directory info
                printf("%s/ -- inode: %llu, mode: %o, mtime: %llu\n", name, inode, mode% 01000, modtime);
            }
        } else {
            //print hard link info
            printf("%s/ -- inode: %llu\n", name, inode);
        }
    }
    for (int i = 0; i < numStored; i++) {
        free(names[i]);
    }
    free(names);
    free(Map);
    fclose(fd);
}

//create archived files in tar 
void extractArchive(char *filename) {
    FILE *fd;
    fd = fopen(filename, "rb");
    if (fd == NULL) {
        perror("fopen");
        exit(-1);
    }

    unsigned int magic;
    unsigned int correctmagic = 0x7261746D;
    if (fread(&magic, sizeof(magic), 1, fd) == 1) {
        if (magic != correctmagic) {
            fprintf(stderr, "Bad magic number (%d), should be: %d.\n", magic, correctmagic);
        }
    } else {
        perror("fread");
        fclose(fd); 
        exit(-1);
    }


    //store pointers of malloc'd names for freeing later
    char **names = (char **) malloc(sizeof(char*));
    int storageSize = 1;
    int numStored = 0;

    while (!feof(fd)) {
        //all files will have inode, name length, and name, so read and store
        unsigned long long inode;
        if (fread(&inode, sizeof(inode), 1, fd) != 1) {
            //after reaching end, if you try to read, it will hit eof, so break
            if (feof(fd)) {
                break;
            }
            perror("fread");
            fclose(fd);
            exit(-1);
        }

        unsigned int namelength;
        if (fread(&namelength, sizeof(namelength), 1, fd) != 1) {
            perror("fread");
            fclose(fd);
            exit(-1);
        }

        char *name = (char *) malloc(sizeof(char) * namelength + 1);
        if (fread(name, sizeof(char), namelength, fd) != namelength) {
            perror("fread");
            fclose(fd);
            exit(-1);
        }
        name[namelength] = '\0';
        
        names[numStored] = name;
        numStored++;
        if (numStored == storageSize) {
            storageSize *= 2;
            names = realloc(names, storageSize * sizeof(char*));
        }
       

        //if inode has not been encountered before, it is not a hard link
        if (get_inode(inode) == NULL) {
            set_inode(inode, name);
            unsigned int mode;
            //get the octal representation of the mode
            char octalString[10];
            if (fread(&mode, sizeof(mode), 1, fd) == 1) {
                sprintf(octalString, "%o", mode);
            } else {
                perror("fread");
                free(name);
                fclose(fd);
                exit(-1);
            }

            //get modification time
            unsigned long long modtime;
            if (fread(&modtime, sizeof(modtime), 1, fd) != 1) {
                perror("fread");
                free(name);
                fclose(fd);
                exit(-1);
            }

            //if regular file, fopen 
            if (octalString[0] == '1') {
                unsigned long long sz;
                if (fread(&sz, sizeof(sz), 1, fd) != 1) {
                    perror("fread");
                    free(name);
                    fclose(fd);
                    exit(-1);
                }

                FILE *fp = fopen(name, "wb");
                if (fd == NULL) {
                    perror("fopen");
                    free(name);
                    free(Map);
                    fclose(fd);
                    exit(-1);
                }
                char buffer[sz];
                fread(buffer, 1, sz, fd);
                fwrite(buffer, 1, sz, fp);
                if (chmod(name, mode) == -1) {
                    perror("chmod");
                    free(name);
                    free(Map);
                    fclose(fp);
                    fclose(fd);
                    exit(-1);
                }

                struct timeval time[2];
                gettimeofday(&time[0], NULL);
                time[1].tv_sec = modtime;
                time[1].tv_usec = 0; 

                if (utimes(name, time) == -1) {
                    perror("utimes");
                    free(name);
                    free(Map);
                    fclose(fp);
                    fclose(fd);
                    exit(-1);
                }
                fclose(fp);
            } else {
                //make directory
                mkdir(name, mode);
            }    
        } else {
            //make hard link
            if (link(get_inode(inode), name) == -1) {
                perror("link");
                free(name);
                free(Map);
                exit(-1);
            }
        }
        
    }
    for (int i = 0; i < numStored; i++) {
        free(names[i]);
    }
    free(names);
    free(Map);
    fclose(fd);
}

int main(int argc, char *argv[]) {
    int c;
    char *filename;
    char *dirname;
    int i = 0;
    int option;
    int optFlag = 0;
    int fileFlag = 0;
    int dirFlag = 0;
    while ((c = getopt(argc, argv, "cxtf:")) != -1) {
        switch(c) {
            case 'c':
                optFlag++;
                option = 'c';
                break;
            case 'x':
                optFlag++;
                option = 'x';
                break;
            case 't':
                optFlag++;
                option = 't';
                break;
            case 'f':
                fileFlag = 1;
                while (optarg[i] != '\0') {
                i++;
                }
                filename = (char*) malloc(i * sizeof(char) + 1);
                strcpy(filename, optarg);
                i = 0;
                break;
        }

    }

    if (optFlag == 0) {
        fprintf(stderr, "Error: No mode specified.\n");
        exit(-1);
    }

    if (fileFlag == 0) {
        fprintf(stderr, "No tarfile specified.\n");
        exit(-1);
    }

    if (optFlag > 1) {
        fprintf(stderr, "Error: Multiple modes specified. \n");
        if (fileFlag == 1) {
            free(filename);
        }
        exit(-1);
    }
    

    switch(option) {
            case 'c':
                if (optind < argc) {
                dirFlag = 1;
                char * s = argv[optind];
                while(s[i] != '\0') {
                    i++;
                }
                dirname = (char*) malloc(i * sizeof(char) + 1);
                strcpy(dirname, s);
                i = 0;
                } else {
                    //no directory specified
                    fprintf(stderr, "Error: No directory target specified.\n");
                    if (fileFlag == 1) {
                            free(filename);
                    }
                    exit(1);
                }
                createArchive(filename, dirname);
                break;
            case 'x':
                extractArchive(filename);
                break;
            case 't':
                printArchive(filename);
                break;
        }
    if (fileFlag == 1) {
        free(filename);
    }
    if (dirFlag == 1) {
        free(dirname);
    }
    exit(0);
}