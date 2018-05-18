
#include "readline.h"
#include <stdlib.h>
#include <unistd.h>


int create_buffer(int fildes, struct buffer_t *buffer, size_t nbyte){
    buffer->buf = malloc(sizeof(char)*nbyte);

    buffer->fd = fildes;
    buffer->cursor = nbyte;
    buffer->nbyte = nbyte;
    buffer->porler = 0;
}

int destroy_buffer(struct buffer_t *buffer){
    free(buffer->buf);
}

/** Carrega em buf uma string com a a proxima linha do descritor de ficheiro associado ao buffer
    A string é terminada com '/n' e NULL
    Devolve o número de caracteres lidos para o buffer, incluindo '\n' e '\0'
*/
ssize_t readln(struct buffer_t *buffer, char **buf){

    int i=0;

    if(buffer->porler==0){                  // Se já não houver caracteres para ler no buffer, ler os proximos bytes para o buffer
        buffer->porler = read(buffer->fd, buffer->buf, buffer->nbyte);
        buffer->cursor = 0;
    }

    if (buffer->porler == 0) return 0;      // Devolve 0 caso o read anterior tenha devolvido 0

    char *ret = malloc(sizeof(char)*buffer->nbyte);
    int tamret = buffer->nbyte;

    while(buffer->porler != 0 && buffer->buf[buffer->cursor] != '\n'){
        ret[i++] = buffer->buf[buffer->cursor++];
        buffer->porler--;


        if(tamret==i){
            tamret *= 2;
            ret = realloc(ret, tamret);
        }

        if(buffer->porler==0){
            buffer->porler = read(buffer->fd, buffer->buf, buffer->nbyte);
            buffer->cursor = 0;
        }
    }

    if(tamret <= i+2){
        tamret += 2;
        ret = realloc(ret,tamret);
    }

    ret[i++] = '\n';
    ret[i++] = 0;

    *buf = ret;

    buffer->cursor++;
    if(buffer->porler > 0) buffer->porler--;

    return i;
}
