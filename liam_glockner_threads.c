
//Liam Glockner
//ECE 231, 32 Bit Lab 1
//4/14/2021

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Header file for sleep function
#include <pthread.h> // Header file for thread library
#include <time.h>
#include <sys/epoll.h>

#define SIZE 10


long buffer[SIZE]; //shared buffer array
int count = 0; // #chars in buffer array
int putIndex = 0; // buffer write variable
int getIndex = 0; // buffer read variable

pthread_mutex_t lock; //mutex lock variable


void configure_gpio_input(int gpio_number){//function designed to configure gpiopin as input pin
        char gpio_num[10];
        sprintf(gpio_num, "export%d", gpio_number);
        const char* GPIOExport="/sys/class/gpio/export";
        FILE* fp = fopen(GPIOExport, "w");
        fwrite(gpio_num, sizeof(char), sizeof(gpio_num), fp);
        fclose(fp);
        char GPIODirection[40];
        sprintf(GPIODirection, "/sys/class/gpio/gpio%d/direction", gpio_number);
        fp = fopen(GPIODirection, "w");
        fwrite("in", sizeof(char), 2, fp);
        fclose(fp);
}

void configure_interrupt(int gpio_number){//configures interrupt on pin prvoided

        configure_gpio_input(gpio_number);
        char InterruptEdge[40];
        sprintf(InterruptEdge, "/sys/class/gpio/gpio%d/edge", gpio_number);
        FILE* fp = fopen(InterruptEdge, "w");
        fwrite("falling", sizeof(char), 7, fp); //configured for tracking falling edges in our case
        fclose(fp);
}

void* inputThread(void* input) { //first thread, responsible for filling buffer and taking timestamp values
        struct timespec start, end;
        long time_ns;
        double time_sec;
        int interrupt;
        int gpio_number = 67;

        char InterruptPath[40];
        sprintf(InterruptPath, "/sys/class/gpio/gpio%d/value", gpio_number);

        int epfd; // next section structures the epoll event handling based on manual instructions
        struct epoll_event events, ev;
        FILE* fp = fopen(InterruptPath, "r");
        int fd = fileno(fp);

        epfd = epoll_create(1);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        char c = 0; //c starts at A

   	while(c <= 1100){

                pthread_mutex_lock(&lock); //critical section starts, lock starts

                while (count == SIZE) { // if buffer full release lock
                        pthread_mutex_unlock(&lock);
                        pthread_mutex_lock(&lock);
                }

                interrupt = epoll_wait(epfd, &events, 1, -1);
                clock_gettime(CLOCK_MONOTONIC, &start); // get clocktime
                time_ns = (start.tv_sec)*1000000000 + start.tv_nsec;
                time_sec = time_ns/1000000000.0; //calculate time in seconds
                count++;
                buffer[putIndex] = time_ns;
                putIndex++;

                if (putIndex == SIZE) {
                        putIndex = 0;
                }
                c++;
                pthread_mutex_unlock(&lock); //unlock section
        }
}

void* outputThread(void* input) { // second thread designed to read buffer and print the average period in seconds
        long time_sec;
        double totalT = 0; //time variable
        char ch=0; // goes from 0 to 100

        while(ch <= 100){
                totalT = 0;
                pthread_mutex_lock(&lock); // critical section starts

                while (count == 0) { //if buffer empty release lock
                        pthread_mutex_unlock(&lock);
                        pthread_mutex_lock(&lock);
                }
                // next section  reads char from shared array and print result
                ch++;
                while(getIndex < 9){
                        count--;
                        time_sec = buffer[getIndex+1]-buffer[getIndex];
                        totalT += time_sec/1000000000.0;
                        getIndex++;
                }
                count--;
                getIndex = SIZE;
                totalT = totalT/9;
                printf("Period = %f Seconds. Count is %d\n", totalT, ch);

                // after last char, index = 0
                if (getIndex == SIZE) {
                        getIndex = 0;
                }

                pthread_mutex_unlock(&lock); // critical section ends

                // exit the thread using break
                if(ch >= 100){
                        break;
                }
        }
}

int main(){
        int gpio_number = 67; //corresponds to P8_8
        configure_interrupt(gpio_number); //enable interrupt on gpiopin
        int data = 6;

        pthread_t thread_id[2];
        printf("Before Threads \n");
        //lock starts
        if (pthread_mutex_init(&lock, NULL) != 0) {
                printf("\n mutex init has failed\n");
                return 1;
        }
        //ceate the two threads needed using second approach given in manual
        pthread_create(&(thread_id[0]), NULL, outputThread, (void*)(&data) );
        pthread_create(&(thread_id[1]), NULL, inputThread, (void*)(&data) );
        pthread_join(thread_id[0], NULL);
        pthread_join(thread_id[1], NULL);
        pthread_mutex_destroy(&lock); //destroy the lock
        printf("After Threads \n");
        pthread_exit(0);
}


