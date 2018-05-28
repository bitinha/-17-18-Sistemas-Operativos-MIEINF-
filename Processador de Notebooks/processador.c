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
//alterar para que apenas se faça kill no pai em todos os filhos e apenas o pai trate de fechar
char *pathsave;

void interromper(int s){
    unlink(pathsave);
    _exit(-1);
}

//Devolve 0 em caso de sucesso
int executa_programas(char *linha){

    int npipes = 0, pid, res;

    for(int i = 0; linha[i]; i++){
        if(linha[i] == '|') npipes++;
    }

    wordexp_t words;

    if(npipes==0){      //Caso em que se executa apenas um comando
        pid=fork();
        if(!pid){
            wordexp(linha, &words, 0);
            execvp(words.we_wordv[0], words.we_wordv);
            _exit(-1);
        }
        if(pid<0) return -1;

        wait(&res);
        if (WIFEXITED(res)){
            res = WEXITSTATUS(res);
            return res;
        }else return -1;
    }

    int pfds[npipes][2];
    int num_proc = npipes + 1;      //Número de processos a serem criados
    int pid_list[num_proc];         //Array com os pids dos processos criados

    //Cria as pipes necessárias
    for(int i = 0; i<npipes; i++){
        if(pipe(pfds[i]) == -1) return -1;
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
        _exit(-1);
    }
    // O primeiro processo é o único que escreveria no 1º pipeline, por isso fecha-se para os restantes processos
    close(pfds[0][1]);

    if(pid<0) return -1;
    pid_list[0]=pid;


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
            _exit(-1);
        }
        if(pid<0){
            for(int j=0;j<i;j++){       //KILL NOS PROCESSOS JÁ CRIADOS
                kill(pid_list[j],SIGKILL);
            }
            return -1;
        }
        pid_list[i] = pid;              //ADICIONA PID A LISTA
        close(pfds[i-1][0]);
        close(pfds[i][1]);
    }

    pid = fork();
    // Último processo a ser criado irá executar o último programa que irá ler do último pipeline e escrever no stdout
    if(!pid){
        dup2(pfds[npipes-1][0],0);
        close(pfds[npipes-1][0]);
        comando = strsep(&linha,"|");
        wordexp(comando, &words, 0);
        execvp(words.we_wordv[0], words.we_wordv);
        _exit(-1);
    }
    if(pid<0){
        for(int j=0;j<num_proc-1;j++){      //KILL NOS PROCESSOS JÁ CRIADOS
            kill(pid_list[j],SIGKILL);
        }
        return -1;
    }
    pid_list[num_proc-1] = pid;         //ADICIONA PID A LISTA

    for(int i=0; i<num_proc; i++){
        waitpid(pid_list[i], &res, 0);
        if (WIFEXITED(res)){
            res = WEXITSTATUS(res);
            if(res!=0){
                for(int j = i+1; j<num_proc; j++){        //Mata o resto
                    kill(pid_list[j], SIGKILL);
                }
                return -1;
            }
        }else{
            for(int j = i+1; j<num_proc; j++){        //Mata o resto
                kill(pid_list[j], SIGKILL);
            }
            return -1;
        }
    }

    return 0;

}

/** Devolve 0 caso existessem comandos para executar, 1 caso os comandos tenham sido executados com sucesso e -1 caso ocorresse algum erro*/
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
            int resexec = executa_programas(linha_mod);
            _exit(resexec);
        }

        close(pfds[1]);

        if(pid<0) return -1;


        wait(&r);
        if (WIFEXITED(r)){
            r = WEXITSTATUS(r);
            printf("%d\n", r);
            if(r!=0) return -1;
            else return 1;
        }else return -1;

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
            int rproc = processa_linha(linha, pfds, &processos, output);
            if(rproc == 1){
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
            else if(rproc == -1){
                interromper(SIGINT);
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