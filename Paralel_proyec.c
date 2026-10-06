#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>  // Para sysconf
#include <time.h>    
#include <math.h>    

#define MAX_PASSWORD_LENGTH 100

// Estructura para los datos compartidos y el identificador de hilo
typedef struct {

    char* target_password;
    int mode;
    int length;
    int found;
    pthread_mutex_t lock;
    pthread_cond_t cond;  
    int thread_id;  
    long start_hilo;
    long end_hilo;  
    int* imprimir_contrasenas; 

} Datos_Hilos;

// Verifica si la contraseña es válida según el modo
int mode_validate(const char* password, int modo) {

    while (*password) {

        char c = *password;

        switch (modo) {

            case 1:
                if (!isdigit(c)) return 0; // Modo 1: Solo números
                break;

            case 2:
                if (!isdigit(c) && !islower(c)) return 0; // Modo 2: Números y letras minúsculas
                break;

            case 3:
                if (!isdigit(c) && !islower(c) && !isupper(c)) return 0; // Modo 3: Números, letras minúsculas y mayúsculas
                break;

            case 4:
                if (!isdigit(c) && !islower(c) && !isupper(c) && !ispunct(c)) return 0; // Modo 4: Números, letras y caracteres especiales
                break;

            default:
                return 0; // Modo inválido
        }

        password++;

    }

    return 1;

}

// Obtiene el conjunto de caracteres según el modo
const char* characters(int mode) {

    switch (mode) {

        case 1: return "0123456789"; // Modo 1: Solo números

        case 2: return "0123456789abcdefghijklmnopqrstuvwxyz"; // Modo 2: Números y letras minúsculas

        case 3: return "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"; // Modo 3: Números, letras minúsculas y mayúsculas

        case 4: return "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()+¡_?¿-.,"; // Modo 4: Números, letras y caracteres especiales

        default: return "";

    }

}

// Genera la contraseña a partir de un numero
void generate_password(char* password, long num_ind, const char* chars_mode, int chars_mode_cant, int length) {

    for (int i = length - 1; i >= 0; i--) { 

        password[i] = chars_mode[num_ind % chars_mode_cant];
        num_ind /= chars_mode_cant;

    }
    password[length] = '\0';
}

// Prueba un rango de contraseñas
void passwords_verfication(long start, long end, const char* chars_mode, int chars_mode_cant, Datos_Hilos* data) {

    char password_generate[MAX_PASSWORD_LENGTH];
    
    for (long i = start; i < end; i++) {

        pthread_mutex_lock(&data->lock);

        // Verifica si la contraseña ya fue encontrada
        if (data->found) {

            pthread_mutex_unlock(&data->lock);
            return;  

        }

        // Genera una nueva contraseña a partir del numero de num_ind
        generate_password(password_generate, i, chars_mode, chars_mode_cant, data->length);

        // Imprime la contraseña si la opción está activada y no se encontró la contraseña
        if (*(data->imprimir_contrasenas) && !data->found) {

            printf("Hilo %d probando: %s\n", data->thread_id, password_generate);

        }
        
        // Compara la contraseña generada con la contraseña objetivo
        if (strcmp(password_generate, data->target_password) == 0) {
            data->found = 1; 
            *(data->imprimir_contrasenas) = 0; 
            printf("Hilo %d encontró la contraseña: %s\n", data->thread_id, password_generate);

            pthread_cond_broadcast(&data->cond); // Notifica a otros hilos para que se detengan
            pthread_mutex_unlock(&data->lock);
            return;  
        }
        pthread_mutex_unlock(&data->lock);
    }

}

// Ejecuta los hilos
void* brute_force(void* arg) {

    Datos_Hilos* data = (Datos_Hilos*)arg;
    const char* chars_mode = characters(data->mode);
    int chars_mode_cant = strlen(chars_mode);

    // Genera contraseñas en el rango asignado
    passwords_verfication(data->start_hilo, data->end_hilo, chars_mode, chars_mode_cant, data);

    pthread_exit(NULL);

}

int main() {

    FILE* archivo = fopen("password.txt", "r");

    if (archivo == NULL) {

        printf("No se pudo abrir el archivo.\n");
        return 1;

    }

    int modo, largo;
    char password[100];
    
    // Leer los datos del archivo
    fscanf(archivo, "Modo: %d\n", &modo);
    fscanf(archivo, "Largo: %d\n", &largo);
    fscanf(archivo, "Contraseña: %s\n", password); 
    int imprimir_contrasenas = 0; 
    fscanf(archivo, "Imprimir: %d\n", &imprimir_contrasenas);
    fclose(archivo);

    // Verifica el largo de la contraseña
    if (largo > 10) {

        printf("La contraseña excede el largo permitido (máximo 10 caracteres).\n");
        return 1;

    }

    // Verifica que la contraseña sea válida según el modo
    if (!mode_validate(password, modo)) {

        printf("La contraseña no coincide con el modo especificado.\n");
        return 1;

    }

    // Cantidad de procesadores lógicos de la compu
    int procesadores_logicos = sysconf(_SC_NPROCESSORS_ONLN);

    if (procesadores_logicos < 1) {

        printf("Error al obtener la cantidad de procesadores lógicos.\n");
        return 1;

    }

    printf("Cantidad de procesadores lógicos disponibles: %d\n", procesadores_logicos);

    // Verifica que la contraseña sea válida
    printf("La contraseña es válida para el modo %d y tiene el largo %d.\n", modo, largo);

    const char* chars_mode = characters(modo);
    int chars_mode_cant = strlen(chars_mode);

    // Calcula el total de combinaciones posibles
    long total_combinaciones = pow(chars_mode_cant, largo);
    printf("Total de combinaciones posibles: %ld\n", total_combinaciones);

    // Inicia los datos para cada hilo
    Datos_Hilos data[procesadores_logicos];
    pthread_t hilos[procesadores_logicos];

    long combinaciones_hilo = total_combinaciones / procesadores_logicos;
    long combinaciones_restantes = total_combinaciones % procesadores_logicos;

    // Se le asigna un rango a cada hilo
    for (int i = 0; i < procesadores_logicos; i++) {
        long inicio = i * combinaciones_hilo;
        long fin = (i == procesadores_logicos - 1) ? 
                   (inicio + combinaciones_hilo + combinaciones_restantes - 1) : 
                   (inicio + combinaciones_hilo - 1);
        printf("Hilo %d - Combinaciones: %ld - Rango [%ld, %ld]\n", i + 1, fin - inicio + 1, inicio, fin);
    }
    
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);  // Inicia el conteo del tiempo

    // Para crear hilos
    for (int i = 0; i < procesadores_logicos; i++) {
        data[i].target_password = password;
        data[i].mode = modo;
        data[i].length = largo;
        data[i].found = 0;
        pthread_mutex_init(&data[i].lock, NULL);
        pthread_cond_init(&data[i].cond, NULL); 
        data[i].thread_id = i + 1; // ID para cada hilo

        // Asigna el rango de contraseñas a probar por este hilo
        data[i].start_hilo = i * combinaciones_hilo;
        data[i].end_hilo = (i == procesadores_logicos - 1) ? 
                            (data[i].start_hilo + combinaciones_hilo + combinaciones_restantes) : 
                            (data[i].start_hilo + combinaciones_hilo);
        data[i].imprimir_contrasenas = &imprimir_contrasenas; 

        pthread_create(&hilos[i], NULL, brute_force, (void*)&data[i]);
    }

    for (int i = 0; i < procesadores_logicos; i++) {

        pthread_join(hilos[i], NULL);
    }

    // Libera los mutex
    for (int i = 0; i < procesadores_logicos; i++) {

        pthread_mutex_destroy(&data[i].lock);
        pthread_cond_destroy(&data[i].cond);

    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Tiempo total de ejecución: %.6f segundos\n", elapsed_time);


    return 0;

}
