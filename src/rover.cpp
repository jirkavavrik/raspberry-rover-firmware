#include <wiringPi.h>
#include <wiringSerial.h>
#include <wiringPiI2C.h>
#include <ads1115.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

int speed = 1000;

int client_socket;
int disconnect = 0;

int adc0input;
float batteryVoltage;
FILE *voltageFile;

int fd;
#define PCA9685_MODE1 0x0
#define PCA9685_PRESCALE 0xFE
#define LED0_ON_L 0x6
#define LEDALL_ON_L 0xFA
#define PIN_ALL 16

enum {
RED,
GREEN,
BLUE
};

void pca9685PWMFreq(int fd, float freq) {
	// Cap at min and max
	freq = (freq > 1000 ? 1000 : (freq < 40 ? 40 : freq));

	//http://www.nxp.com/documents/data_sheet/PCA9685.pdf Page 24
	int prescale = (int)(25000000.0f / (4096 * freq) - 0.5f);

	int settings = wiringPiI2CReadReg8(fd, PCA9685_MODE1) & 0x7F;
	int sleep	= settings | 0x10;// Set sleep bit to 1
	int wake 	= settings & 0xEF;// Set sleep bit to 0
	int restart = wake | 0x80;// Set restart bit to 1

	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, sleep);
	wiringPiI2CWriteReg8(fd, PCA9685_PRESCALE, prescale);
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, wake);
	delay(1);
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, restart);
}

void pca9685PWMWrite(int fd, int pin, int on, int off) {
	int reg = (pin >= PIN_ALL ? LEDALL_ON_L : LED0_ON_L + 4 * pin);
	// Write to on and off registers and mask the 12 lowest bits of data to overwrite full-on and off
	wiringPiI2CWriteReg16(fd, reg	 , on  & 0x0FFF);
	wiringPiI2CWriteReg16(fd, reg + 2, off & 0x0FFF);
}

void shutdown() {
	digitalWrite(21, LOW); pca9685PWMWrite(fd, 0, 0, 0);
	digitalWrite(23, LOW); pca9685PWMWrite(fd, 1, 0, 0);
	close(client_socket);
	printf("client socket closed.\n");
	system("sudo shutdown -h now &");
}

void statusLED(int colour, int intensity) {
	switch(colour){
		case RED:
			pca9685PWMWrite(fd, 4, 0, intensity);
			pca9685PWMWrite(fd, 5, 0, 0);
			pca9685PWMWrite(fd, 6, 0, 0);
			break;
		case GREEN:
			pca9685PWMWrite(fd, 4, 0, 0);
			pca9685PWMWrite(fd, 5, 0, intensity);
			pca9685PWMWrite(fd, 6, 0, 0);
			break;
		case BLUE:
			pca9685PWMWrite(fd, 4, 0, 0);
			pca9685PWMWrite(fd, 5, 0, 0);
			pca9685PWMWrite(fd, 6, 0, intensity);
			break;
	}
}

int inline evaluateMessage(char m) {
	//print message to stdout
	printf("message: %c\n", m);
				switch(m){
					case 'X': system("sudo -u pi python3 /home/pi/raspberry-rover-firmware/src/webcamera.py &"); break;
					case 'x': system("pkill -15 -f ""webcamera.py"""); break;

					case '0': disconnect = 1; break;

					case 'a':
						pca9685PWMWrite(fd, 2, 0, 0);
						pca9685PWMWrite(fd, 3, 0, 0);
						break;

					case 'b':
						pca9685PWMWrite(fd, 2, 0, 100);
						pca9685PWMWrite(fd, 3, 0, 100);
						break;

					case 'c':
						pca9685PWMWrite(fd, 2, 0, 2400);
						pca9685PWMWrite(fd, 3, 0, 2400);
						break;

					case 'F':
						digitalWrite(21, LOW); pca9685PWMWrite(fd, 0, 0, speed);
						digitalWrite(23, LOW); pca9685PWMWrite(fd, 1, 0, speed);
						//goForward();
						break;

					case 'S':
						digitalWrite(21, LOW); pca9685PWMWrite(fd, 0, 0, 0);
						digitalWrite(23, LOW); pca9685PWMWrite(fd, 1, 0, 0);
						//STOP();
						break;

					case 'L':
						digitalWrite(21, LOW); pca9685PWMWrite(fd, 0, 0, 0);
						digitalWrite(23, LOW); pca9685PWMWrite(fd, 1, 0, speed);
						//turnLeft();
						break;

					case 'R':
						digitalWrite(21, LOW); pca9685PWMWrite(fd, 0, 0, speed);
						digitalWrite(23, LOW); pca9685PWMWrite(fd, 1, 0, 0);
						//turnRight();
						break;

					case 'B':
						digitalWrite(21, HIGH); pca9685PWMWrite(fd, 0, 0, 4095 - speed);
						digitalWrite(23, HIGH); pca9685PWMWrite(fd, 1, 0, 4095 - speed);
						//goBack();
						break;

					case '+':
						if(speed <=3995)
							speed += 100;
						break;
					case'-':
						if(speed >=100)
							speed -= 100;
						break;
				}//end of switch
				if(disconnect == 1) {
					statusLED(GREEN, 512);
					printf("disconnecting now...\n");return -1;
				}
	return 0;
}

int main(int argc, char *argv[]) {
	wiringPiSetup();

	//setup ads1115
	ads1115Setup (120, 0x48);
	digitalWrite(120,ADS1115_GAIN_4);

	//setup pca9685
	fd = wiringPiI2CSetup(0x40);
	if (fd < 0)	{
		printf("Error in i2c setup\n");
		return fd;
	}
	int settings = wiringPiI2CReadReg8(fd, PCA9685_MODE1) & 0x7F;
	int autoInc = settings | 0x20;
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, autoInc);
	pca9685PWMFreq(fd, 500);

	//daytime lights on
	pca9685PWMWrite(fd, 2, 0, 100);
	pca9685PWMWrite(fd, 3, 0, 100);

	//H-bridge pins
	pinMode(21, OUTPUT);
	pinMode(23, OUTPUT);

	//shutdown button
	pinMode(29, INPUT);
	pullUpDnControl (29, PUD_UP);
	wiringPiISR (29, INT_EDGE_FALLING,  shutdown) ;

	char message;
	int server_socket, client_socket, portno;
	struct sockaddr_in server_address;

	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		statusLED(RED, 512);
		exit(EXIT_FAILURE);
     }

	//parse port number
	portno = atoi(argv[1]);

	//create socket, if unsuccessful, print error message
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)  {
		fprintf(stderr, "ERROR opening socket\n");
		statusLED(RED, 512);
		exit(EXIT_FAILURE);
	}

	//assign server address
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(portno);

	if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "ERROR on binding");
		statusLED(RED, 512);
		exit(EXIT_FAILURE);
	}

	statusLED(GREEN, 512);

	listen(server_socket,5);
	while(true) {//network loop
		disconnect = 0;
		client_socket = accept(server_socket, NULL, NULL);
		statusLED(BLUE, 512);
		if (client_socket < 0) {
			fprintf(stderr, "ERROR on accept");
			statusLED(RED, 512);
		}

		while(true) {//control loop
			if(recv(client_socket, &message, sizeof(message),MSG_DONTWAIT) != -1) {
				//send the message back to client
				send(client_socket, &message, sizeof(message), 0);
				if(evaluateMessage(message) < 0)
					break;
			}

			adc0input = analogRead(120); //read the ads1115
			batteryVoltage = adc0input/1600.0f; //convert the value to Volts ( /8 to convert the value to mV , /1000 to convert it to Volts and *5 to get value in front of the voltage divider)
			voltageFile  = fopen("/var/ramdrive/v.txt", "w");
			fprintf(voltageFile, "%.2f", batteryVoltage);
			fclose(voltageFile);

			//char voltageString[10];
			//int n = sprintf(voltageString, "%.2f", batteryVoltage);
			//send(client_socket, voltageString, n, 0);
			delay(100);
		}//end of control loop
		close(client_socket);
		printf("client socket closed.\n");
	}//end of network loop
	return 0;
}
