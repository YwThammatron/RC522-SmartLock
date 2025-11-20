/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "string.h"
#include "stdbool.h"
#include "RC522.h"
#include "ILI9341_Touchscreen.h"
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ROWS 4
#define COLS 4
#define MAX_PASSWORD_LEN 10
#define SIGNAL_PASS_PORT GPIOE
#define SIGNAL_PASS_PIN GPIO_PIN_7
#define SIGNAL_FAIL_PORT GPIOE
#define SIGNAL_FAIL_PIN GPIO_PIN_8


#define UID_LEN 5

#define MAX_SIZE 100
#define STR_LENGTH 50
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t rowPins[ROWS] = {GPIO_PIN_13, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_14};
GPIO_TypeDef* rowPorts[ROWS] = {GPIOF, GPIOE, GPIOE, GPIOF};

uint16_t colPins[COLS] = {GPIO_PIN_13, GPIO_PIN_15, GPIO_PIN_14, GPIO_PIN_9};
GPIO_TypeDef* colPorts[COLS] = {GPIOE, GPIOF, GPIOG, GPIOG};

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

char correctPassword[MAX_PASSWORD_LEN + 1] = "123456";

typedef enum {
	NORMAL,
	VALIDATE_OLD_PASSWORD,
	CHANGE_NEW_PASSWORD
} Mode_t;
Mode_t keypad_mode = NORMAL;



const uint8_t allowed_uids[][UID_LEN] = {
    {117, 218, 111, 6, 198},
    {241, 154, 56, 6, 84}
};
const size_t allowed_count = sizeof(allowed_uids) / UID_LEN;

uint8_t uid[UID_LEN];
uint8_t tagType[2];
uint8_t status;



typedef enum {
  SELECT_MODE,
  KEYPAD_MODE,
  RFID_MODE
} SystemMode_t;
SystemMode_t system_mode = SELECT_MODE;

//typedef enum {
//	FAIL, // IS_PASS=0, IS_CLOSE=1
//	OPEN, // IS_PASS=0, IS_CLOSE =0
//	SUCCESS // IS_PASS=1, IS_CLOSE=1
//} LogState_t;
//LogState_t current_state = IDLE;

uint8_t fail_count = 0;
bool is_locked = false;
uint32_t lock_start_time = 0;

//Servo variables
uint32_t current;
uint32_t prev = 0;

//ultrasonic variables
uint32_t IC_Val1 = 0;
uint32_t IC_Val2 = 0;
uint32_t Difference = 0;
uint8_t Is_First_Captured = 0;
float Distance = 0;

//State variables
uint8_t isClose = 1;
uint8_t isPass = 0;
uint8_t prev_state = NULL;
//Timer variables
uint32_t count = 0;
//debug variables


//wifi variables
//char SSID[] = "NaCl - GUEST";
//char PASSWORD[] = "N@CL601_Guest";
//char URL[] = "http://10.60.0.59:3000";
//
//uint32_t wifi_connect_interval = 0;

uint8_t screen = 0;
uint8_t p_screen = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
//void ESP_ConnectWifi(){
//	char cmd[100];
//	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", SSID, PASSWORD);
//	HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 100);
//}

//void ESP_HttpGet(char* path){
//	char cmd[100];
//	snprintf(cmd, sizeof(cmd), "AT+HTTPGET=\"%s/%s\"\r\n", URL, path);
//	HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 100);
//}

char getKey(void) {
	for (int r = 0; r < ROWS; r++) {
		HAL_GPIO_WritePin(rowPorts[r], rowPins[r], GPIO_PIN_RESET);
		HAL_Delay(1);

		for (int c = 0; c < COLS; c++) {
			if (HAL_GPIO_ReadPin(colPorts[c], colPins[c]) == GPIO_PIN_RESET) {
				HAL_Delay(30);

				if (HAL_GPIO_ReadPin(colPorts[c], colPins[c]) == GPIO_PIN_RESET) {
					while (HAL_GPIO_ReadPin(colPorts[c], colPins[c]) == GPIO_PIN_RESET) {
						HAL_Delay(10);
					}
					HAL_GPIO_WritePin(rowPorts[r], rowPins[r], GPIO_PIN_SET);

					return keys[r][c];
				}
			}
		}
		HAL_GPIO_WritePin(rowPorts[r], rowPins[r], GPIO_PIN_SET);
	}
	return 0;
}

static bool is_uid_allowed(uint8_t *uid) {
    for (size_t i = 0; i < allowed_count; ++i) {
        if (memcmp(uid, allowed_uids[i], UID_LEN) == 0) return true;
    }
    return false;
}

void led_show_result(uint8_t isPasswordCorrect) {
	if (isPasswordCorrect) {
		HAL_GPIO_WritePin(SIGNAL_PASS_PORT, SIGNAL_PASS_PIN, 1);
		HAL_Delay(3000);
		HAL_GPIO_WritePin(SIGNAL_PASS_PORT, SIGNAL_PASS_PIN, 0);
	}
	else {
		HAL_GPIO_WritePin(SIGNAL_FAIL_PORT, SIGNAL_FAIL_PIN, 1);
		HAL_Delay(3000);
		HAL_GPIO_WritePin(SIGNAL_FAIL_PORT, SIGNAL_FAIL_PIN, 0);
	}
}

void uart_print(const char *s) {
	HAL_UART_Transmit(&huart3, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//Servo functions
void Servo_Open(uint32_t delay) {
    // Angle should be between 0 and 180
    // Pulse width min-max: 1ms (500) to 2ms (2600) for 0° to 180°
	// Pulse width for this project: 600-2000
	// Best Delay for parameter is 190
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 2000);
    HAL_Delay(delay);
}

void Servo_Close(uint32_t delay) {
    // Angle should be between 0 and 180
    // Pulse width min-max: 1ms (500) to 2ms (2600) for 0° to 180°
	// Pulse width for this project: 600-2000
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 600);
    HAL_Delay(delay);
}

//Ultrasonic Callback function
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
        if (Is_First_Captured == 0) {
            IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_4, TIM_INPUTCHANNELPOLARITY_FALLING);
            Is_First_Captured = 1;
        } else {
            IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_4, TIM_INPUTCHANNELPOLARITY_RISING);
            Is_First_Captured = 0;

            if (IC_Val2 > IC_Val1)
                Difference = IC_Val2 - IC_Val1;
            else
                Difference = (0xFFFF - IC_Val1 + IC_Val2);

            Distance = Difference / 58.0;
        }

        //if distance is too close from ultrasonic then led on board turn on
        if(Distance >= 2.0 && Distance <= 3.5 ){
        	//Door closed
        	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_RESET);
        	isClose = 1;
        }
        else if(Distance > 3.5){
            //Door still open
        	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_SET);
        	isClose = 0;
        }

    }
}

void pwdEnter(char* entered){
	uint8_t censor = strlen(entered);
	char buffer[20];
	strcpy(buffer, entered);
	for (int i = 0; buffer[i] != '\0'; i++){
		buffer[i] = '*';
	}
	ILI9341_Draw_Text("000000000000", 161-(12*6), 130, WHITE, 2 , WHITE);
	ILI9341_Draw_Text(buffer, 161-(censor*6), 130, BLACK, 2, WHITE);
}

void LCD_MULT(char* buf){
	if (p_screen != screen){
		if (screen == 0){
				ILI9341_Fill_Screen(BLACK);
			}
			if (screen == 1){	//Enter password
				ILI9341_Fill_Screen(WHITE);
				ILI9341_Draw_Text("Enter Password...", 49, 100, BLACK, 2, WHITE);
			}
			if (screen == 2){
				ILI9341_Fill_Screen(WHITE);
				ILI9341_Draw_Text("[New] Password...", 69, 100, BLACK, 2, WHITE);
			}
			if (screen == 3){
				ILI9341_Fill_Screen(WHITE);
				ILI9341_Draw_Text("UNLOCKED", 113, 115, GREEN, 2, WHITE);
			}
			if (screen == 4){
				ILI9341_Draw_Text("INVALID ACCESS", 77, 170, RED, 2, WHITE);
			}
			if (screen == 5){
				ILI9341_Fill_Screen(WHITE);
				ILI9341_Draw_Text("TEMPORARY LOCKED", 71, 115, RED, 2, WHITE);
			}
			if (screen == 6){
				ILI9341_Fill_Screen(WHITE);
				ILI9341_Draw_Text("TAP YOUR RFID", 83, 115, BLACK, 2, WHITE);
			}
			if (screen == 7){
				ILI9341_Draw_Text("YOUR OLD PASSWORD", 59, 150, BLACK, 2, WHITE);
			}
			p_screen = screen;
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();
  MX_SPI5_Init();
  /* USER CODE BEGIN 2 */
  MFRC522_Init();

//  for (int i = 0; i < ROWS; i++) {
//	  ESP_ConnectWifi();
//	  HAL_Delay(500);
//  }

  HAL_TIM_Base_Start_IT(&htim4);

  for (int i = 0; i < ROWS; i++) {
	  HAL_GPIO_WritePin(rowPorts[i], rowPins[i], 1);
  }

  char entered[MAX_PASSWORD_LEN + 1];
  char buffer[64];
  uint8_t idx = 0;
  memset(entered, 0, sizeof(entered));

  uart_print("System Ready.\r\nPress A = Keypad | Press B = RFID\r\n");

  //Servo Ultrasonic PWM/Timer Start
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  ILI9341_Init();
  ILI9341_Set_Rotation(SCREEN_HORIZONTAL_1);
  ILI9341_Fill_Screen(BLACK);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if(isClose == 0){
		  if(prev_state != isClose){
			  ESP_HttpGet("info/door_is_opened\r\n");
		  }
		  if(isPass == 1){
			  //If Timer still counts then stop
			  HAL_TIM_Base_Stop_IT(&htim1);
			  count = 0;
			  //Reset Access when door is opened
			  isPass = 0;
		  }
		  else{
			  //If Door is opened for some sec then buzzer alert
			  if(count == 0){
				  HAL_TIM_Base_Start_IT(&htim1);
			  }
			  //count 100 = 1s
			  else if(count >= 1500 && count < 2000){
				  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
			  }
			  else if(count >= 2000){
				  uart_print("Door still open. Buzzer closed\r\n");
				  ESP_HttpGet("alert/door_still_open\r\n");
				  HAL_TIM_Base_Stop_IT(&htim1);
				  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
				  count = 200;
			  }
		  }
	  }
	  else {
		  if(prev_state != isClose){
			  ESP_HttpGet("info/door_is_closed\r\n");
		  }
		  if(isPass == 0){
			  if(wifi_connect_interval == 0){
				  ESP_ConnectWifi();
			  }
			  wifi_connect_interval++;
			  wifi_connect_interval %= 10;
			  Servo_Close(190);
			  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
			  HAL_TIM_Base_Stop_IT(&htim1);
			  count = 0;
			  screen = 0;
			  if(prev_state != isClose) {
				  system_mode = SELECT_MODE;
				  screen = 0;
				  uart_print("Press A = Keypad | Press B = RFID\r\n");
			  }
		  }
		  else{
			  Servo_Open(190);
			  //Start Timer to see if user not open door then close again
			  if(count == 0){
				 HAL_TIM_Base_Start_IT(&htim1);
			  }
			  //count 100 = 1s
			  else if(count >= 800){
				  ESP_HttpGet("info/unlock_but_door_is_closing\r\n");
				  uart_print("User didn't open the door, revoke access.\r\n");
				  HAL_TIM_Base_Stop_IT(&htim1);
				  Servo_Close(190);
				  isPass = 0;
				  count = 200;
				  screen = 0;
				  system_mode = SELECT_MODE;
			  }
		  }
	  }
	  prev_state = isClose;

	  char key;

	  if(!is_locked){
		  key = getKey();
	  }

	  if (system_mode == SELECT_MODE) {
		  if (key == 'A') {
			  system_mode = KEYPAD_MODE;
			  uart_print("Keypad Mode Selected.\r\n");
			  screen = 1;
		  }
		  else if (key == 'B') {
			  system_mode = RFID_MODE;
			  uart_print("RFID Mode Selected.\r\n");
			  screen = 6;
		  }
		  continue;
	  }

	  if(key == 'D') {
		  system_mode = SELECT_MODE;
		  uart_print("Press A = Keypad | Press B = RFID\r\n");
		  screen = 0;
	  }

	  if (system_mode == KEYPAD_MODE && !is_locked && key) {
				  if (key >= '0' && key <= '9') {
					  if (idx < MAX_PASSWORD_LEN) {
						  entered[idx++] = key;
						  entered[idx] = '\0';
						  sprintf(buffer, "%c", key);
						  uart_print(buffer);
						  pwdEnter(entered);
					  }
					  else {
						  uart_print("Password can be up to 10 characters.\r\n");
						  memset(entered, 0, sizeof(entered));
						  idx = 0;
					  }
				  }
				  else if (key == '#') {
					  entered[idx] = '\0';

					  if (keypad_mode == NORMAL) {
						  if (strcmp(entered, correctPassword) == 0) {
							  ESP_HttpGet("info/password_correct\r\n");
							  uart_print("Password Correct\r\n");
							  isPass = 1;
							  led_show_result(1);
							  screen = 3;
							  pwdEnter("");
						  }
						  else {
							  ESP_HttpGet("warning/password_fail\r\n");
							  uart_print("Password Fail\r\n");
							  isPass = 0;
							  led_show_result(0);
							  screen = 4;
							  pwdEnter("");
							  fail_count++;
							  if (fail_count >= 3) {
								  screen = 5;
								  is_locked = true;
								  lock_start_time = HAL_GetTick();
								  ESP_HttpGet("alert/door_lock\r\n");
								  uart_print("Too many failed attempts! System locked for 10 seconds.\r\n");
							  }
						  }
					  }
					  else if (keypad_mode == VALIDATE_OLD_PASSWORD) {
						  if (strcmp(entered, correctPassword) == 0) {
							uart_print("Old password correct. \r\nEnter new password");
							keypad_mode = CHANGE_NEW_PASSWORD;
							screen = 2;
							pwdEnter("");
						  }
						  else {
							ESP_HttpGet("warning/invalid_password_change\r\n");
							uart_print("\r\nWrong old password.\r\n");
							keypad_mode = NORMAL;
							screen = 4;
							pwdEnter("");
						  }
					  }
					  else if (keypad_mode == CHANGE_NEW_PASSWORD) {
						  strcpy(correctPassword, entered);
						  ESP_HttpGet("info/password_changed\r\n");
						  uart_print("Password changed !!!\r\n");
						  keypad_mode = NORMAL;
						  screen = 1;
						  pwdEnter("");
					  }

					  idx = 0;
					  memset(entered, 0, sizeof(entered));
				  }
				  else if (key == '*') {
					  if (idx > 0) {
						  idx--;
						  entered[idx] = '\0';
						  uart_print("\b \b");
						  pwdEnter(entered);
					  }
				  }
				  else if (key == 'C') {
					  uart_print("Enter old password to change");

					  keypad_mode = VALIDATE_OLD_PASSWORD;
					  screen = 7;
					  idx = 0;
					  memset(entered, 0, sizeof(entered));
				  }
				  else {
					  sprintf(buffer, "Ignored: %c\r\n", key);
					  uart_print(buffer);
				  }
	  }

	  else if(system_mode == RFID_MODE && !is_locked){
			  status = MFRC522_Request(PICC_REQIDL, tagType);

			  if (status != MI_OK) {
				  HAL_Delay(100);
				  continue;
			  }

			  status = MFRC522_Anticoll(uid);
			  if (status != MI_OK) {
				  HAL_Delay(100);
				  continue;
			  }

			  char buffer[64];
			  sprintf(buffer, "UID: %d %d %d %d %d\r\n", uid[0], uid[1], uid[2], uid[3], uid[4]);
			  uart_print(buffer);

			  if (is_uid_allowed(uid)) {
				  ESP_HttpGet("info/rfid_pass\r\n");
				  uart_print("Pass\r\n");
				  isPass = 1;
				  led_show_result(1);
				  screen = 3;
			  }
			  else {
				  ESP_HttpGet("warning/rfid_fail\r\n");
				  uart_print("Fail\r\n");
				  isPass = 0;
				  led_show_result(0);
				  screen = 4;
				  fail_count++;
				  if (fail_count >= 3) {
					  is_locked = true;
					  lock_start_time = HAL_GetTick();
					  ESP_HttpGet("alert/door_lock\r\n");
					  uart_print("Too many failed attempts! System locked for 10 seconds.\r\n");
					  screen = 5;
				  }
			  }
	  }
	  LCD_MULT(entered);
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {

        if (is_locked) {
            // กระพริบ LED หรือเช็กเวลา
            if (HAL_GetTick() - lock_start_time >= 10000) {
                is_locked = false;
                fail_count = 0;
                uart_print("System unlocked. You may try again.\r\n");
                system_mode = SELECT_MODE;
                screen = 0;
            }
        }
    }
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
