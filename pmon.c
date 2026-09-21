#include "pmon.h"
#include "jobs.h"
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

// banderas volatiles para las senales
static volatile sig_atomic_t sigalrm_flag = 0;
static volatile sig_atomic_t sigint_flag = 0;

// manejador de la alarma para refrescar la pantalla
static void handle_alrm(int sig) {
    (void)sig;
    sigalrm_flag = 1;
}

// manejador de ctrl+c para salir limpiamente al prompt
static void handle_int(int sig) {
    (void)sig;
    sigint_flag = 1;
}

// estructura para guardar tiempos previos y calcular cpu
#define MAX_HISTORIAL 128

typedef struct {
    pid_t pid;
    unsigned long utime;
    unsigned long stime;
    double timestamp;
    int activo;
} HistorialCpu;

static HistorialCpu historial[MAX_HISTORIAL];
static int total_historial = 0;

// funcion auxiliar para obtener el tiempo actual en segundos
static double obtener_tiempo_actual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

// leer /proc/[pid]/stat para sacar el estado y los tiempos de cpu
static int leer_proc_stat(pid_t pid, char *estado_str, size_t estado_len, unsigned long *utime, unsigned long *stime) {
    char ruta[128];
    snprintf(ruta, sizeof(ruta), "/proc/%d/stat", (int)pid);

    FILE *f = fopen(ruta, "r");
    if (!f) {
        return -1; // el proceso termino o no se puede leer
    }

    char linea[1024];
    if (!fgets(linea, sizeof(linea), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    // buscar el ultimo parentesis donde termina el nombre del comando
    char *cierre = strrchr(linea, ')');
    if (!cierre) {
        return -1;
    }

    char estado_char = 'S';
    *utime = 0;
    *stime = 0;

    // saltar el parentesis y leer estado y saltar 10 campos hasta utime y stime
    if (sscanf(cierre + 2, "%c %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu",
               &estado_char, utime, stime) < 3) {
        return -1;
    }

    // convertir la letra de estado al texto que pide el pdf
    if (estado_char == 'R') {
        strncpy(estado_str, "ejecutando", estado_len);
    } else if (estado_char == 'S' || estado_char == 'D') {
        strncpy(estado_str, "durmiendo", estado_len);
    } else if (estado_char == 'Z') {
        strncpy(estado_str, "zombie", estado_len);
    } else if (estado_char == 'T') {
        strncpy(estado_str, "detenido", estado_len);
    } else {
        strncpy(estado_str, "durmiendo", estado_len);
    }
    estado_str[estado_len - 1] = '\0';

    return 0;
}

// leer /proc/[pid]/status para obtener la memoria fisica residente (vmrss)
static long leer_proc_status_rss(pid_t pid) {
    char ruta[128];
    snprintf(ruta, sizeof(ruta), "/proc/%d/status", (int)pid);

    FILE *f = fopen(ruta, "r");
    if (!f) {
        return 0;
    }

    char linea[256];
    long rss_kb = 0;
    while (fgets(linea, sizeof(linea), f)) {
        if (strncmp(linea, "VmRSS:", 6) == 0) {
            sscanf(linea + 6, "%ld", &rss_kb);
            break;
        }
    }
    fclose(f);
    return rss_kb;
}

// calcular porcentaje de cpu usando el delta con la lectura previa
static double calcular_cpu(pid_t pid, unsigned long utime, unsigned long stime, double ahora, long ticks_por_seg) {
    int idx = -1;
    for (int i = 0; i < total_historial; i++) {
        if (historial[i].pid == pid) {
            idx = i;
            break;
        }
    }

    // si es la primera vez que vemos este pid en este pmon
    if (idx == -1) {
        if (total_historial < MAX_HISTORIAL) {
            idx = total_historial++;
        } else {
            idx = 0;
        }
        historial[idx].pid = pid;
        historial[idx].utime = utime;
        historial[idx].stime = stime;
        historial[idx].timestamp = ahora;
        historial[idx].activo = 1;
        return 0.0;
    }

    // calcular delta de tiempo y ticks
    double delta_tiempo = ahora - historial[idx].timestamp;
    if (delta_tiempo <= 0.0001) {
        return 0.0;
    }

    unsigned long delta_ticks = (utime + stime) - (historial[idx].utime + historial[idx].stime);
    double segundos_cpu = (double)delta_ticks / (double)ticks_por_seg;
    double porcentaje = (segundos_cpu / delta_tiempo) * 100.0;

    if (porcentaje < 0.0) porcentaje = 0.0;

    // actualizar registro para el siguiente refresco
    historial[idx].utime = utime;
    historial[idx].stime = stime;
    historial[idx].timestamp = ahora;
    historial[idx].activo = 1;

    return porcentaje;
}

// estructura para ordenar y mostrar las filas en la tabla
typedef struct {
    pid_t pid;
    char cmd[32];
    char estado[16];
    double cpu;
    long rss;
} FilaPmon;

// funcion para ordenar las filas de mayor a menor cpu
static void ordenar_filas_por_cpu(FilaPmon *filas, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (filas[j].cpu < filas[j + 1].cpu) {
                FilaPmon temp = filas[j];
                filas[j] = filas[j + 1];
                filas[j + 1] = temp;
            }
        }
    }
}

// dibujar tabla con todos los procesos activos
static void dibujar_tabla(void) {
    // limpiar la pantalla con codigos ansi
    printf("\033[H\033[J");

    // encabezado tal cual como lo pide el pdf
    printf("%-6s %-16s %-12s %-12s %-10s\n", "PID", "COMANDO", "ESTADO", "%CPU(aprox)", "RSS(KB)");

    Job *curr = jobs_get_head();
    if (!curr) {
        printf("(no hay procesos en segundo plano activos)\n");
        fflush(stdout);
        return;
    }

    long ticks_por_seg = sysconf(_SC_CLK_TCK);
    if (ticks_por_seg <= 0) ticks_por_seg = 100;
    double ahora = obtener_tiempo_actual();

    // arreglo para recolectar datos de procesos activos
    FilaPmon filas[64];
    int total_filas = 0;

    // guardar los procesos que hayan terminado para retirarlos
    pid_t muertos[64];
    int total_muertos = 0;

    while (curr != NULL) {
        char estado_str[32];
        unsigned long utime = 0, stime = 0;

        // leer estado y tiempos desde /proc
        if (leer_proc_stat(curr->pid, estado_str, sizeof(estado_str), &utime, &stime) < 0) {
            if (total_muertos < 64) {
                muertos[total_muertos++] = curr->pid;
            }
            curr = curr->next;
            continue;
        }

        // si el proceso esta en estado zombie ya termino
        if (strcmp(estado_str, "zombie") == 0) {
            if (total_muertos < 64) {
                muertos[total_muertos++] = curr->pid;
            }
            curr = curr->next;
            continue;
        }

        long rss_kb = leer_proc_status_rss(curr->pid);
        double cpu_porcentaje = calcular_cpu(curr->pid, utime, stime, ahora, ticks_por_seg);

        // guardar en el arreglo si hay espacio
        if (total_filas < 64) {
            filas[total_filas].pid = curr->pid;
            strncpy(filas[total_filas].cmd, curr->cmd_line ? curr->cmd_line : "-", 16);
            filas[total_filas].cmd[16] = '\0';
            strncpy(filas[total_filas].estado, estado_str, 15);
            filas[total_filas].estado[15] = '\0';
            filas[total_filas].cpu = cpu_porcentaje;
            filas[total_filas].rss = rss_kb;
            total_filas++;
        }

        curr = curr->next;
    }

    // eliminar de la lista los procesos que ya terminaron o estan zombies
    for (int i = 0; i < total_muertos; i++) {
        // recolectar para no dejar zombies en el sistema
        waitpid(muertos[i], NULL, WNOHANG);
        jobs_remove(muertos[i]);
    }

    if (total_filas == 0) {
        printf("(no hay procesos en segundo plano activos)\n");
        fflush(stdout);
        return;
    }

    // ordenar de mayor a menor cpu para el bonus
    ordenar_filas_por_cpu(filas, total_filas);

    // imprimir las filas ordenadas y resaltar la de mayor consumo
    for (int i = 0; i < total_filas; i++) {
        if (i == 0 && filas[i].cpu > 0.0) {
            // resaltar con color y asterisco el proceso con mayor uso de cpu
            printf("\033[1;32m%-6d %-16s %-12s %-12.1f %-10ld (*)\033[0m\n",
                   (int)filas[i].pid, filas[i].cmd, filas[i].estado, filas[i].cpu, filas[i].rss);
        } else {
            printf("%-6d %-16s %-12s %-12.1f %-10ld\n",
                   (int)filas[i].pid, filas[i].cmd, filas[i].estado, filas[i].cpu, filas[i].rss);
        }
    }

    fflush(stdout);
}

// funcion principal de pmon
int pmon_run(char **args) {
    int intervalo = 2; // valor por defecto si no se pasa parametro

    // leer argumento de segundos
    if (args && args[1]) {
        int seg = atoi(args[1]);
        if (seg > 0) {
            intervalo = seg;
        }
    }

    // reiniciar variables
    sigalrm_flag = 0;
    sigint_flag = 0;
    total_historial = 0;

    // instalar manejador de sigalrm
    struct sigaction sa_alrm, vieja_alrm;
    memset(&sa_alrm, 0, sizeof(sa_alrm));
    sa_alrm.sa_handler = handle_alrm;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0; // sin sa_restart para que pause() despierte
    sigaction(SIGALRM, &sa_alrm, &vieja_alrm);

    // instalar manejador de sigint (ctrl+c)
    struct sigaction sa_int, vieja_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_int;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, &vieja_int);

    // primera llamada para dibujar de inmediato
    dibujar_tabla();
    alarm(intervalo);

    // ciclo principal de refresco
    while (!sigint_flag) {
        pause();

        if (sigint_flag) {
            break;
        }

        if (sigalrm_flag) {
            sigalrm_flag = 0;
            dibujar_tabla();
            alarm(intervalo);
        }
    }

    // cancelar alarma pendiente
    alarm(0);

    // restaurar manejadores previos
    sigaction(SIGALRM, &vieja_alrm, NULL);
    sigaction(SIGINT, &vieja_int, NULL);

    printf("\n");
    return 0;
}
