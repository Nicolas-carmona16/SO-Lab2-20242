/* *******************************************
* Integrantes:                               *
* - Efraín García Valencia                   *
* - Nicolás Carmona Cardona                  *
* *******************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

#define DELIM " \t\r\n\a"  // Separadores de tokens en la entrada

// Declaración de funciones
int execute(char* args[], char* retArgs[]);
int readCommand(char* args[], FILE* fp);
int command_cd(char* args[], int numArgs);
int command_path(char* args[], int numArgs);
int lineSeperate(char* line, char* args[], char* delim);
int redirect(char* ret, char* line);
int parallel(char* ret, char* line);
void error_handler();

// Variable global para almacenar las rutas de búsqueda de comandos
char* PATH[20];

// Maneja errores imprimiendo un mensaje en STDERR
void error_handler() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

// Implementación del comando 'cd', cambia el directorio de trabajo
int command_cd(char* args[], int numArgs) {
    if (numArgs != 2) {  // El comando 'cd' debe tener un argumento (el directorio)
        error_handler();
        return 1;
    }
    if (chdir(args[1]) == -1) {  // Cambiar directorio
        error_handler();
        return 1;
    }
    return 0;
}

// Implementación del comando 'path', establece nuevas rutas para buscar comandos
int command_path(char* args[], int numArgs) {
    int i = 0;
    // Copiar las rutas especificadas en 'args' a la variable global PATH
    while (1) {
        if ((PATH[i] = args[i + 1]) == NULL)
            break;
        i++;
    }
    return 0;
}

// Separa una línea de texto en argumentos usando delimitadores
int lineSeperate(char* line, char* args[], char* delim) {
    char *save;
    int argsIndex = 0;
    args[argsIndex] = strtok_r(line, delim, &save);
    argsIndex++;
    // Continuar separando los argumentos
    while (1) {
        args[argsIndex] = strtok_r(NULL, delim, &save);
        if (args[argsIndex] == NULL)
            break;
        argsIndex++;
    }
    if (args[0] == NULL)
        return 0;
    return argsIndex;
}

// Maneja la redirección de salida '>'
int redirect(char* ret, char* line) {
    char* progArgs[100];
    char* retArgs[100];
    ret[0] = '\0';  // Limpiar el buffer de redirección
    ret = ret + 1;  // Saltar el '>' en el string
    int argsNum = lineSeperate(line, progArgs, DELIM);  // Separar el comando y sus argumentos
    if (argsNum == 0) {
        error_handler();
        return 1;
    }
    int retArgc = lineSeperate(ret, retArgs, DELIM);  // Separar el archivo de salida
    if (retArgc != 1) {  // Solo se permite un archivo de salida
        error_handler();
        return 1;
    }
    execute(progArgs, retArgs);  // Ejecutar el comando con redirección
    return 0;
}

// Maneja la ejecución de comandos en paralelo usando '&'
int parallel(char* ret, char* line) {
    char** commands = malloc(100 * sizeof(char*));
    int numCommands = lineSeperate(line, commands, "&");  // Separar los comandos por '&'
    char** args = malloc(50 * sizeof(char*));
    char* retRedir = malloc(100 * sizeof(char));
    for (int i = 0; i < numCommands; i++) {
        // Si se detecta una redirección, manejarla
        if ((retRedir = strchr(commands[i], '>'))) {
            redirect(retRedir, commands[i]);
            continue;
        }
        // Ejecutar cada comando en paralelo
        lineSeperate(commands[i], args, DELIM);
        execute(args, NULL);
    }
    free(args);
    free(commands);
    return 0;
}

// Lee una línea de comandos desde la entrada estándar o desde un archivo
int readCommand(char* args[], FILE* fp) {
    char* line = malloc(100 * sizeof(char));
    size_t len = 0;
    ssize_t nread;
    char* retRedir = NULL;
    char* retParal = NULL;

    fflush(stdin);
    if ((nread = getline(&line, &len, fp)) == -1) {  // Leer la línea de entrada
        return 1;
    }
    if ((strcmp(line, "\n") == 0) || (strcmp(line, "") == 0))  // Línea vacía
        return -1;

    if (line[strlen(line) - 1] == '\n')  // Eliminar salto de línea al final
        line[strlen(line) - 1] = '\0';

    if (line[0] == EOF) {  // Fin de archivo
        return 1;
    }

    // Verificar si hay comandos paralelos '&' o redirección '>'
    if ((retParal = strchr(line, '&'))) {
        parallel(retParal, line);
        return -1;
    }

    if ((retRedir = strchr(line, '>'))) {
        redirect(retRedir, line);
        return -1;
    }

    // Separar la línea en argumentos
    int argsIndex = lineSeperate(line, args, DELIM);
    if (argsIndex == 0) {
        return 0;
    }

    // Comandos internos 'exit', 'cd', 'path'
    if (strcmp(args[0], "exit") == 0) {
        if (args[1])  // No debe tener argumentos
            error_handler();
        exit(0);
    }
    if (strcmp(args[0], "cd") == 0) {
        command_cd(args, argsIndex);
        return -1;
    }
    if (strcmp(args[0], "path") == 0) {
        command_path(args, argsIndex);
        return -1;
    }
    return 0;
}

// Ejecuta un comando con sus argumentos, buscando el ejecutable en las rutas definidas
int execute(char* args[], char* retArgs[]) {
    int pid;
    int status;
    char* commandPath = malloc(100 * sizeof(char*));
    int i = 0;  // Índice para iterar sobre las rutas de PATH
    int isNotFound = 1;

    if (PATH[0] == NULL) {  // Si no se ha definido ninguna ruta
        error_handler();
        return 1;
    }
    if (args == NULL || args[0] == NULL)  // Argumentos nulos
        return 1;

    // Buscar el ejecutable en las rutas definidas
    while (1) {
        if (PATH[i] == NULL)
            break;

        char* tempStr = malloc(100 * sizeof(char));
        if (!strcpy(tempStr, PATH[i])) {  // Copiar la ruta
            error_handler();
            return 1;
        }
        int strLen = strlen(tempStr);
        tempStr[strLen] = '/';
        tempStr[strLen + 1] = '\0';
        strcat(tempStr, args[0]);

        if (access(tempStr, X_OK) == 0) {  // Verificar si el archivo es ejecutable
            strcpy(commandPath, tempStr);
            isNotFound = 0;
            free(tempStr);
            break;
        }
        free(tempStr);
        i++;
    }
    
    if (isNotFound) {  // No se encontró el comando
        error_handler();
        return 1;
    }

    // Crear un proceso hijo para ejecutar el comando
    pid = fork();
    switch (pid) {
        case -1:
            error_handler();
            return 1;
        case 0:  // Proceso hijo
            if (retArgs) {
                int fd_out = open(retArgs[0], O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
                if (fd_out > 0) {  // Redirigir salida estándar al archivo
                    dup2(fd_out, STDOUT_FILENO);
                    fflush(stdout);
                }
            }
            execv(commandPath, args);  // Ejecutar el comando
            exit(1);  // Si execv falla, salir
        default:
            waitpid(pid, &status, 0);  // Esperar que termine el hijo
    }
    free(commandPath);
    return 0;
}

// Función principal
int main(int argc, char* argv[]) {
    int isBatchMode = 0;
    PATH[0] = "/bin";  // Definir el directorio de búsqueda de comandos por defecto

    FILE* fp;
    char** args;
    if (argc == 2) {  // Modo batch (ejecución desde archivo)
        if (!(fp = fopen(argv[1], "r"))) {  // Abrir el archivo de comandos
            error_handler();
            exit(1);
        }
        isBatchMode = 1;
    } else if (argc < 1 || argc > 2) {  // Número incorrecto de argumentos
        error_handler();
        exit(1);
    }

    int isFinish = 0;
    while (1) {
        if (isBatchMode == 1) {  // Modo batch
            while (1) {
                args = malloc(100 * sizeof(char));
                int readStatus = readCommand(args, fp);  // Leer el comando del archivo
                fflush(fp);
                if (readStatus == -1)
                    continue;  // Si es un comando interno, continuar
                if (readStatus == 1) {
                    isFinish = 1;
                    break;
                }
                int errNum = execute(args, NULL);  // Ejecutar el comando
                free(args);
                if (errNum == 1)
                    continue;
            }
            break;
        } else {  // Modo interactivo
            fprintf(stdout, "wish> ");  // Imprimir el prompt
            fflush(stdout);
            args = malloc(100 * sizeof(char));
            int readStatus = readCommand(args, stdin);  // Leer comando de la entrada estándar
            fflush(stdin);
            if (readStatus == -1)
                continue;   // Si es un comando interno, continuar
            if (readStatus == 1)
                break;
            if (execute(args, NULL) == 1)
                continue;   // Si hay un error, continuar
            free(args);
        }
        if (isFinish)
            break;
    }
    return 0;
}
