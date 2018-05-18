#include "stringlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct resultado{
    char *resultado;
    int tamanho;
};



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

/** Adiciona uma resultado ao array de resultados no indice fornecido, aumentando o tamanho do array se necessário
*/ 
void adiciona_resultado(LRES output, int id, char *resultado, int lido){
    while(output->nresultados <= id){
        output->nresultados *= 2;
        output->resultados = realloc(output->resultados, sizeof(struct resultado)*output->nresultados);
        for(int i = (output->nresultados)/2; i < output->nresultados; i++){
            output->resultados[i].resultado = NULL;
            output->resultados[i].tamanho = 0;
        }
    }

    output->resultados[id].resultado = strndup(resultado,lido);
    output->resultados[id].tamanho = lido;
}

int busca_resultado(LRES output, int id, char **resultado){
    *resultado = output->resultados[id].resultado;
    return output->resultados[id].tamanho;
}
