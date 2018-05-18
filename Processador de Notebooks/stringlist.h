
typedef struct lista_resultados{
    struct resultado *resultados;
    int nresultados;
} *LRES;

LRES create_lista_resultados(int n);
void destroy_lista_resultados(LRES output);
void adiciona_resultado(LRES output, int id, char *resultado, int lido);
int busca_resultado(LRES output, int id, char **resultado);