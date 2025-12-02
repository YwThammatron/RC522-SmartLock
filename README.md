# RC522 Smart Digital Lock (5 Participants)

Tools and Languages

- STM32CubeIDE
- C
- STMicroelectronics Nucleo F767Zi board
- RC522 RFID module
- Adafruit Switch Numpad 12 keys module
- STM32 TFT Display 2.8" module
- HC-SR04 Ultrasonic module
- SG90 Servo module
- ESP32 Wifi module

**Main Requirement**
- User can unlock project via input password or use RFID token
- User can change password by input old password (or default password in first time)
- User can view logs about all action this project have done after reset 
- Project will lock itself when it is unlocked but its door still not open
- Project will lock itself and switch to safe mode when user cannot unlock project 3 time
- Project will notified user when it is unlocked but its door opened for long time

Responsibility
- Work in scope of door movement which use servo and ultrasonic module
- Test and connect servo and ultrasonic hardware individually
- Implement coding in door movement software related to project states
- Create project state about is door close and is credential pass which is essential core of software section
- Test and adjust project when assemble all individual modules together
- Test project with all possible scenarios

Remarks
- This project actually developed in STM32CubeIDE application so participants didn't use git
- View project -> Core -> Src --> main.c to watch mostly source code in software section
