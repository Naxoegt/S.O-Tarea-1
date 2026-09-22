# miShell

Shell simple desarrollada en **C (POSIX/Linux)** para la asignatura **Sistemas Operativos 2026**.

## Integrantes

* Matías Tirado
* Ignacio Carvajal
* Hugo Flores
* Valentina Serón

## Funcionalidades

* Ejecución de comandos con `fork()`, `execvp()` y `waitpid()`.
* Built-ins: `cd`, `exit`, `jobs` y `pmon`.
* Redirecciones: `<`, `>` y `>>`.
* Pipes de largo arbitrario con `|`.
* Procesos en segundo plano con `&`.
* Manejo de `SIGINT`, `SIGQUIT`, `SIGCHLD` y `SIGALRM`.
* Monitor `pmon` utilizando información de `/proc`.
* Bonus: ordenamiento de procesos por uso de CPU y resaltado del proceso con mayor consumo.

## Compilación

```bash
make
```

Para limpiar los archivos generados:

```bash
make clean
```

## Ejecución

```bash
./mishell
```

Ejemplo:

```bash
miShell:$ ls -l | grep ".c" | wc -l
miShell:$ sort < entrada.txt > salida.txt
miShell:$ sleep 30 &
miShell:$ jobs
miShell:$ pmon 1
```

## Estructura

```text
main.c
parser.c / parser.h
executor.c / executor.h
jobs.c / jobs.h
pmon.c / pmon.h
signals.c / signals.h
defs.h
Makefile
```

## Requisitos

* Linux o WSL
* GCC con soporte `gnu11`
* Make
