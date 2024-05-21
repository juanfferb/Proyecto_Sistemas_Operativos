/*************************************************************
Autor: Nicolas Hincapie Jara - Juan Felipe Fernandez Barrero
Fecha: 21 de Mayo de 2024
Materia: Sistemas Operativos
Tema: Proyecto Sistemas Operativos
Fichero: Manejo de sensor.c
**************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // Almacena las flags de los argumentos de línea de comandos
  int banderas; 
  // Puntero al tipo de sensor
  char *TipoSensor = NULL;   
  // Puntero al intervalo de tiempo
  char *IntervaloTiempo = NULL; 
  // Puntero al nombre del archivo de datos
  char *NombreArchivo = NULL;  
  // Puntero al nombre del pipe nominal
  char *NombrePipe = NULL;     

  // Maneja de banderas
  while ((banderas = getopt(argc, argv, "s:t:f:p:")) != -1) {
    switch (banderas) {
    // Bandera de sensor
    case 's': 
      TipoSensor = argv[optind - 1];
      break;
    // Bandera de sensor del intervalo de tiempo
    case 't': 
      IntervaloTiempo = argv[optind - 1];
      break;
    // Bandera del nombre del archivo
    case 'f': 
      NombreArchivo = argv[optind - 1];
      break;
    // Bandera del nombre del pipe
    case 'p':
      NombrePipe = argv[optind - 1];
      break;
    // Mensaje si se recibe una entrada incorrecta
    default: 
      fprintf(
          stderr,
          "Estructura: %s -s TipoSensor -t IntervaloTiempo -f NombreArchivo -p NombrePipe\n",
          argv[0]);
      return 1;
    }
  }

  // Conversión de cadenas de 'TipoSensorEntero' y 'IntervaloTiempoEntero' a enteros
  int TipoSensorEntero = atoi(TipoSensor);
  int IntervaloTiempoEntero = atoi(IntervaloTiempo);

  // Verificación tipo de sensor
  if (TipoSensorEntero != 1 && TipoSensorEntero != 2) {
    fprintf(stderr,
            "Error: El tipo de sensor es invalido, el sensor debe ser 1 o 2.\n");
    return 1;
  }

  // Apertura del pipe nominal en modo escritura
  int pipeNominal = open(NombrePipe, O_WRONLY);
  if (pipeNominal == -1) {
    perror("Error abriendo el pipe");
    exit(1);
  } else {
    printf("El pipe se abrio correctamente: %s\n", NombrePipe);
  }

  // Apertura del archivo de datos en modo lectura
  FILE *archivoDatos = fopen(NombreArchivo, "r");
  if (!archivoDatos) {
    fprintf(stderr, "Error abriendo el archivo: %s\n", NombreArchivo);
    close(pipeNominal);
    exit(1);
  } else {
    printf("El archivo se abrio correctamente\n");
  }

  // Lectura y envío de datos al pipe nominal
  char linea[1024];
  while (fgets(linea, sizeof(linea), archivoDatos)) {
    float validacion = atof(linea);

    // Valores negativos invalidos
    if (validacion < 0) {
      continue;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%d:%.2f\n", TipoSensorEntero, validacion);

    // Tipo de sensor y el valor leído
    if (TipoSensorEntero == 1) {
      printf("Sensor manda la temperatura: %.2f\n", validacion);
    } else if (TipoSensorEntero == 2) {
      printf("Sensor manda el pH: %.2f\n", validacion);
    }

    // Escritura en el pipe nominal
    ssize_t bytes_escritos = write(pipeNominal, buffer, strlen(buffer));
    if (bytes_escritos == -1) {
      perror("Error al escribir el pipe");
      break;
    }

    // Pausa de acuerdo al intervalo de tiempo 
    sleep(IntervaloTiempoEntero);
  }

  // Cierre del archivo y el pipe
  fclose(archivoDatos);
  close(pipeNominal);

  return 0;
}