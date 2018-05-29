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

struct fila{
    int *elementos;
    int proximo;
    int existentes;
};

char *pathsave;
struct fila processos_criados;


void mata_processos(){
    while(processos_criados.proximo < processos_criados.existentes){
        kill(processos_criados.elementos[processos_criados.proximo++], SIGKILL);
    }
}

void interromper(int s){
    unlink(pathsave);
    mata_processos();
    _exit(-1);
}


//Devolve o descritor de ficheiro de onde pode ser lido o resultado ou -1 em caso de erro
int executa_programas(char *linha, char *res_ant, int tam_res_ant){

    wordexp_t words;
    int npipes = 0, pid, res, pipe_resultado[2];

    for(int i = 0; linha[i]; i++){
        if(linha[i] == '|') npipes++;
    }

    int num_proc = npipes + 1;      //Número de processos a serem criados

    //Estrutura sobre os processos filho
    processos_criados.elementos = malloc(num_proc*sizeof(int));         //Array com os pids dos processos criados
    processos_criados.proximo = 0;
    processos_criados.existentes = 0;


    if(npipes==0){      //Caso em que se executa apenas um comando
        pipe(pipe_resultado);
        pid=fork();
        if(!pid){
            dup2(pipe_resultado[1],1);
            if(tam_res_ant > 0){
                int ptemp[2];
                pipe(ptemp);
                dup2(ptemp[0],0);
                write(ptemp[1], res_ant, tam_res_ant);
                close(ptemp[1]);
                close(ptemp[0]);
            }
            close(pipe_resultado[1]);
            close(pipe_resultado[0]);
            wordexp(linha, &words, 0);
            execvp(words.we_wordv[0], words.we_wordv);
            _exit(-1);
        }
        close(pipe_resultado[1]);
        if(pid<0) return -1;
        processos_criados.elementos[processos_criados.existentes++] = pid;

        wait(&res);
        processos_criados.proximo++;
        if (WIFEXITED(res)){
            res = WEXITSTATUS(res);
            if(res==0) return pipe_resultado[0];
            return -1;
        }else return -1;
    }

    int pfds[npipes][2];

    //Cria as pipes necessárias
    for(int i = 0; i<npipes; i++){
        if(pipe(pfds[i]) == -1) return -1;
    }

    char *comando = strsep(&linha,"|");

    // Criação de um processo para executar o 1º programa que irá ler do stdin e escrever no 1º pipeline
    pid = fork();
    if(!pid){
        if(tam_res_ant > 0){
            int ptemp[2];
            pipe(ptemp);
            dup2(ptemp[0],0);
            write(ptemp[1], res_ant, tam_res_ant);
            close(ptemp[1]);
            close(ptemp[0]);
        }
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
    processos_criados.elementos[processos_criados.existentes++] = pid;


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
            mata_processos();       //KILL NOS PROCESSOS JÁ CRIADOS
            return -1;
        }
        processos_criados.elementos[processos_criados.existentes++] = pid;//ADICIONA PID A LISTA
        close(pfds[i-1][0]);
        close(pfds[i][1]);
    }


    pipe(pipe_resultado);
    pid = fork();
    // Último processo a ser criado irá executar o último programa que irá ler do último pipeline e escrever no stdout
    if(!pid){
            dup2(pipe_resultado[1],1);
            close(pipe_resultado[1]);
            close(pipe_resultado[0]);
        dup2(pfds[npipes-1][0],0);
        close(pfds[npipes-1][0]);
        comando = strsep(&linha,"|");
        wordexp(comando, &words, 0);
        execvp(words.we_wordv[0], words.we_wordv);
        _exit(-1);
    }
    close(pipe_resultado[1]);
    if(pid<0){
        mata_processos();      //KILL NOS PROCESSOS JÁ CRIADOS
        return -1;
    }
    processos_criados.elementos[processos_criados.existentes++] = pid;         //ADICIONA PID A LISTA

    for(int i=0; i<num_proc; i++){
        waitpid(processos_criados.elementos[i], &res, 0);
        processos_criados.proximo++;
        if (WIFEXITED(res)){
            res = WEXITSTATUS(res);
            if(res!=0){
                return -1;
            }
        }else{
            return -1;
        }
    }

    return pipe_resultado[0];

}


/** Devolve o descritor de ficheiro de onde pode ser lido o resulado caso a execução tenha sido bem sucedida,
    0 caso não existessem comandos para executar e -1 caso ocorresse algum erro*/
int processa_linha(char *linha, int *ncomando, LRES output){

    int i, p=0, tamint=0, overhead=1;


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

        if(p>*ncomando) return -1;               //Comando impossível

        char *linha_mod = strdup(linha+overhead);
        linha_mod[strlen(linha_mod)-1]=0;       //Substitui \n por \0


        (*ncomando)++;

        int tam_res_ant = 0;
        char *res_ant = NULL;
        if(p>0){
            tam_res_ant = busca_resultado(output, (*ncomando)-p, &res_ant);
        }

        int fd = executa_programas(linha_mod, res_ant, tam_res_ant);
        return fd;
    }

    return 0;
}


int main(int argc, char const *argv[]){
    if(argc<2){
        printf("Indique o ficheiro a processar ao chamar este programa\n");
        exit(1);
    }

    processos_criados.proximo = 0;
    processos_criados.existentes = 0;
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
    int ncomando=0;
    LRES output = create_lista_resultados(32);
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
            int fd_res = processa_linha(linha, &ncomando, output);
            if(fd_res > 0){
                for(lido = read(fd_res, temp, tamtemp); lido == tamtemp; lido += read(fd_res, temp + (tamtemp/2), tamtemp/2)){
                    tamtemp *= 2;
                    temp = realloc(temp, tamtemp);
                }
                close(fd_res);
                adiciona_resultado(output, ncomando, temp, lido);
                write(nsave, ">>>\n", 4);
                write(nsave, temp, lido);
                write(nsave, "<<<\n", 4);
            }
            else if(fd_res == -1){
                interromper(SIGINT);
            }
        }
        free(linha);
    }

    free(temp);


    destroy_buffer(&buffer);
    destroy_lista_resultados(output);

    rename(pathsave, argv[1]);
    free(pathsave);
    close(notebook);
    close(nsave);


    return 0;
}