/* *******************************************
* Integrantes:                               *
* - Efraín García Valencia                   *
* - Nicolás Carmona Cardona                  *
* *******************************************/

#include <stdio.h>      // Para printf y fprintf
#include <stdlib.h>     // Para exit
#include <sys/time.h>   // Para gettimeofday
#include <sys/types.h>  // Para pid_t
#include <sys/wait.h>   // Para waitpid
#include <unistd.h>     // Para fork y execvp

int main(int argc, char *argv[]) {
    /*Verifica si se ha pasado un comando como argumento, si no hay suficientes argumentos, 
    imprime como se debe usar el programa y termina con un código de error.*/
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct timeval start, end;  // Estructuras para almacenar el tiempo inicial y final
    
    gettimeofday(&start, NULL); // Obtener el tiempo inicial antes de crear el proceso hijo

    pid_t pid = fork();  // fork() crea un nuevo proceso (hijo)

    /*Verificar si fork() ha fallado, si falla, se imprime un mensaje de error y 
    se termina el programa con un código de error.*/
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) {
        // El proceso hijo ejecuta el comando que se pasó como argumento usando execvp
        execvp(argv[1], &argv[1]);

        // Si execvp falla, se imprime un error y el proceso hijo termina
        perror("exec failed");
        exit(EXIT_FAILURE);
    } 
    else {
        int status;
        waitpid(pid, &status, 0);   // El padre espera a que el hijo termine
        
        gettimeofday(&end, NULL);   // Obtener el tiempo final después de que el hijo termina

        // Calcular el tiempo transcurrido en segundos y microsegundos
        double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

        // Mostrar el tiempo transcurrido en segundos con 5 decimales
        printf("Elapsed time: %.5f seconds\n", elapsed_time);
    }

    return 0;
}
