#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define TAM 4*1024

struct buffer_t{
    int fd;
    char* buf;
    size_t nbyte, porler;
    int cursor;
};

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


struct resultado{
    char *resultado;
    int tamanho;
};


typedef struct lista_resultados{
    struct resultado *resultados;
    int nresultados;
} *LRES;

LRES create_lista_resultados(int n){
    LRES output = (LRES) malloc(sizeof(struct lista_resultados));
    output->nresultados = n;
    output->resultados = (struct resultado *) malloc(sizeof(struct resultado)*n);

    for(int i = 0; i < n; i++){
        output->resultados[i].resultado = NULL;
        output->resultados[i].tamanho = 0;
    }

    return output;
}

void destroy_lista_resultados(LRES output){
    for(int i = 0; i < output->nresultados; i++){
        free(output->resultados[i].resultado);
    }

    free(output->resultados);
    free(output);
}


void adiciona_resultado(LRES output, int id, char *temp, int lido){
    while(output->nresultados <= id){
        output->nresultados *= 2;
        output->resultados = realloc(output->resultados, sizeof(struct resultado)*output->nresultados);
        for(int i = (output->nresultados)/2; i < output->nresultados; i++){
            output->resultados[i].resultado = NULL;
            output->resultados[i].tamanho = 0;
        }
    }

    output->resultados[id].resultado = strndup(temp,lido);
    output->resultados[id].tamanho = lido;
}


/** Devolve o ñº de processos criados*/
int processa_linha(char *linha, int pfds[2], int *nprocessos, LRES output){

    int i, r=0, p=0, tamint=0;


    if(linha[0] == '$'){

        if(linha[1]=='|') p=1;
        else if(p=atoi(linha+1)){
            if(p>0){
                tamint=snprintf(NULL,0,"%d",p);
                if(linha[1 + tamint] != '|') p = 0; 
            }
            else p = 0;
        }
        // for (i = 0; linha[i]!='\n' && linha[i] != 0; ++i){
        //     char c = linha[i];
        //     if(c=='|' || c=='&' || c==';' || c=='<' || c=='>' || c=='(' || c==')' || c=='{' || c=='}'){
        //         linha[i]=' ';
        //     }
        // }
        linha[strlen(linha)-1]=0;

        wordexp_t words;
        wordexp(linha+2+tamint, &words, 0);

        r++;
        (*nprocessos)++;
        pipe(pfds);

        if(!fork()){
            dup2(pfds[1],1);
            if(p>=1){
                int ptemp[2];
                pipe(ptemp);
                dup2(ptemp[0],0);
                write(ptemp[1], output->resultados[(*nprocessos)-p].resultado, output->resultados[(*nprocessos)-p].tamanho);
                close(ptemp[1]);
                close(ptemp[0]);
            }
            close(pfds[1]);
            close(pfds[0]);
            execvp(words.we_wordv[0], words.we_wordv);
        }
        close(pfds[1]);
        wordfree(&words);

    }

    return r;
}


int main(int argc, char const *argv[]){
    if(argc<2){
        printf("Indique o ficheiro a processar ao chamar este programa\n");
        exit(1);
    }

    int notebook, nsave;

    if((notebook=open(argv[1],O_RDONLY)) == -1){
        perror("Não foi possível abrir o ficheiro");
        exit(2);
    }

    struct stat sb;
    if(fstat(notebook,&sb) == -1) exit(2);

    char *pathsave = malloc(strlen(argv[1])+6);      // ".temp"(6 bytes)
    strcpy(pathsave, argv[1]);
    strcat(pathsave, ".temp");
    if((nsave=creat(pathsave, sb.st_mode)) == -1){
        perror("Não foi possível abrir o ficheiro");
        exit(2);
    }

    char *linha = NULL;
    size_t len = 0;
    struct buffer_t buffer;
    int processos=0;
    int pfds[2];
    //struct lista_resultados output;
    LRES output = create_lista_resultados(32);
    //output.resultados = malloc(32*sizeof(struct resultado));
    ssize_t lido;
    char *temp = malloc(sizeof(char)*TAM);
    int tamtemp = TAM;


    create_buffer(notebook, &buffer, TAM);


    while((len = readln(&buffer, &linha))!=0){
        if(strcmp(linha, ">>>\n") == 0){
            free(linha);
            while((len = readln(&buffer, &linha))!=0 && strcmp(linha, "<<<\n")) free(linha);
        }else{
            write(nsave, linha, len-1);
            if(processa_linha(linha, pfds, &processos, output) > 0){
                wait(NULL);
                for(lido = read(pfds[0], temp, tamtemp); lido == tamtemp; lido += read(pfds[0], temp + (tamtemp/2), tamtemp/2)){
                    tamtemp *= 2;
                    temp = realloc(temp, tamtemp);
                }
                close(pfds[0]);
                adiciona_resultado(output, processos, temp, lido);
                write(nsave, ">>>\n", 4);
                write(nsave, temp, lido);
                write(nsave, "<<<\n", 4);
            }
        }
    free(linha);
    }

    free(temp);

/*
    for(int i=1;i<=processos;i++){
        printf("%d\n", i);
        printf("%s\n", output.resultados[i].resultado);
    }*/

    destroy_buffer(&buffer);
    destroy_lista_resultados(output);

    rename(pathsave, argv[1]);
    free(pathsave);
    close(notebook);
    close(nsave);


    return 0;
}