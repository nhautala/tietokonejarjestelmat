Void dataCollectionTask(UArg arg0, UArg arg1) {
    I2C_Handle i2c;
    I2C_Params i2cParams;

    I2C_Params_init(&i2cParams);
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);

    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    Task_sleep(100000 / Clock_tickPeriod); // Anna anturin asettua

    while (dataIndex < DATA_SIZE) {
        float ax, ay, az, gx, gy, gz;
        mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);

        // Tallenna data taulukkoon
        sensorData[dataIndex][0] = ax;
        sensorData[dataIndex][1] = ay;
        sensorData[dataIndex][2] = az;
        sensorData[dataIndex][3] = gx;
        sensorData[dataIndex][4] = gy;
        sensorData[dataIndex][5] = gz;

        dataIndex++;

        Task_sleep(1000000 / Clock_tickPeriod); // Pysäytä keräys 1 sekunniksi (voit säätää aikaväliä tarpeen mukaan)
    }

    // Tulosta kerätty data debug-ikkunaan
    for (int i = 0; i < dataIndex; i++) {
        System_printf("Data %d: %.4f %.4f %.4f %.4f %.4f %.4f\n", i + 1,
                      sensorData[i][0], sensorData[i][1], sensorData[i][2],
                      sensorData[i][3], sensorData[i][4], sensorData[i][5]);
    }

    System_flush();
}


void detectMotion(float ax, float ay, float az, float gx, float gy, float gz) {
    // Check for motion in the z-axis
    if (fabs(az) > MOTION_THRESHOLD) {
        playSound();
    }

    // Check for motion in the x or y-axis
    if (fabs(ax) > MOTION_THRESHOLD || fabs(ay) > MOTION_THRESHOLD) {
        flashLED();
    }
}

void playSound() {
    // Ensure that the global 'uart' handle is initialized before calling this function
    if (uart == NULL) {
        System_printf("UART handle not initialized for playSound\n");
        System_flush();
        return;
    }

    // Send the sound command using the existing UART handle
    const char *soundCommand = "PLAY_SOUND\n";
    UART_write(uart, soundCommand, strlen(soundCommand));
    System_printf("Playing sound!\n");
    System_flush();
}

void flashLED() {
    // Ensure that the global 'ledHandle' is initialized before calling this function
    if (ledHandle == NULL) {
        System_printf("LED handle not initialized for flashLED\n");
        System_flush();
        return;
    }

    // Turn on the LED
    GPIO_write(ledHandle, Board_LED0, Board_LED_ON);
    Task_sleep(500000 / Clock_tickPeriod); // Keep the LED on for 0.5 seconds

    // Turn off the LED
    GPIO_write(ledHandle, Board_LED0, Board_LED_OFF);
}