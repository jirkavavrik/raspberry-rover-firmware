#include <wiringPi.h>
#include <wiringSerial.h>
#include <softPwm.h>
#include <ads1115.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>


#define FW true //dopředu
#define BW false //dozadu

unsigned char speednow = 50;

int client_socket;

int adc0input;
float batteryVoltage;
FILE *voltageFile;//

void motorRight(unsigned char value, bool dir = FW) {
	if(dir == FW)
		digitalWrite(23, LOW);
	else
		digitalWrite(23, HIGH);
    
	if(dir == FW) {
		if(value == 0)
			softPwmWrite(24, 0);
		else      
			softPwmWrite(24, value); 
	}
	else
		softPwmWrite(24, (100 - value)); //reverse value so 0 stays min and 100 stays max
}

void motorLeft(unsigned char value, bool dir = FW) {
	if(dir == FW)
		digitalWrite(21, LOW);
	else
		digitalWrite(21, HIGH);

	if(dir == FW) {
		if(value == 0)
			softPwmWrite(22, 0);
		else      
			softPwmWrite(22, value*0.9);
	}
	else
		softPwmWrite(22, 100 - value*0.9); //reverse value so 0 stays min and 100 stays max
}

void STOP() {
	motorLeft(0, FW);
	motorRight(0, FW);
}

void turnRight(unsigned char pwm) {
	motorRight(0, FW);
	motorLeft(pwm, FW); 
}

void turnLeft(unsigned char pwm) {
	motorLeft(0, FW);
	motorRight(pwm, FW); 
}

void goForward(unsigned char pwm) {
	motorLeft(pwm, FW);
	motorRight(pwm, FW);
}

void goBack (unsigned char pwm) {
	digitalWrite(21, HIGH);//left
	softPwmWrite(22, 100-pwm*0.9);
	digitalWrite(23, HIGH);//right
	softPwmWrite(24, 100-pwm);
}

void fw(unsigned char l, unsigned char r) {
	motorLeft(l, FW);
	motorRight(r, FW);
}

void bw(unsigned char l, unsigned char r){
	motorLeft(l, BW);
	motorRight(r, BW);
}

void shutdown() {
	STOP();
	close(client_socket);
	printf("client socket closed.\n");
	system("sudo shutdown -h now &");
}

int main(int argc, char *argv[]) {
	wiringPiSetup();

	ads1115Setup (120, 0x48);
	digitalWrite(120,ADS1115_GAIN_4);
	
	//H-bridge pins
	pinMode(21, OUTPUT);
	pinMode(22, OUTPUT);
	pinMode(23, OUTPUT);
	pinMode(24, OUTPUT);
	softPwmCreate (22, 0, 100);
	softPwmCreate (24, 0, 100);
	
	//shutdown button
	pinMode(29, INPUT);
	pullUpDnControl (29, PUD_UP);
	wiringPiISR (29, INT_EDGE_FALLING,  shutdown) ;
	
	int server_socket, client_socket, portno;
	int disconnect = 0;
	char message;
	struct sockaddr_in server_address;

	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
     }
     
	//parse port number
	portno = atoi(argv[1]);
	
	//create socket, if unsuccessful, print error message
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) 
		fprintf(stderr, "ERROR opening socket\n");
	
	//assign server address
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(portno);
	
	if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
		fprintf(stderr, "ERROR on binding");

	listen(server_socket,5);
	while(true) {//network loop
		disconnect = 0;
		client_socket = accept(server_socket, NULL, NULL);
		if (client_socket < 0) 
			fprintf(stderr, "ERROR on accept");
	
		while(true) {//control loop
			if(recv(client_socket, &message, sizeof(message),MSG_DONTWAIT) != -1) {
				printf("message: %c\n", message);
				switch(message){
					case 'X': system("python3 /home/pi/raspberry-rover-firmware/src/webcamera.py &"); break;
					case 'x': system("pkill -15 -f ""webcamera.py"""); break;
					
					case '0': disconnect = 1; break;
					
					case 'F': 
						goForward(speednow); break;
      
					case 'G':
						fw(speednow/2, speednow); break;
		
					case 'I':
						fw(speednow, speednow/2); break;
		
					case 'S':
						STOP(); break;
      
					case 'L': 
						turnLeft(speednow);  break;
      
					case 'R': 
						turnRight(speednow);  break;
      
					case 'B': 
						goBack(speednow); break;
      
					case 'H':
						bw(speednow/2, speednow); break;
				
					case 'J':
						bw(speednow, speednow/2);	break;
						
					case '+':
						if(speednow <=95)
							speednow += 5;
						break;
					case'-':
						if(speednow >=5)
							speednow -= 5;
						break;
				}//end of switch
				if(disconnect == 1) {
					printf("disconnecting now...\n");break;
				}			
			}//end of if recv

			adc0input = analogRead(120); //read the ads1115
			batteryVoltage = adc0input/1600.0f; //convert the value to Volts ( /8 to convert the value to mV , /1000 to convert it to Volts and *5 to get value in front of the voltage divider)
			voltageFile  = fopen("/var/ramdrive/v.txt", "w");
			fprintf(voltageFile, "%f", batteryVoltage);
			fclose(voltageFile);

			char voltageString[10];
			int n = sprintf(voltageString, "%f", batteryVoltage);
			send(client_socket, voltageString, n, 0);
			delay(100);	
		}//end of control loop
		close(client_socket);
		printf("client socket closed.\n");
	}//end of network loop
	return 0;
}
