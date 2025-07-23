
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// ANSI color codes for beautification
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_BOLD    "\033[1m"

// ------------- CONSTANTS & ENUMS -------------
#define TOTAL_BEDS 5
#define ICU_BEDS 5
#define GENERAL_BEDS 10
#define MAX_PATIENTS 100

typedef enum { REGULAR, EMERGENCY, GENERAL, ICU } PatientType;

// ------------- PATIENT STRUCT -------------
typedef struct {
    int id;
    char name[64];
    int age; // Some code uses age
    PatientType type;
    int severity; // For ICU/general distinction
    time_t check_in_time;
    int isICU; // For code that uses ICU flag
} Patient;

// ------------- PRIORITY QUEUE -------------
typedef struct {
    Patient* patients[MAX_PATIENTS];
    int size;
    pthread_mutex_t lock;
} PriorityQueue;

void pq_init(PriorityQueue* pq) {
    pq->size = 0;
    pthread_mutex_init(&pq->lock, NULL);
}

// Higher priority for EMERGENCY, FIFO for same priority
static int patient_cmp(const Patient* a, const Patient* b) {
    if (a->type != b->type)
        return (b->type - a->type); // EMERGENCY > REGULAR
    return (a->check_in_time > b->check_in_time) - (a->check_in_time < b->check_in_time);
}

void pq_push(PriorityQueue* pq, Patient* patient) {
    pthread_mutex_lock(&pq->lock);
    if (pq->size >= MAX_PATIENTS) {
        pthread_mutex_unlock(&pq->lock);
        return;
    }
    int i = pq->size++;
    pq->patients[i] = patient;
    // Bubble up to maintain priority
    while (i > 0 && patient_cmp(pq->patients[i], pq->patients[i-1]) < 0) {
        Patient* tmp = pq->patients[i];
        pq->patients[i] = pq->patients[i-1];
        pq->patients[i-1] = tmp;
        i--;
    }
    pthread_mutex_unlock(&pq->lock);
}

Patient* pq_pop(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->lock);
    if (pq->size == 0) {
        pthread_mutex_unlock(&pq->lock);
        return NULL;
    }
    Patient* top = pq->patients[0];
    for (int i = 1; i < pq->size; ++i) {
        pq->patients[i-1] = pq->patients[i];
    }
    pq->size--;
    pthread_mutex_unlock(&pq->lock);
    return top;
}

int pq_is_empty(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->lock);
    int empty = (pq->size == 0);
    pthread_mutex_unlock(&pq->lock);
    return empty;
}

// ------------- LOGGER -------------
static FILE* log_file = NULL;
static pthread_mutex_t log_lock;

void logger_init(const char* filename) {
    pthread_mutex_init(&log_lock, NULL);
    log_file = fopen(filename, "a");
}

void logger_log_event(const char* event, Patient* patient) {
    pthread_mutex_lock(&log_lock);
    if (log_file && patient) {
        fprintf(log_file, "%s: PatientID=%d, Name=%s, Type=%d, Time=%ld\n", event, patient->id, patient->name, patient->type, patient->check_in_time);
        fflush(log_file);
    } else if (log_file) {
        fprintf(log_file, "%s: (no patient)\n", event);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_lock);
}

void logger_log_bed_status(int total_beds, int occupied_beds) {
    pthread_mutex_lock(&log_lock);
    if (log_file) {
        fprintf(log_file, "Bed Status: %d/%d beds occupied\n", occupied_beds, total_beds);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_lock);
}

void logger_close() {
    pthread_mutex_lock(&log_lock);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_lock);
}

// ------------- GLOBALS -------------
static int occupied_beds = 0;
static pthread_mutex_t bed_lock;
PriorityQueue pq;

// Semaphores for ICU/General beds
sem_t icuBeds, generalBeds;

// For graceful shutdown
volatile sig_atomic_t running = 1;

// Utility: Print real-time status
void print_status() {
    pthread_mutex_lock(&bed_lock);
    printf(COLOR_BOLD COLOR_CYAN "\n[STATUS] Beds Occupied: %d/%d\n" COLOR_RESET, occupied_beds, TOTAL_BEDS);
    printf(COLOR_BOLD COLOR_YELLOW "Patients in Queue: %d\n" COLOR_RESET, pq.size);
    pthread_mutex_unlock(&bed_lock);
}

// Signal handler for graceful shutdown
void handle_sigint(int sig) {
    running = 0;
    printf(COLOR_BOLD COLOR_MAGENTA "\n[INFO] Shutting down hospital system...\n" COLOR_RESET);
}

// Thread to periodically print status
void* status_monitor(void* arg) {
    while (running) {
        print_status();
        sleep(4);
    }
    return NULL;
}

// ------------- THREAD ROUTINES -------------
void* admit_patients(void* arg) {
    while (1) {
        pthread_mutex_lock(&bed_lock);
        if (!pq_is_empty(&pq) && occupied_beds < TOTAL_BEDS) {
            Patient* p = pq_pop(&pq);
            occupied_beds++;
            logger_log_event("Admitted", p);
            logger_log_bed_status(TOTAL_BEDS, occupied_beds);
            printf("Admitted: %s (%s)\n", p->name, (p->type==EMERGENCY)?"EMERGENCY":"REGULAR");
            free(p);
        }
        pthread_mutex_unlock(&bed_lock);
        sleep(1); // Simulate time between admissions
    }
    return NULL;
}

void* discharge_patients(void* arg) {
    while (running) {
        pthread_mutex_lock(&bed_lock);
        if (occupied_beds > 0) {
            occupied_beds--;
            logger_log_event("Discharged", NULL);
            logger_log_bed_status(TOTAL_BEDS, occupied_beds);
            printf(COLOR_YELLOW "[DISCHARGE] Discharged a patient.\n" COLOR_RESET);
        }
        pthread_mutex_unlock(&bed_lock);
        sleep(5); // Simulate time between discharges
    }
    return NULL;
}

void* allocate_bed(void* arg) {
    Patient* p = (Patient*)arg;
    if (p->type == ICU) {
        printf(COLOR_CYAN "[ICU REQUEST] Patient %d requires ICU\n" COLOR_RESET, p->id);
        sem_wait(&icuBeds);
        printf(COLOR_BOLD COLOR_MAGENTA "[ICU ALLOCATED] Patient %d (Severity: %d)\n" COLOR_RESET, p->id, p->severity);
        sleep(1);
        sem_post(&icuBeds);
    } else {
        printf(COLOR_CYAN "[WARD REQUEST] Patient %d requires General Ward\n" COLOR_RESET, p->id);
        sem_wait(&generalBeds);
        printf(COLOR_BOLD COLOR_MAGENTA "[WARD ALLOCATED] Patient %d (Severity: %d)\n" COLOR_RESET, p->id, p->severity);
        sleep(1);
        sem_post(&generalBeds);
    }
    free(p);
    return NULL;
}

// ------------- PATIENT ARRIVAL SIMULATION -------------
void add_patient(const char* name, PatientType type, int severity, int isICU) {
    Patient* p = malloc(sizeof(Patient));
    static int id_gen = 1;
    p->id = id_gen++;
    strncpy(p->name, name, sizeof(p->name)-1);
    p->name[sizeof(p->name)-1] = '\0';
    p->type = type;
    p->check_in_time = time(NULL);
    p->severity = severity;
    p->isICU = isICU;
    pq_push(&pq, p);
    logger_log_event("Check-In", p);
}

// ------------- MAIN -------------
int main() {
    signal(SIGINT, handle_sigint);
    pthread_mutex_init(&bed_lock, NULL);
    pq_init(&pq);
    logger_init("hospital.log");

    // ICU/General beds
    sem_init(&icuBeds, 0, ICU_BEDS);
    sem_init(&generalBeds, 0, GENERAL_BEDS);

    pthread_t admit_thread, discharge_thread, status_thread;
    pthread_create(&admit_thread, NULL, admit_patients, NULL);
    pthread_create(&discharge_thread, NULL, discharge_patients, NULL);
    pthread_create(&status_thread, NULL, status_monitor, NULL);

    // Initial patients
    add_patient("Alice", REGULAR, 5, 0);
    sleep(1);
    add_patient("Bob", EMERGENCY, 9, 1);
    sleep(1);
    add_patient("Charlie", REGULAR, 3, 0);
    sleep(1);
    add_patient("Diana", EMERGENCY, 10, 1);
    sleep(1);
    add_patient("Eve", REGULAR, 2, 0);
    sleep(1);
    add_patient("Frank", REGULAR, 4, 0);

    // Simulate ICU/General bed allocation
    pthread_t threads[10];
    for (int i = 0; i < 10; i++) {
        Patient* p = malloc(sizeof(Patient));
        p->id = 100 + i;
        snprintf(p->name, sizeof(p->name), "WardPatient_%d", p->id);
        p->severity = rand() % 10 + 1;
        p->type = (p->severity > 6) ? ICU : GENERAL;
        pthread_create(&threads[i], NULL, allocate_bed, (void*)p);
        usleep(100000); // 0.1 sec
    }
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    // --- Interactive User Input Loop ---
    char cmd[16];
    while (running) {
        printf(COLOR_BOLD COLOR_CYAN "\nType 'add' to admit patient, 'emergency' for emergency, 'status' for status, or 'exit' to quit:\n> " COLOR_RESET);
        fflush(stdout);
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (strncmp(cmd, "add", 3) == 0) {
            char name[64]; int sev, icu;
            printf("Enter patient name: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            printf("Enter severity (1-10): ");
            scanf("%d", &sev); while(getchar()!='\n');
            printf("ICU? (1 for yes, 0 for no): ");
            scanf("%d", &icu); while(getchar()!='\n');
            add_patient(name, icu ? ICU : REGULAR, sev, icu);
        } else if (strncmp(cmd, "emergency", 9) == 0) {
            char name[64];
            printf("Enter emergency patient name: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            add_patient(name, EMERGENCY, 10, 1);
            printf(COLOR_RED "[EMERGENCY] Emergency patient added!\n" COLOR_RESET);
        } else if (strncmp(cmd, "status", 6) == 0) {
            print_status();
        } else if (strncmp(cmd, "exit", 4) == 0) {
            running = 0;
            break;
        }
    }

    // Cleanup
    running = 0;
    pthread_join(admit_thread, NULL);
    pthread_join(discharge_thread, NULL);
    pthread_join(status_thread, NULL);
    logger_close();
    printf(COLOR_BOLD COLOR_GREEN "System shutdown complete.\n" COLOR_RESET);
    return 0;
}
