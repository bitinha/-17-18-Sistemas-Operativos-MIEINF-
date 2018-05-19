#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "readline.h"
#include "stringlist.h"

#define TAM 4*1024

char *pathsave;

void interromper(int s){
    unlink(pathsave);
    exit(1);
}

void executa_programas(char *linha){

    int npipes = 0, pid;

    for(int i = 0; linha[i]; i++){
        if(linha[i] == '|') npipes++;
    }

    wordexp_t words;

    if(npipes==0){
        wordexp(linha, &words, 0);
        execvp(words.we_wordv[0], words.we_wordv);
        kill(0,SIGINT);
    }

    int pfds[npipes][2];

    //Cria as pipes necessárias
    for(int i = 0; i<npipes; i++){
        if(pipe(pfds[i]) == -1) kill(0, SIGINT);
    }

    char *comando = strsep(&linha,"|");

    // Criação de um processo para executar o 1º programa que irá ler do stdin e escrever no 1º pipeline
    pid = fork();
    if(!pid){
        close(pfds[0][0]);
        dup2(pfds[0][1],1);
        close(pfds[0][1]);
        wordexp(comando, &words, 0);
        execvp(words.we_wordv[0], words.we_wordv);
        kill(0,SIGINT);
    }
    if(pid<0) kill(0, SIGINT);

    
    // O primeiro processo é o único que escreveria no 1º pipeline, por isso fecha-se para os restantes processos
    close(pfds[0][1]);


    // Ciclo que irá criar os processos que irão ler de um pipeline e escrever no seguinte
    for(int i = 1; i < npipes; i++){
        comando = strsep(&linha,"|");
        pid = fork();
        if(!pid){
            dup2(pfds[i-1][0],0);
            close(pfds[i-1][0]);
            dup2(pfds[i][1],1);
            close(pfds[i][1]);
            wordexp(comando, &words, 0);
            execvp(words.we_wordv[0], words.we_wordv);
            kill(0,SIGINT);
        }
        if(pid<0) kill(0, SIGINT);
        close(pfds[i-1][0]);
        close(pfds[i][1]);
    }

    
    // Último processo (pai de todos os outros) irá executar o último programa que irá ler do último pipeline e escrever no stdout
    dup2(pfds[npipes-1][0],0);
    close(pfds[npipes-1][0]);
    comando = strsep(&linha,"|");
    wordexp(comando, &words, 0);
    execvp(words.we_wordv[0], words.we_wordv);
    kill(0,SIGINT);


}

/** Devolve o nº de processos criados*/
int processa_linha(char *linha, int pfds[2], int *nprocessos, LRES output){

    int i, r=0, p=0, tamint=0, overhead=1;


    if(linha[0] == '$'){

        if(linha[1]=='|'){              //Verifica se este comando deve ir buscar o resultado do ultimo
            p=1;
            overhead=2;
        }
        else if(p=atoi(linha+1)){       //Verifica se este comando deve ir buscar o resultado doutro comando
            if(p>0){
                tamint=snprintf(NULL,0,"%d",p);
                if(linha[1 + tamint] != '|'){
                    p = 0; 
                }else{
                    overhead += tamint + 1;
                }
            }
            else p = 0;
        }
        // for (i = 0; linha[i]!='\n' && linha[i] != 0; ++i){
        //     char c = linha[i];
        //     if(c=='|' || c=='&' || c==';' || c=='<' || c=='>' || c=='(' || c==')' || c=='{' || c=='}'){
        //         linha[i]=' ';
        //     }
        // }

        char *linha_mod = strdup(linha+overhead);
        linha_mod[strlen(linha_mod)-1]=0;       //Substitui \n por \0


        r++;
        (*nprocessos)++;
        pipe(pfds);

        int pid = fork();

        if(!pid){
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
            executa_programas(linha_mod);
        }

        if(pid<0) kill(0,SIGINT);

        close(pfds[1]);

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

    pathsave = malloc(strlen(argv[1])+6);      // ".temp"(6 bytes)
    strcpy(pathsave, argv[1]);
    strcat(pathsave, ".temp");
    if((nsave=creat(pathsave, sb.st_mode)) == -1){
        perror("Não foi possível abrir o ficheiro");
        exit(2);
    }

    signal(SIGINT, interromper);


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