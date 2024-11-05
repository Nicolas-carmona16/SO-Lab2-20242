/* *******************************************
* Integrantes:                               *
* - Efraín García Valencia                   *
* - Nicolás Carmona Cardona                  *
* *******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define MAX_PATHS 64

char error_message[30] = "An error has occurred\n";
char *paths[MAX_PATHS];  // Almacena los directorios para buscar ejecutables

// Imprime el mensaje de error
void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

// Inicializa el path con /bin como valor predeterminado
void initialize_paths() {
    paths[0] = strdup("/bin");
    paths[1] = NULL;
}

// Función para manejar el comando "cd"
int cd_command(char **args) {
    if (args[1] == NULL || args[2] != NULL) {  // cd solo debe tener un argumento
        print_error();
    } else {
        if (chdir(args[1]) != 0) {
            print_error();
        }
    }
    return 1;
}

// Función para manejar el comando "exit"
int exit_command(char **args) {
    if (args[1] != NULL) {  // Error si exit tiene argumentos
        print_error();
        return 1;
    }
    exit(0);
}

// Función para manejar el comando "path"
int path_command(char **args) {
    // Libera la memoria de los paths actuales antes de sobrescribir
    for (int i = 0; paths[i] != NULL; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }
    // Almacena los nuevos paths
    for (int i = 1; args[i] != NULL && i < MAX_PATHS; i++) {
        paths[i - 1] = strdup(args[i]);
    }
    return 1;
}

// Verifica si el comando es uno de los built-in (cd, exit, path)
int built_in_commands(char **args) {
    if (args[0] == NULL) return 0;
    
    if (strcmp(args[0], "cd") == 0) {
        return cd_command(args);
    } else if (strcmp(args[0], "exit") == 0) {
        return exit_command(args);
    } else if (strcmp(args[0], "path") == 0) {
        return path_command(args);
    }
    return 0;  // No es un comando integrado
}

// Busca el ejecutable en los directorios del path
char *find_executable(char *command) {
    if (paths[0] == NULL) {
        return NULL;  // No se encontró ningún path para buscar
    }

    for (int i = 0; paths[i] != NULL; i++) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], command);
        if (access(full_path, X_OK) == 0) {
            return strdup(full_path);  // Retorna la ruta del ejecutable
        }
    }
    return NULL;  // No se encontró el ejecutable
}

// Ejecuta un comando externo
void execute_command(char **args, char *output_file) {
    if (args[0] == NULL) {
        print_error();
        return;
    }

    if (paths[0] == NULL) {
        print_error();
        return;
    }

    pid_t pid = fork();
    
    if (pid == 0) {  // Proceso hijo
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                print_error();
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);  // Redirige stdout
            dup2(fd, STDERR_FILENO);  // Redirige stderr
            close(fd);
        }

        // Soporte para rutas absolutas o relativas
        if (args[0][0] == '/' || args[0][0] == '.') {
            if (access(args[0], X_OK) == 0) {
                execv(args[0], args);
                print_error();  // Si execv falla
                exit(1);
            } else {
                print_error();
                exit(1);
            }
        } else {
            // Busca en los directorios del path
            char *executable = find_executable(args[0]);
            if (executable != NULL) {
                execv(executable, args);
                free(executable);
                print_error();  // Si execv falla
                exit(1);
            } else {
                print_error();
                exit(1);
            }
        }
    } else if (pid < 0) {
        print_error();  // Error en fork
    } else {
        wait(NULL);  // Espera a que el proceso hijo termine
    }
}

// Procesa y ejecuta un solo comando
void process_command(char *line) {
    char *args[MAX_ARGS];
    char *output_file = NULL;
    int i = 0;
    int redirection_count = 0;

    // Verifica si la línea empieza con '>'
    if (line[0] == '>') {
        print_error();
        return;
    }

    // Divide la línea en tokens
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        if (strcmp(token, ">") == 0) {
            redirection_count++;
            if (redirection_count > 1) {  // Múltiples redirecciones
                print_error();
                return;
            }
            token = strtok(NULL, " \t\n");
            if (token == NULL || strchr(token, '>')) {  // No hay archivo o múltiples >
                print_error();
                return;
            }
            output_file = token;  // Almacena el archivo de salida
        } else {
            args[i] = token;  // Almacena el argumento
            i++;
        }
        token = strtok(NULL, " \t\n");
    }

    if (token != NULL) {  // Demasiados argumentos
        print_error();
        return;
    }
    
    args[i] = NULL;  // Termina la lista de argumentos

    if (args[0] == NULL) return;  // Comando vacío

    if (built_in_commands(args)) {
        if (output_file != NULL) {  // Redirección con comando built-in
            print_error();
        }
        return;  // Si es un comando integrado, ya fue procesado
    }

    execute_command(args, output_file);  // Ejecuta el comando externo
}

// Procesa múltiples comandos en paralelo
void process_parallel_commands(char *line) {
    char *commands[MAX_ARGS];
    int i = 0;

    // Divide la línea en comandos separados por &
    char *token = strtok(line, "&");
    while (token != NULL && i < MAX_ARGS - 1) {
        commands[i] = token;  // Almacena el comando
        i++;
        token = strtok(NULL, "&");
    }
    
    if (token != NULL) {  // Demasiados comandos paralelos
        print_error();
        return;
    }
    
    commands[i] = NULL;

    pid_t pids[MAX_ARGS];
    int j = 0;

    // Ejecuta cada comando en un proceso separado
    for (j = 0; commands[j] != NULL; j++) {
        if ((pids[j] = fork()) == 0) {
            process_command(commands[j]);  // Procesa el comando
            exit(0);
        } else if (pids[j] < 0) {
            print_error();  // Error en fork
            return;
        }
    }

    // Espera a que todos los procesos paralelos terminen
    for (int k = 0; k < j; k++) {
        waitpid(pids[k], NULL, 0);
    }
}

// Función principal del shell
int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t len = 0;

    // Verifica si es modo batch o interactivo
    FILE *input_stream = stdin;

    if (argc > 2) {
        print_error();
        exit(1);
    }

    if (argc == 2) {
        input_stream = fopen(argv[1], "r");
        if (!input_stream) {
            print_error();
            exit(1);
        }
    }

    initialize_paths();  // Inicializa las rutas de búsqueda

    while (1) {
        if (input_stream == stdin) {
            printf("wish> ");
            fflush(stdout);  // Asegura que el prompt se muestre
        }

        if (getline(&line, &len, input_stream) == -1) {
            if (input_stream != stdin) {
                fclose(input_stream);  // Cierra el archivo batch
            }
            free(line);  // Libera la memoria antes de salir
            exit(0);  // EOF encontrado
        }

        // Si hay & en la línea, es un comando paralelo
        if (strchr(line, '&')) {
            process_parallel_commands(line);
        } else {
            process_command(line);  // Procesa un comando normal
        }
    }

    return 0;
}
