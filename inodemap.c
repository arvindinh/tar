#include "inodemap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char const ** Map=NULL;

void set_inode( ino_t i, const char * f ){
    uint32_t mappos = i % MAPSIZE;
    
    if( Map == NULL )
        Map = (const char  **)calloc(MAPSIZE, sizeof(char*));
    
    Map[mappos] = f;
}

const char * get_inode( ino_t i ){
    if( Map == NULL )
        Map = (const char  **)calloc(MAPSIZE, sizeof(char*));
    
    return Map[ i % MAPSIZE ];
}
