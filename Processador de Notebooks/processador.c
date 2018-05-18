#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "readline.h"
#include "stringlist.h"

#define TAM 4*1024


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
                char *res_ant;
                int tam_res_ant = busca_resultado(output, (*nprocessos)-p, &res_ant);
                write(ptemp[1], res_ant, tam_res_ant);
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