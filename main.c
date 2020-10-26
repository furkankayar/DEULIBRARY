/*
    main.c
    by Furkan Kayar
*/

#define _GNU_SOURCE // Defined to avoid implicit declaration warning of 'asprintf' function
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TRUE                    1
#define FALSE                   0
#define STUDENT_NUMBER          100         // Total student number
#define STUDENT_NUMBER_PERIOD   5           // Total student number that comes in a period
#define STUDENT_INCOMING_PERIOD 1500000     // Students comes in randomly periods max 1500 miliseconds
#define STUDENT_WORKING_TIME    6           // After a room is full, students will work then empty room
#define ROOM_CLEANING_TIME      1           // After each room is empty, it should be cleaned for a second to accept new students
#define ROOM_CAPACITY           4           // Maximum student number in a room
#define ROOM_NUMBER             10          // Room number in a library
#define MAX_MESSAGE_NUMBER      10000       // Capacity of message array
#define EMPTY                   0           // Indicates empty state of room
#define ANNOUNCING              1           // Indicates announcing state of room keeper. Actually there is no physical room keeper in room it is a state of room
#define CLEANING                2           // Indicates cleaning state of room. Room keeper can not be free so it cleans room when room is empty
#define BUSY                    3           // Indicates busy state of room
#define WORKING                 0           // Indicates working state of student
#define WAITING                 1           // Indicates waiting state of student
#define LEAVING                 2           // Indicates leaving state of student
#define NOT_ENTERED             -1
#define UNDEFINED               -1

#define PROGRAM_NAME    "DEULIB"
#define gotoxy(x,y)     printf("\033[%d;%dH", (y), (x))
#define clear()         printf("\033[H\033[J")
#define COLOR_RED       "\x1b[31m"
#define COLOR_GREEN     "\x1b[32m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_BLUE      "\x1b[34m"
#define COLOR_MAGENTA   "\x1b[35m"
#define COLOR_CYAN      "\x1b[36m"
#define COLOR_RESET     "\x1b[0m"

#define BOOL int

/*
    Room struct keeps all information about one room
    This passed to room thread as a parameter and gives thread an identity
    number: The id number of room starting from 1
    state: Stores current activity of room
    student_number: Stores current student number in room
    student_id_arr: Stores ids of students in the room
    times_used: Indicates this room how many times used. This value is used to select rooms for students and less used room has higher priority.
*/
typedef struct working_room{

    int number;
    int state;
    int student_number;
    int* student_id_arr;
    int times_used;

} room;

/*
    Student struct keeps all information about one student
    This passed to student thread as a parameter and gives thread an identity
    number: The id number of student starting from 1
    state: Keeps current state of room
    room_number: The room number that student is assigned to
*/
typedef struct student{

    int number;
    int state;
    int room_number;

} student;


void init_room_student(void);
void init_semaphores(void);
void init_threads(pthread_t*, void*, void**, int, int);
void join_threads(pthread_t*, int);
void* student_thread(void*);
void* room_thread(void*);
int get_most_full_room(void);
void* print_simulation(void);
void add_message(char*);
int comparator(room*, room*);
void room_arr_copy(room**, room**, int);
void sort(room**, int);


student** students;                         // An struct array that keeps all students and their information
room** rooms;                               // An struct array that keeps all rooms and their information
char* message_arr[MAX_MESSAGE_NUMBER];      // Message array that contains messages that send by students and rooms
int message_number = 0;                     // Total number of sent messages
sem_t mutex;                                // A mutex that is used to synchronize access to critical regions
sem_t rooms_sem[ROOM_NUMBER];               // A semaphore array of rooms. This semaphores initialized with 0 value. This means rooms have to wait until any student posts room's semaphore.
sem_t rooms_mutex[ROOM_NUMBER];             // A mutex array of rooms. This mutexes are used to synchronize mutual access of students that uses common room
sem_t students_sem;                         // A semaphore that is initialized according to room number multiplied by room capacity.
sem_t msg_mutex;                            // Student and room threads uses same message array and message number. This mutex is used to synchronize access to this common variables
int total_outgoing_student_number = 0;      // Keeps total student number that left from library
struct winsize window;                      // Used to get terminal size
struct timeval start;                       // Used to reach current time unit of nanoseconds



int main(void){

    srand(time(NULL));
    ioctl(0, TIOCGWINSZ, &window);

    if(window.ws_col < 110 || window.ws_row < 45){
        puts("Please stretch your terminal to run simulation properly\nPress " COLOR_YELLOW "\033[1mENTER\033[0m" COLOR_RESET " to continue...");
        getchar();
    }

    pthread_t students_t[STUDENT_NUMBER];   // Student threads
    pthread_t rooms_t[ROOM_NUMBER];         // Room threads
    pthread_t simulation_t[1];              // Simulation thread

    init_room_student(); // Initializing student and room arrays with default values
    init_semaphores();   // Initializing semaphores

    init_threads(simulation_t, print_simulation, NULL, 1, FALSE); // Inıtializing simuleation thread
    gettimeofday(&start, NULL); // The start time of room and student threads are stored in start struct
    init_threads(rooms_t, room_thread, (void**)rooms, ROOM_NUMBER, FALSE); // Initializing room threads
    init_threads(students_t, student_thread, (void**)students, STUDENT_NUMBER, TRUE); // Initializing student threads


    join_threads(students_t, STUDENT_NUMBER); // Joining student threads
    join_threads(simulation_t, 1); // Joining simulation thread


    while(total_outgoing_student_number != STUDENT_NUMBER); // Program waiting until all students are left.


    if(total_outgoing_student_number == STUDENT_NUMBER){
        for(int i = 0 ; i < ROOM_NUMBER ; i++){
            pthread_cancel(rooms_t[i]); // Rooms always wait for new students even if there is no new student. So, room threads are canceled when all students has left.
        }
    }

    gotoxy(2, 8 + ROOM_NUMBER * 3);
    printf("Press " COLOR_YELLOW "\033[1mENTER\033[0m" COLOR_RESET " to show logs.%20s\n", " ");
    getchar();

    for(int k = 0 ; k < message_number ; k++){ // Printing all messages after simulation end
        printf(" %s", message_arr[k]);
    }

    return 0;
}

/*
    Initializing room and student arrays with default values
*/
void init_room_student(void){

    students = (student**) malloc(sizeof(student*) * STUDENT_NUMBER);
    rooms = (room**) malloc(sizeof(room*) * ROOM_NUMBER);

    int i = 0;
    for(i = 0 ; i < STUDENT_NUMBER ; i++){
        students[i] = (student*) malloc(sizeof(student));
        students[i]->number = i + 1;
        students[i]->state = NOT_ENTERED;
        students[i]->room_number = UNDEFINED;
    }
    for(i = 0 ; i < ROOM_NUMBER ; i++){
        rooms[i] = (room*) malloc(sizeof(room));
        rooms[i]->number = i + 1;
        rooms[i]->state = EMPTY;
        rooms[i]->student_number = 0;
        rooms[i]->student_id_arr = (int*) malloc(sizeof(int) * ROOM_CAPACITY);
        rooms[i]->times_used = 0;
    }

}

/*
    Initializing all semaphores
*/
void init_semaphores(void){

    int i = 0;
    sem_init(&mutex, 0, 1); // Mutex starts from 1 and first access will be accepted without any sem_post
    sem_init(&msg_mutex, 0, 1); // Mutex starts from 1 and first access will be accepted without any sem_post
    sem_init(&students_sem, 0, ROOM_CAPACITY * ROOM_NUMBER); //Student semaphore starts from room_capacity * room_number because this value indicates maximum number of working student in the rooms. Other students have to wait until any room is empty and this semaphore is posted.
    for(i = 0 ; i < ROOM_NUMBER ; i++){
        sem_init(&rooms_sem[i], 0, 0);  // Rooms semaphores starts from zero because room has to wait until any student comes
        sem_init(&rooms_mutex[i], 0, 1); // Room mutexes start from 1
    }
}

/*
    Initializes threads according to given parameters
    threads: Thread array
    function: Function that threads will run
    struct_arr: Parameters that will send to threads
    size: Number of threads will be created
    allow_periods: Indicates threads will be created periodically or directly
*/
void init_threads(pthread_t* threads, void* function, void** struct_arr, int size, int allow_periods){

    int i = 0;
    for(i = 0 ; i < size ; i++){
        if(struct_arr == NULL){
            pthread_create(&threads[i], NULL, function, NULL);
        }
        else{
            pthread_create(&threads[i], NULL, function, struct_arr[i]);
        }
        if((i + 1) % STUDENT_NUMBER_PERIOD == 0 && allow_periods){
            usleep(rand() % STUDENT_INCOMING_PERIOD);
        }
    }
}

/*
    Joins threads according to given parameters
    threads: Thread array
    size: Size of thread array
*/
void join_threads(pthread_t* threads, int size){

    int i = 0;
    for(i = 0 ; i < size ; i++){
        pthread_join(threads[i], NULL);
    }
}

/*
    Performs operation for a room
    Run by a thread
    room_ptr: Room struct that keeps room information
*/
void* room_thread(void* room_ptr){

    room* rm = (room*)room_ptr;
    int i = 0;

    while(TRUE){

        if(rm->student_number < ROOM_CAPACITY){ // Indicates room is not full and must wait for a incoming student

            sem_wait(&rooms_sem[rm->number - 1]); // Room waits here until any student posts this semaphore
            if(rm->state == EMPTY){
                rm->state = ANNOUNCING; // Room keeper is awaken
                char* open_msg;
                struct timeval open_stop;
                gettimeofday(&open_stop, NULL);
                asprintf(&open_msg, "Room keeper %d has " COLOR_RED "\033[1mOPENED\033[0m" COLOR_RESET " the room! " COLOR_GREEN "\t\t\t\t%d ms" COLOR_RESET "\n", rm->number,(int)(((double)(open_stop.tv_usec - start.tv_usec) / 1000000 + (double)(open_stop.tv_sec - start.tv_sec)) * 1000));
                add_message(open_msg);
            }

            // Room keeper announces left empty seat number
            char* anounce_msg;
            struct timeval announce_stop;
            gettimeofday(&announce_stop, NULL);
            asprintf(&anounce_msg, "Room keeper %d is " COLOR_RED "\033[1mANNOUNCING\033[0m" COLOR_RESET " %d empty seat left! " COLOR_GREEN "\t\t%d ms" COLOR_RESET "\n", rm->number, ROOM_CAPACITY - rm->student_number, (int)(((double)(announce_stop.tv_usec - start.tv_usec) / 1000000 + (double)(announce_stop.tv_sec - start.tv_sec)) * 1000));
            add_message(anounce_msg);
            sem_post(&rooms_mutex[rm->number - 1]); // Allows to new student to continue that is assigned to this room.
            /*
                 Actually program can run properly without this mutex
                 but announcing messages are not working properly because
                 student_number is common variable for students that are
                 assigned to this room and they can change this variable
                 before it printed.
                 Example condition occurs if mutex is not used:

                    Room keeper 3 ANNOUNCING 2 empty seat left
                    Room keeper 3 ANNOUNCING 2 empty seat left
                    Room keeper 3 ANNOUNCING 0 empty seat left
                    Room keeper 3 ANNOUNCING 0 empty seat left

                *Only message is wrong, program runs properly
            */
        }
        else{ // If student number of room reached to ROOM_CAPACITY

            char* full_msg;
            struct timeval full_stop;
            gettimeofday(&full_stop, NULL);
            asprintf(&full_msg, "Room %d is " COLOR_RED "\033[1mFULL\033[0m" COLOR_RESET " capacity! " COLOR_GREEN "\t\t\t\t\t%d ms" COLOR_RESET "\n", rm->number,(int)(((double)(full_stop.tv_usec - start.tv_usec) / 1000000 + (double)(full_stop.tv_sec - start.tv_sec)) * 1000));
            add_message(full_msg);

            rm->state = BUSY; // Room state is updated as busy

            sleep(STUDENT_WORKING_TIME); // Room is sleeping before send student

            sem_wait(&mutex); // Enter critical region
            rm->times_used += 1;
            rm->state = CLEANING;

            for(i = 0; i < ROOM_CAPACITY ; i++){ // Changing states of students that are working in this room as leaving
                rm->student_number -= 1;
                students[rm->student_id_arr[i] - 1]->state = LEAVING;
                rm->student_id_arr[i] = 0;
                total_outgoing_student_number += 1;
            }

            sem_post(&mutex); // Exit critical region
            sleep(ROOM_CLEANING_TIME); // This is not compulsory, only makes simulation looking good. If it is not used we can not see when room is empty because new students directly enter room

            for(i = 0; i < ROOM_CAPACITY ; i++){
                sem_post(&students_sem); // Letting new students to find empty room to study
            }

        }

    }

    pthread_exit(NULL);
}


/*
    Performs operation for a student
    Run by a thread
    student_ptr: Student struct that keeps student information
*/
void* student_thread(void* student_ptr){

    student* st = (student*)student_ptr;

    if(st->state == NOT_ENTERED || st->room_number == UNDEFINED) // Student enters library
        st->state = WAITING;

    char* enter_msg;
    struct timeval enter_stop;
    gettimeofday(&enter_stop, NULL);
    asprintf(&enter_msg, "Student %d has " COLOR_BLUE "\033[1mENTERED\033[0m" COLOR_RESET " into library and started to wait! " COLOR_GREEN "\t%d ms" COLOR_RESET "\n" , st->number, (int)(((double)(enter_stop.tv_usec - start.tv_usec) / 1000000 + (double)(enter_stop.tv_sec - start.tv_sec)) * 1000));
    add_message(enter_msg);

    sem_wait(&students_sem); // If there is no empty room students have to wait
    sem_wait(&mutex); // Enter critical region
    st->room_number = get_most_full_room(); // Student is assigned to most full less used room
    if(st->room_number == -1){ // This condition never happening while program is working properly (I tested so many times :) ). But if it comes true, program will crush down
        /*
            This condition can be true if and only if all rooms are full and any room posted students_sem while it is still full. This is impossible
        */
        printf("FATAL: AN UNEXPECTED ERROR HAS OCCURED AND STUDENT %d HAS BEEN TERMINATED!!!\n", st->number);
        pthread_exit(NULL);
    }

    st->state = WORKING; // Student is assigned to a room and started working
    char* work_msg;
    struct timeval work_stop;
    gettimeofday(&work_stop, NULL);
    asprintf(&work_msg, "Student %d " COLOR_BLUE "\033[1mWORKING\033[0m" COLOR_RESET " in the %d. room! " COLOR_GREEN "\t\t\t\t%d ms" COLOR_RESET "\n", st->number, st->room_number, (int)(((double)(work_stop.tv_usec - start.tv_usec) / 1000000 + (double)(work_stop.tv_sec - start.tv_sec)) * 1000));
    add_message(work_msg);
    sem_wait(&rooms_mutex[st->room_number - 1]); // If any student is assigned to same room before this student must wait until room keeper sends announce and posts mutex.
    rooms[st->room_number - 1]->student_id_arr[rooms[st->room_number - 1]->student_number] = st->number; // Number of this student is added to student number array of assigned room
    rooms[st->room_number - 1]->student_number += 1; // Student number of assigned room is increased
    sem_post(&rooms_sem[st->room_number - 1]); // Waking up room keeper or letting to announce
    sem_post(&mutex); // Exit critical region


    long starvation_detect_start = time(NULL);
    while(st->state == WORKING){ // Student working until state is changed by room. Room changes this state to leaving if it is full

        /*
            If the room never be full, student detects that he worked too much. And leaves the room.
        */
        if(time(NULL) - starvation_detect_start > STUDENT_WORKING_TIME + 3){
            sem_wait(&mutex);
            char* starvation_msg;
            struct timeval startvation_stop;
            gettimeofday(&startvation_stop, NULL);
            asprintf(&starvation_msg, COLOR_RED "STARVATION DETECTED " COLOR_RESET "Student %d " COLOR_BLUE "\033[1mLEAVING\033[0m" COLOR_RESET " from %d. room! " COLOR_GREEN "\t\t%d ms" COLOR_RESET "\n", st->number, st->room_number, (int)(((double)(startvation_stop.tv_usec - start.tv_usec) / 1000000 + (double)(startvation_stop.tv_sec - start.tv_sec)) * 1000));
            add_message(starvation_msg);
            rooms[st->room_number-1]->student_number -= 1;
            st->state = LEAVING;
            total_outgoing_student_number += 1;
            sem_post(&mutex);
            pthread_exit(NULL);
        }
    }

    char* leave_msg;
    struct timeval leave_stop;
    gettimeofday(&leave_stop, NULL);
    asprintf(&leave_msg, "Student %d " COLOR_BLUE "\033[1mLEAVING\033[0m" COLOR_RESET " from %d. room! " COLOR_GREEN "\t\t\t\t%d ms" COLOR_RESET "\n", st->number, st->room_number, (int)(((double)(leave_stop.tv_usec - start.tv_usec) / 1000000 + (double)(leave_stop.tv_sec - start.tv_sec)) * 1000));
    add_message(leave_msg);

    pthread_exit(NULL);
}

/*
    Returns most full and less used room number
*/
int get_most_full_room(void){

    int i = 0;
    int max_student_number = -1;
    int most_full_room_id = -1;
    room** rooms_tmp = (room**)malloc(sizeof(room*) * ROOM_NUMBER);
    room_arr_copy(rooms_tmp, rooms, ROOM_NUMBER); // Copies rooms array to another array because it will sort this array and real array should not be affected

    sort(rooms_tmp, ROOM_NUMBER); // Sorts rooms according to their times_used variable

    for(i = 0 ; i < ROOM_NUMBER ; i++){

        /*
            Selects room that is the most full but not completely full.
            This selection gives most full less used room because the
            array is already sorted and it gives priority to rooms that
            has lower index if rooms have same student number
        */

        if(rooms_tmp[i]->student_number < ROOM_CAPACITY && rooms_tmp[i]->state != BUSY && rooms_tmp[i]->student_number > max_student_number){

            max_student_number = rooms_tmp[i]->student_number;
            most_full_room_id = rooms_tmp[i]->number;
        }
    }

    return most_full_room_id;
}


/*
    Prints all data reached from rooms and students arrays.
    There are too many magical numbers that is used to align values.
    It is not worth to explain.
*/
void* print_simulation(void){

    int leaving_students[STUDENT_NUMBER] = { 0 };
    int leaving_student_number = 0;

    while(leaving_student_number < STUDENT_NUMBER){

        clear();
        gotoxy(window.ws_col / 2 - strlen(PROGRAM_NAME), 0);
        printf(COLOR_BLUE PROGRAM_NAME COLOR_RESET "\n");


        gotoxy(3, 3);
        printf(COLOR_MAGENTA "ROOM\n NUMBER" COLOR_RESET "\n");
        gotoxy((ROOM_CAPACITY * 7 - 6 )/ 2 + 10, 3);
        printf(COLOR_MAGENTA "STUDENT" COLOR_RESET "\n");
        gotoxy((ROOM_CAPACITY * 7 - 6 )/ 2 + 10, 4);
        printf(COLOR_MAGENTA "NUMBERS" COLOR_RESET "\n");
        gotoxy(ROOM_CAPACITY * 7 + 13, 3);
        printf(COLOR_MAGENTA "TIMES" COLOR_RESET "\n");
        gotoxy(ROOM_CAPACITY * 7 + 13, 4);
        printf(COLOR_MAGENTA "USED" COLOR_RESET "\n");

        int i = 0;
        // Start draw room slots
        for(i = 0 ; i < ROOM_NUMBER ; i++){
            int j = 0;
            gotoxy(4, 6 + i * 3);
            printf(COLOR_YELLOW "%2d" COLOR_RESET "\n", i + 1);
            gotoxy(ROOM_CAPACITY * 7 + 15, 6 + i * 3);
            printf(COLOR_YELLOW "%d" COLOR_RESET "\n", rooms[i]->times_used);
            for(j = 0 ; j < ROOM_CAPACITY ; j++){
                gotoxy(10 + j * 7, 5 + i * 3);
                printf(COLOR_GREEN "________\n" COLOR_RESET "\n");
                gotoxy(10 + j * 7, 6 + i * 3);
                printf(COLOR_GREEN "|      |\n" COLOR_RESET "\n");
                gotoxy(10 + j * 7, 7 + i * 3);
                printf(COLOR_GREEN "‾‾‾‾‾‾‾‾\n" COLOR_RESET "\n");
            }
        }
        // End draw room slots

        // Start fill room slots
        for(i = 0 ; i < ROOM_NUMBER ; i++){
            int j = 0;
            for(j = 0 ; j < rooms[i]->student_number ; j++){
                gotoxy(13 + j * 7, 6 + i * 3);
                printf(COLOR_RESET "%d" COLOR_RESET "\n",rooms[i]->student_id_arr[j]);
            }
        }
        // End fill room slots

        //Start list waiting students
        int student_num_per_line = (int)(STUDENT_NUMBER / ROOM_NUMBER);
        gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + (student_num_per_line * 4 - 8)/2, 3);
        printf(COLOR_MAGENTA "STUDENTS" COLOR_RESET "\n");
        gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + (student_num_per_line * 4 - 8)/2, 4);
        printf(COLOR_MAGENTA "WAITING" COLOR_RESET "\n");
        int line = 0;
        int count = 0;
        for(i = 0 ; i < STUDENT_NUMBER ; i++){

            if(students[i]->state == WAITING){
                gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + count * 4, line + 6);
                printf(COLOR_RED "%d " COLOR_RESET "\n", students[i]->number);
                count += 1;

                if(count == student_num_per_line){
                    line += 1;
                    count = 0;
                }
            }
        }
        //End list waiting students

        //Start list leaving students
        int z = 0;
        for(z = 0 ; z < STUDENT_NUMBER ; z++){

            if(students[z]->state == LEAVING){

                BOOL flag = FALSE;
                int t = 0;
                for(t = 0 ; t < leaving_student_number ; t++){
                    if(leaving_students[t] == students[z]->number){
                        flag = TRUE;
                    }
                }
                if(!flag){
                    leaving_students[leaving_student_number++] = students[z]->number;
                }
            }
        }

        gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + (student_num_per_line * 4 - 8)/2, 10 + STUDENT_NUMBER / student_num_per_line);
        printf(COLOR_MAGENTA "STUDENTS" COLOR_RESET "\n");
        gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + (student_num_per_line * 4 - 8)/2, 11 + STUDENT_NUMBER / student_num_per_line);
        printf(COLOR_MAGENTA "LEAVING" COLOR_RESET "\n");
        count = 0;
        line = 0;
        for(i = 0 ; i < leaving_student_number ; i++){


            gotoxy(ROOM_CAPACITY * 7 + 18 + 5 + count * 4, line + 13 + STUDENT_NUMBER / student_num_per_line);
            printf(COLOR_RED "%d " COLOR_RESET "\n", leaving_students[i]);
            count += 1;

            if(count == student_num_per_line){
                line += 1;
                count = 0;
            }
        }
        //End list leaving students

        gotoxy(2, 8 + ROOM_NUMBER * 3);
        puts("Logs will be available after simulation end.");

        usleep(100000); //100 MILISECONDS
    }

    pthread_exit(NULL);
}

/*
    Adds message to message_arr synchronously
    message: Message that will be added
*/
void add_message(char* message){

    sem_wait(&msg_mutex);
    message_arr[message_number++] = message;
    sem_post(&msg_mutex);
}

/*
    Copies room array to given another array
    dest: Destionation array
    src: Source array
    len: Length of source array
*/
void room_arr_copy(room** dest, room** src, int len){

    int i = 0;

    for(i = 0 ; i < len ; i++){

        dest[i] = (room*) malloc(sizeof(room));
        dest[i]->number = src[i]->number;
        dest[i]->state = src[i]->state;

        dest[i]->student_id_arr = (int*)malloc(sizeof(int) * ROOM_CAPACITY);
        int j = 0;
        for(j = 0 ; j < ROOM_CAPACITY ; j++){
            dest[i]->student_id_arr[j] = src[i]->student_id_arr[j];
        }

        dest[i]->student_number = src[i]->student_number;
        dest[i]->times_used = src[i]->times_used;

    }
}

/*
    Bubble sort algorithm to sort given room array according to comparator function
    src: Source array
    len: Length of source array
*/
void sort(room** src, int len){

    int i = 0, j = 0;

    for(i = 0 ; i < len-1 ; i++){
        for(j = 0 ; j < len-i-1 ; j++){
            if(comparator(src[j], src[j+1])){
                room* tmp = src[j];
                src[j] = src[j+1];
                src[j+1] = tmp;
            }
        }
    }
}

/*
    Compares two rooms according to their times_used variable
    rm_1: First room
    rm_2: Second room
*/
int comparator(room* rm_1, room* rm_2) {

    return (rm_1->times_used > rm_2->times_used);

}

