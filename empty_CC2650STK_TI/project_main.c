/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"

/* Task stack size */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

/* liiketunnistuskynnys */
#define MOTION_THRESHOLD 0.5

// JTKJ: Tehtävä 3. Tilakoneen esittely
// JTKJ: Exercise 3. Definition of the state machine
enum state { WAITING = 1, DATA_READY };
enum state programState = WAITING;

// JTKJ: Tehtävä 3. Valoisuuden globaali muuttuja
// JTKJ: Exercise 3. Global variable for ambient light
double ambientLight = -1000.0;

// JTKJ: Tehtävä 1. Lisää painonappien RTOS-muuttujat ja alustus
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// JTKJ: Exercise 1. Add pins RTOS-variables and configuration here
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    // JTKJ: Tehtävä 1. Vilkuta jompaa kumpaa lediä
    // JTKJ: Exercise 1. Blink either led of the device
    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;
    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {
    UART_Params uartParams;
    UART_Handle uart;

    UART_Params_init(&uartParams);
    uartParams.baudRate = 9600;
    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL) {
        System_abort("Error Initializing UART\n");
    }

    while (1) {
        if (programState == DATA_READY) {
            // JTKJ: Tehtävä 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
            //       Muista tilamuutos
            // JTKJ: Exercise 3. Print out sensor data as a string to the debug window if the state is correct
            //       Remember to modify the state
            System_printf("Valoisuus: %.2f lux\n", ambientLight);
            System_flush();
            programState = WAITING;
        }

        if (programState == DATA_READY) {
            char buffer[50];
            int len = snprintf(buffer, sizeof(buffer), "Valoisuus: %.2f lux\n", ambientLight);
            // JTKJ: Tehtävä 4. Lähetä sama merkkijono UARTilla
            // JTKJ: Exercise 4. Send the same sensor data string with UART
            UART_write(uart, buffer, len);
            programState = WAITING;
        }

        System_printf("uartTask\n");
        System_flush();
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {
    I2C_Handle i2c;
    I2C_Params i2cParams;

    I2C_Params_init(&i2cParams);
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);

    while (1) {
        // lisätään tarkkailtavaksi liiketiedoksi detectMotion()
        float ax, ay, az, gx, gy, gz;
        mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);
        detectMotion(ax, ay, az, gx, gy, gz);
        
        uint16_t lux = opt3001_get_data(i2c);
        System_printf("Mittausarvo: %u lux\n", lux);
        System_flush();

        ambientLight = (double)lux;

        System_printf("sensorTask\n");
        System_flush();
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

void detectMotion(float ax, float ay, float az, float gx, float gy, float gz) {
    
    // jos laitetta liikutetaan ylös tai alas soitetaan äänimerkki
    if (fabs(az) > MOTION_THRESHOLD) {
        playSound();
    }
    
    // jos laitetta liikutetaan vaakatasossa väläytetään LEDiä
    if (fabs(ax) > MOTION_THRESHOLD || fabs  > MOTION_THRESHOLD) {
        flashLED();
    }
}

void playSound() {
    UART_Params uartParams;
    UART_Handle uart;
    
    UART_Params_init(&uartParams);
    uartParams.baudRate = 9600;
    uart = UART_open(Board_UART1, &uartParams);
    
    if (uart == NULL) {
        System_printf("Error initializing UART for sound\n");
        System_flush();
        return;
    }
    
    // lähetetään äänimerkki
    const char *soundCommand = "PLAY_SOUND\n";
    UART_write(uart, soundCommand, strlen(soundCommand));
    System_printf("Playing sound!\n");
    System_flush();
}

void flashLED() {
    // LEDin vilkuttaminen
    GPIO_write(Board_LED0, Board_LED_ON);
    Task_sleep(500000 / Clock_tickPeriod); // Pidä LED päällä 0.5 sekuntia
    GPIO_write(Board_LED0, Board_LED_OFF);
}

Int main(void) {
    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle) {
        System_abort("Error initializing LED pin\n");
    }

    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle) {
        System_abort("Error initializing button pin\n");
    }

    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
        System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    System_printf("Hello world!\n");
    System_flush();

    BIOS_start();

    return (0);
}
