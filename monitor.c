/*************************************************************
Autor: Nicolas Hincapie Jara - Juan Felipe Fernandez Barrero
Fecha: 20 de Mayo de 2024
Materia: Sistemas Operativos
Tema: Proyecto
Fichero: Manejo de sensor.c
**************************************************************/

//Librerias para el correcto funcionamiento del codigo
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
//Bandera de temperatura
#define FLAG_TEMP "TEMP" 

// Bandera de pH
#define FLAG_PH "PH"

// Arreglos de punteros de temperatura y pH
char **bTemp, **bufferph;


// Tamaño del buffer
int tamBuffer;            
// Semáforos para manipular el acceso al buffer
sem_t vac_temp, full_temp, vac_ph, full_ph;
// Mutex al acceder al buffer
pthread_mutex_t temp_mutex, ph_mutex;           
// Entrada y salida para el buffer de temperatura
int in_temp = 0, out_temp = 0; 
// Entrada y salida para el buffer de pH
int ent_ph = 0, ex_ph = 0;     

// Archivos correspondientes a los datos de temperatura y pH
char *arcv_temp = NULL, *arcv_ph = NULL;

// Inicialización de los buffers que almacenarán datos de temperatura y pH.
void inicializar_buffers() {
    // Se asigna memoria para el arreglo de punteros que representan los buffers.
      bTemp = (char **)malloc(tamBuffer * sizeof(char *));
    bufferph = (char **)malloc(tamBuffer * sizeof(char *));

    // Verifica reserva de memoria
    if (!bTemp || !bufferph) {
        // Mensaje por si ocurrio un error en la reserva y se acaba del programa.
        perror("Error al asignar memoria para buffers");
        exit(1);
    }

    // Asignacion de memoria buffer por buffer.
    for (int i = 0; i < tamBuffer; i++) {
        // Se asigna memoria para un buffer individual de tamaño 128.
          bTemp[i] = (char *)malloc(128 * sizeof(char));
        bufferph[i] = (char *)malloc(128 * sizeof(char));

        // Se verifica si la asignación de memoria fue exitosa.
        if (!bTemp[i] || !bufferph[i]) {
            // Si falla la asignación, se imprime un mensaje de error y se sale del programa.
            perror("Error al asignar memoria para buffers");
            exit(1);
        }
    }
}

// Liberar memoria asignada para los buffers
void liberar_memoria() {
    // Se libera la memoria individualmente.
    for (int i = 0; i < tamBuffer; i++) {
        free(bTemp[i]);
        free(bufferph[i]);
    }
    // Se libera la memoria asignada para los arreglos que representan los buffers.
    free(bTemp);
    free(bufferph);
}

//Hilo que recolecta datos de los sensores
void *almacen(void *param) {
    // Se obtiene el nombre del pipe como parámetro de entrada.
    char *nom_pipe = (char *)param;

    // Se abre el pipe en modo lectura.
    int opr = open(nom_pipe, O_RDONLY);  
    if (opr == -1) {
        // Impresión de un mensaje de error y finaliza el programa.
        perror("Error al abrir el pipe en el collector");
        exit(1);
    }

    //Buffer para leer los datos.
    char lectura_buffer[1024];

    // Se inicia un bucle para leer los datos del pipe.
    while (read(opr, lectura_buffer, sizeof(lectura_buffer)) > 0) {
        // Se verifica si el último caracter del buffer es un salto de línea
        if (lectura_buffer[strlen(lectura_buffer) - 1] == '\n') {
              lectura_buffer[strlen(lectura_buffer) - 1] = '\0';
        }

        // Se divide el contenido del buffer teniendo en cuenta a los dos puntos ":" como el token que indica la finalización o inicio de otro.
        char *tok = strtok(lectura_buffer, ":");
        int tipoS = atoi(tok);  
          tok = strtok(NULL, ":");
        char *data = tok;

        // Se verifica el tipo de sensor.
        if (tipoS == 1 || tipoS == 2) { 
            // Seleccionar los semáforos, mutex y buffers según el tipo de sensor.
            sem_t *empt, *full; 
            pthread_mutex_t *mut;  
            char **buffer;  
            int *in;  

            if (tipoS == 1) {
                empt = &vac_temp;
                full = &full_temp;
                mut = &temp_mutex;
                buffer = bTemp;
                in = &in_temp;
            } else {
                empt = &vac_ph;
                full = &full_ph;
                mut = &ph_mutex;
                buffer = bufferph;
                in = &ent_ph;
            }

            // Maneja disponibilidad en el buffer.
            sem_wait(empt);

            // Bloquear el mutex para accedeso al buffer.
            pthread_mutex_lock(mut);  

            // Almacenar los datos en el buffer.
            sprintf(buffer[*in], "%s:%s", (tipoS == 1) ? FLAG_TEMP : FLAG_PH, data);

            // Actualizar el índice de entrada del buffer.
            *in = (*in + 1) % tamBuffer;  

            // Desbloquear el mutex para permitir el acceso a hilos.
            pthread_mutex_unlock(mut); 

            // Aumentar el contador de elementos en el buffer.
            sem_post(full);  
        } else {
            // Si se recibe una lectura incorrecta, se imprime un mensaje de error.
          printf("Error: Medición incorrecta recibida\n");
        }
    }

    // Liberar los semáforos de espacio vacío.
    sleep(10);
    sem_post(&vac_temp);
    sem_post(&vac_ph);

    // Cerrar el pipe.
    close(opr);  

    // Terminar la ejecución del hilo.
    return NULL;
}

// Función de hilo para recolectar datos de temperatura
void *temperatura_hilo() {
    // Se abre el archivo de temperatura en modo de añadir contenido al final del archivo.
    FILE *arcvT = fopen(arcv_temp, "a");
    if (arcvT == NULL) {
        // Si ocurre un error al abrir el archivo, se imprime un mensaje de error y se sale del programa.
        perror("Error abriendo el archivo de temperatura");
        exit(1);
    }

    // Bucle infinito para procesar continuamente los datos de temperatura.
    while (1) {
        // Esperar a que haya datos disponibles en el buffer de temperatura.
        sem_wait(&full_temp);

        // Bloquear el mutex para acceder al buffer de temperatura de manera segura.
        pthread_mutex_lock(&temp_mutex);

        // Verificar si el dato en el buffer de temperatura es una lectura válida.
        if (strncmp(bTemp[out_temp], FLAG_TEMP, strlen(FLAG_TEMP)) == 0) {
            // Extraer el valor de temperatura del dato en el buffer.
            char *data = bTemp[out_temp] + strlen(FLAG_TEMP) + 1;

            // Convertir el dato de temperatura a un número flotante.
            float temperatura = atof(data);

            // Obtener la marca de tiempo actual.
            time_t act = time(NULL);
            struct tm *time_info = localtime(&act);
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);

            // Verificar si la temperatura está dentro del rango aceptable.
            if (temperatura < 20.0 || temperatura > 31.6) {
              printf("Alerta: La temperatura esta fuera del rango %.1f\n",temperatura);
            } else {
              // Escribir el tiempo y el valor de temperatura en el archivo de temperatura. 
              fprintf(arcvT, "{%s} %f\n", timestamp, temperatura);
              fflush(arcvT);
            }
        }

        out_temp = (out_temp + 1) % tamBuffer;

        // Desbloquear el mutex para permitir el acceso a otros hilos.
        pthread_mutex_unlock(&temp_mutex);

        // Aumentar el contador de espacios vacíos en el buffer de temperatura.
        sem_post(&vac_temp);
    }

    // Cerrar el archivo de temperatura.
    fclose(arcvT);

    // Terminar la ejecución del hilo.
    return NULL;
}

// Función de hilo para recolectar datos de pH
void *ph_thread_collec() {
    // Se abre el archivo de pH de modo que añada la información al final del archivo.
    FILE *fileData = fopen(arcv_ph, "a");
    if (fileData == NULL) {
        // Impresion de mensaje de error y se sale del programa.
        perror("Error abriendo el archivo de pH");
        exit(1);
    }

    // Ciclo que procesa continuamente los datos de pH.
    while (1) {
        // Esperar a tener datos en el buffer de pH.
        sem_wait(&full_ph);

        // Bloquear el mutex para acceder al buffer de pH.
        pthread_mutex_lock(&ph_mutex);

        // Verificar si el dato en el buffer de pH es valido.
        if (strncmp(bufferph[ex_ph], FLAG_PH, strlen(FLAG_PH)) == 0) {
            // Extraer y poner el valor de pH del dato en el buffer.
            char *data = bufferph[ex_ph] + strlen(FLAG_PH) + 1; 
            float ph = atof(data);  

            // Obtener el tiempo actual.
            time_t now = time(NULL);  
            struct tm *tm_info = localtime(&now);  
            char actTime[20];
            strftime(actTime, sizeof(actTime), "%Y-%m-%d %H:%M:%S", tm_info);  

            // Verificar si el pH está dentro del rango establecido.
            if (ph < 6 || ph > 8) {
              printf("Alerta, el pH esta fuera del rango %.1f\n", ph);
            } else {
                // Escribir la marca de tiempo y el valor de pH en el archivo de pH.
                fprintf(fileData, "{%s} %f\n", actTime, ph);
                fflush(fileData);  
            }
        }

          ex_ph = (ex_ph + 1) % tamBuffer;  

        // Desbloquear el mutex para permitir el acceso a otros hilos.
        pthread_mutex_unlock(&ph_mutex);

        //Manejo de espacio vacios
        sem_post(&vac_ph);  
    }

    // Cerrar el archivo de pH.
    fclose(fileData); 

    // Terminar la ejecución del hilo.
    return NULL;
}
int main(int argc, char *argv[]) {
  int bandera; // Almacena las flags de los argumentos de línea de comandos
  char *pipenom = NULL;   // Puntero al nombre del pipe nominal

  while ((bandera = getopt(argc, argv, "b:t:h:p:")) != -1) {
    switch (bandera) {
      // Bandera del tamaño del Buffer 
      case 'b': 
          tamBuffer = atoi(optarg);
        break;
      // Bandera del archivo de temperatura
      case 't': 
          arcv_temp = optarg;
        break;
      // Bandera del archivo de pH
      case 'h':  
          arcv_ph = optarg;
        break;
      // Bandera del nombre del Pipe
      case 'p': 
          pipenom = optarg;
        break;
       // Mensaje de uso en caso de argumentos incorrectos
      default: 
        fprintf(stderr,
                "Estructura: %s -b <TamañoBuffer> -t <file-temp> -h <file-ph> -p "
                "<NombrePipe>\n",
                argv[0]);
        return 1;
    }
  }

  // Verificar la validez de los nombres de archivo
  if (!arcv_temp || !arcv_ph) {
    fprintf(stderr, "Error abriendo el archivo.\n");
    exit(1);
  }

  // Crear el pipe si no existe
  if (access(pipenom, F_OK) == -1) {
    printf("Se creo el pipe: '%s'\n", pipenom);

    if (mkfifo(pipenom, 0666) == -1) {
      perror("Error creando el pipe");
      exit(1);
    }
  } else {
    printf("El pipe ya existe y no debe ser creado\n");
  }

  // Inicializar semáforos para manejo de acceso
  printf("Se estan inicializando los semaforos\n");
  // Inicializar semáforo de espacios vacíos en el buffer de temperatura.
  sem_init(&vac_temp, 0, tamBuffer);
  // Inicializar semáforo de espacios llenos en el buffer de temperatura vacíos.
  sem_init(&full_temp, 0, 0);
  // Inicializar semáforo de espacios vacíos en el buffer de pH.
  sem_init(&vac_ph, 0, tamBuffer);
  // Inicializar semáforo de espacios llenos en el buffer de pH vacíos.
  sem_init(&full_ph, 0, 0);
  // Inicializar mutex para el buffer de temperatura.
  pthread_mutex_init(&temp_mutex, NULL);
  // Inicializar mutex para el buffer de pH.
  pthread_mutex_init(&ph_mutex, NULL);


  // Se llama la función para inicializar los buffers
  inicializar_buffers();
  printf("Se estan inicializando los buffer: 2\n");
  printf("──────────────────────────────────────────\n");

  // Se delcaran hilos de pH y temperatura para recolectar datos
  pthread_t recolector_thread, ph_thread, temperatura_thread;

  // Se crean los hilos para ejecutar las funciones correspondientes
  pthread_create(&recolector_thread, NULL, almacen, pipenom); // Hilo para recolectar datos del pipe
  pthread_create(&ph_thread, NULL, ph_thread_collec, NULL); // Hilo para recolectar datos de pH
  pthread_create(&temperatura_thread, NULL, temperatura_hilo, NULL); // Hilo para recolectar datos de temperatura

  // Se espera a que los hilos terminen su ejecución para poder continuar
  pthread_join(recolector_thread, NULL); // Esperar a que termine el hilo de recolección
  pthread_join(ph_thread, NULL); // Esperar a que termine el hilo de recolección de datos de pH
  pthread_join(temperatura_thread, NULL); // Esperar a que termine el hilo de recolección de datos de temperatura

  // Se destruyen los semáforos y mutex para liberar los recursos
  sem_destroy(&vac_temp);
  sem_destroy(&full_temp); 
  sem_destroy(&vac_ph); 
  sem_destroy(&full_ph);
  pthread_mutex_destroy(&temp_mutex); 
  pthread_mutex_destroy(&ph_mutex); 
  // Se llama a la función de liberar memoria asignada a los buffers
    liberar_memoria();

  return 0;
}