        //fp_0 = fopen(valuePath_0, "r");
        //fp_1 = fopen(valuePath_1, "r");
        // loop to monitor events

        while(1){
                fp_0 = fopen(valuePath_0, "r");
                fp_1 = fopen(valuePath_1, "r");
                // default state is 1 since buttons are configured with
                // internal pull-up resistors. So, reading 0 means button is pressed
                fscanf(fp_0, "%d", &(state[0]));
                fscanf(fp_1, "%d", &(state[1]));
                fclose(fp_0);
                fclose(fp_1);
                // event 0 detected trigger callback with corresponding


                if( state[0] == 0 ){
                        event_callback(&event_handler_0);
                }
                // event 1 detected trigger callback with corresponding


                if( state[1] == 0 ){
                        event_callback(&event_handler_1);
                }
        }

}

void* outputThread(void* input) {
        // put in things relating to reading the temp sensor
        int fd = open(AIN_DEV, O_RDONLY);
        while (1)
        {
                char buffer[1024];
                // Read the temperature sensor data
                int ret = read(fd, buffer, sizeof(buffer));
                if (ret != -1)
                {
                        buffer[ret] = '\0';
                        double celsius = temperature(buffer);
                        double fahrenheit = CtoF(celsius);
                        printf("digital value: %s celsius: %f fahrenheit: %f\n", buffer, celsius,
                        fahrenheit);
                        lseek(fd, 0, 0);
                }
                sleep(1);
        }
        close(fd);
}




int main(){
        char pin_number[32] = "P8_13";
        config_pin(pin_number, "pwm");

        // ----------------------------------------------------------
        int data = 6;
        pthread_t thread_id[2];
        // instantiating argument required for
        // thread creation
        printf("Before Threads \n");

        pthread_create(&(thread_id[0]), NULL, outputThread, (void*)(&data) );
        pthread_create(&(thread_id[1]), NULL, inputThread, (void*)(&data) );
        pthread_join(thread_id[0], NULL);
        pthread_join(thread_id[1], NULL);
        printf("After Threads \n");
        pthread_exit(0);
}
                       