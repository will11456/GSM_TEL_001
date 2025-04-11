#ifndef MAIN_PIN_MAP_H_
#define MAIN_PIN_MAP_H_

// I2C configuration
#define SDA_PIN GPIO_NUM_11
#define SCL_PIN GPIO_NUM_10

//Inputs
#define INPUT_1 GPIO_NUM_6
#define INPUT_2 GPIO_NUM_7

//Outputs
#define OUTPUT_1 GPIO_NUM_4
#define OUTPUT_2 GPIO_NUM_5

//Modem UART
#define MODEM_RX GPIO_NUM_12
#define MODEM_TX GPIO_NUM_13

//Modem Control
#define PWR_KEY GPIO_NUM_18
#define RAIL_4V_EN GPIO_NUM_21

//Test Pads
#define TEST_PAD_1 GPIO_NUM_8
#define TEST_PAD_2 GPIO_NUM_36
#define TEST_PAD_3 GPIO_NUM_9
#define TEST_PAD_4 GPIO_NUM_37


#endif /* MAIN_PIN_MAP_H_ */
