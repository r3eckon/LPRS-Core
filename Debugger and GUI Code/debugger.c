#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#define DATA_HISTORY_COUNT 5
#define DEVICECOUNT 10

int fd;
int err;
int running;
int queued;

pthread_t inThread;

char * CMD_STOP = "/stop";//Stop debugger process and exit
char * CMD_SEND = "/send";//Send command to device specified by args
char * CMD_WRITE = "/write";//Froce write device data to file
char * CMD_INPUTTEST = "/test";//Simulate input message
char * EMPTY = "";

char *tcmd;
char *targs;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


char * rcmd;
char * rargs;

int avail;
char lastChar;
char serialBuff[1024];
int msgcount=0;
const int serialWait = 1000000;
int serialTime = 0;

const char EOL = '\n';

int readIndex = 0;

char * lastmsg;
int lastmsgSize = 0;


const int SHRWRTERR_NONE = 0;
const int SHRWRTERR_OPENFAILED = 1;
const int SHRWRTERR_LOCKFAILED = 2;
const int SHRWRTERR_UNLOCKFAILED = 3;

char * devicedata[100];

int data_history[DEVICECOUNT][DATA_HISTORY_COUNT];

int devicedatalength[100];

void initDeviceData()
{
	char * cline;
	
	for(int i=0; i < DEVICECOUNT; i++)
	{
		devicedata[i] = malloc(256);
		sprintf(devicedata[i], "Waiting for device #%d", i);
		
		for(int j=0; j < DATA_HISTORY_COUNT; j++)
		{
			data_history[i][j] = 0;
		}
		
	}
	
}

char toRet[8192];//8K buffer should be enough for now

char * createFullDataString()
{
	char * writepointer = toRet;
	
	sprintf(writepointer, "%s\n", devicedata[0]);
	
	for(int i=1; i < DEVICECOUNT; i++)
	{
		//Offset in string by length of last line data + the newline char added
		sprintf(writepointer += strlen(devicedata[i-1]) + 1, "%s\n", devicedata[i]);
	}

	return toRet;
}

char historyString[16];
char lastLine[200];

char * createFullDataStringWithHistory()
{
	char * writepointer = toRet;
	int clength = 0;
	char * hstring = historyString;
	char * lline = lastLine;
	
	//Lengths
	int llf, llh;
	
	for(int i=0; i < DEVICECOUNT; i++)
	{
		
		clength = 0;
		lline = lastLine;
		llh=0;
		
		for(int j=0; j < DATA_HISTORY_COUNT; j++)
		{
			memset(historyString, 0, 16);
			
			sprintf(hstring, "%d", data_history[i][j]);
			
			if(j > 0)
				sprintf(lline += llh + 1, "%s,", hstring);
			else
				sprintf(lline, "%s,", hstring);
			
			llh = strlen(hstring);
		}
		
		if(i > 0)
			sprintf(writepointer += llf + 1, "%s\n", lastLine);
		else
			sprintf(writepointer, "%s\n", lastLine);
			
		llf = strlen(lastLine);

	}

	return toRet;
}

int shared_write(char * file,  char * toWrite)
{
	struct flock l;
	int sfd;
	
	l.l_type = F_WRLCK;
	l.l_whence = SEEK_SET;
	l.l_start = 0;
	l.l_len = 0;
	l.l_pid = getpid();
	
	if((sfd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
		return SHRWRTERR_OPENFAILED;
		
	if(fcntl(sfd, F_SETLK, &l) < 0)
		return SHRWRTERR_LOCKFAILED;
	
	write(sfd, toWrite, strlen(toWrite));	
	
	l.l_type = F_UNLCK;
	if(fcntl(sfd, F_SETLK, &l) < 0)
		return SHRWRTERR_UNLOCKFAILED;
	
	close(sfd);
	
	return SHRWRTERR_NONE;
}


void *inFunction(void *value)
{
	char full[200];
	char cmd[50];
	char *args;
	
	int firstChar;
	
	char *token;
	
	char *start;
	
	while(running)
	{
		if((firstChar = getchar()) == 0x0A)
		{
			continue;
		}
		
		memset(full, 0, 200);
		
		start = &full[1];
		full[0] = firstChar;
		
		fgets(start, 200, stdin);
		
		sscanf(full, "%s", cmd);
		
		token =  strtok(full, " ");
		token = strtok(NULL, " ");
		args = token;
		
		printf("Command : %s\n", cmd);
		printf("Arguments : %s\n", args);
		
		pthread_mutex_lock(&mutex);
		
		tcmd = cmd;
		targs = args;
		queued=1;
		pthread_mutex_unlock(&mutex);
		
		
		
	}
	return NULL;
}

int lastWriteErr=0;

void writeDeviceDataFile()
{
	while((lastWriteErr = shared_write("data.txt", createFullDataString())) != 0)
	{
		printf("Error Writing Shared File : %d\n", lastWriteErr);
	}
}

void writeDeviceDataFileWithHistory()
{
	while((lastWriteErr = shared_write("datahistory.txt", createFullDataStringWithHistory())) != 0)
	{
		printf("Error Writing Shared File : %d\n", lastWriteErr);
	}
}



void ShiftDataHistory(int devid)
{
	int exist=0;
	for(int i=DATA_HISTORY_COUNT - 1; i > 0; i--)
	{
		data_history[devid][i] = data_history[devid][i-1];
	}
}

const int INMODE_UNKNOWN = 0;
const int INMODE_LORA = 1;
const int INMODE_GSM = 2;

int gotData=0;

char msgbuff[1024];

void interpretIncomingData(char * incoming)
{
	char * tk;
	char * dtk;
	int deviceid;//ID of lora device, can include signal repeaters
	int dataid;//ID of received data, mapping to a unique sensor
	int datalength;
	char * data;
	int rssi;
	int snr;
	int mode = INMODE_UNKNOWN;
	char * end;
	long rval;
	
	
	char * isolated;
	char * loramsg;
	
	strncpy(msgbuff, incoming, strlen(incoming));
	
	//Begin by parsing the sender device type
	if(strstr(incoming, "LORA") != NULL)
	{
		mode = INMODE_LORA;
	}
	else if(strstr(incoming, "GSM") != NULL)
	{
		mode = INMODE_GSM;
	}
	
	
	
	
	if(mode == INMODE_LORA)
	{
		tk = strtok(incoming, ":");
		tk = strtok(NULL, ":");
		
		isolated = tk;
		
		for(int i=0; i<strlen(isolated); i++)
		{
			if(isolated[i] == '\n' || isolated[i] == '\r')
			{
				isolated[i] = '\0';
			}
		}
		
		if(strstr(tk, "+RCV") != NULL)//+RCV=ID,LENGTH,MESSAGE*,RSSI,SNR
		{								// * MESSAGE PART = DATAID_DATA
			gotData=0;
			
			tk = strtok(isolated, "=");

			
			tk = loramsg = strtok(NULL, "=");
			
			
			tk = strtok(tk, ",");
			
			
			if(tk != NULL)
			{
				//deviceid = atoi(tk);
				rval = strtol(tk, &end, 0);
				
				if(end == tk)
				{
					printf("String to Number Convert Error\n");
					return;
				}
				
				deviceid = (int)rval;
				
				if(deviceid == 0)
				{
					printf("Lora Address Value Parse Error\n");
					return;
				}
				else
				{
					printf("Lora Address :%d\n", deviceid);
				}
				
				
			}
			else
			{
			    printf("General Parse Error (Lora Address)\n");
			    return;
			}
			
			tk = strtok(NULL, ",");
			
			if(tk != NULL)
			{
				//datalength = atoi(tk);
				rval = strtol(tk, &end, 0);
				
				if(end == tk)
				{
					printf("String to Number Convert Error\n");
					return;
				}
				
				datalength = (int)rval;
				
				printf("Data Length :%d\n", datalength);
			}
			else
			{
				printf("General Parse Error (Data Length)\n");
				return;
			}
			
			tk = strtok(NULL, ",");
			
			if(tk != NULL)
			{
				data = tk;
				printf("Lora Message :%s\n", data);
				
				//Repeater compatible data ID parsing ( DATAID_DATA )
				dtk = strtok(data, "_");
				
				if(dtk == NULL)
				{
					printf("General Parse Error (Sensor ID)\n");
					return;
				}
				
				//dataid = atoi(dtk);
				rval = strtol(dtk, &end, 0);
				
				if(end == dtk)
				{
					printf("String to Number Convert Error\n");
					return;
				}
				
				dataid = (int)rval;
				
				dtk = strtok(NULL,"_");
				
				if(dtk != NULL)
				{
					data = dtk;
				
					printf("Sensor ID :%d\n", dataid);
					printf("Sensor Data :%s\n", data);
				
					gotData = 1;//signal that a write is needed
					strcpy(devicedata[dataid], data);
				
					//New feature for graph drawing, keeps history of last received data
					ShiftDataHistory(dataid);
					//data_history[dataid][0] = atoi(devicedata[dataid]);
					rval = strtol(dtk, &end, 0);
				
					if(end == dtk)
					{
						printf("String to Number Convert Error\n");
						return;
					}
				
					data_history[dataid][0] = (int)rval;
				
					devicedatalength[dataid] = datalength;
				}
				else
				{
					printf("General Parse Error (Sensor Data)\n");
					return;
				}
				
				
			}
			else
			{
				printf("General Parse Error (Main Data)\n");
				return;
			}
				
			//Gotta regrab the full string since strtok fucks shit up
			//Also set pointer back to right place to read RSSI & SNR
			tk = strtok(msgbuff, ",");
			tk = strtok(NULL, ",");
			tk = strtok(NULL, ",");
			tk = strtok(NULL, ",");
			
			if(tk != NULL)
			{
				//rssi = atoi(tk);
				
				rval = strtol(tk, &end, 0);
				
				if(end == tk)
				{
					printf("String to Number Convert Error\n");
					return;
				}
				
				rssi = (int)rval;
				
				printf("RSSI :%d\n", rssi);
			}
			else
			{
				printf("General Parse Error (RSSI)\n");
				printf("String from pointer : %s\n", tk );
				return;
			}
				
			
			tk = strtok(NULL, ",");
			
			if(tk != NULL)
			{
				//snr = atoi(tk);
				
				rval = strtol(tk, &end, 0);
				
				if(end == tk)
				{
					printf("String to Number Convert Error\n");
					return;
				}
				
				snr = (int)rval;
				
				printf("SNR :%d\n", snr);
			}
			else
			{
				printf("General Parse Error (SNR)\n");
				return;
			}
			
			
			if(gotData > 0)
			{
				//writeDeviceDataFile();
				writeDeviceDataFileWithHistory();
			}

			//printf("- Lora Receive Interpretation -\nDevice ID : %d\nLength : %d\nData : %s\nRSSI : %d\nSNR : %d\n", deviceid, datalength, data, rssi, snr);
			
		}
		else
		{
			printf("Isolated LORA data : [%s]\n", isolated);
		}
		
		

	}
	else if(mode == INMODE_GSM)
	{
	
	}
}

void executeCommand(char * cmd, char * args)
{
	//Stop command
	if(strcmp(cmd, CMD_STOP) == 0)
	{
		running =0;
		return;
	}
	
	//Send command
	if(strcmp(cmd, CMD_SEND) == 0)
	{
		serWrite(fd, args, strlen(args));
		return;
	}
	
	//Write command
	if(strcmp(cmd, CMD_WRITE) == 0)
	{
		writeDeviceDataFileWithHistory();
		return;
	}
	
	//Input test command
	if(strcmp(cmd, CMD_INPUTTEST) == 0)
	{
		interpretIncomingData(args);
		return;
	}
	
}


int main(int argc, char **argv)
{
	running=1;
	printf("Starting...\n");
	
	
	if((err = gpioInitialise()) < 0){
		printf("Init failed - Error Code %d\n", err);
		return 1;
	}
	
	if((fd = serOpen("/dev/ttyAMA0", 9600, 0) < 0))
	{
		printf("Unable to open serial port - Error Code %d \n", err);
		return 1;
	}
		
	
	printf("Serial port open. Starting threads...\n");
	
	
	pthread_create(&inThread, NULL, inFunction, NULL);
	
	printf("Started\n");
	
	initDeviceData();
	
	while(running)
	{
		//Lock on threaded vars to acquire & execute command
		if(queued == 1)
		{
			pthread_mutex_lock(&mutex);
			rcmd = tcmd;
			rargs = targs;
			queued=0;
			pthread_mutex_unlock(&mutex);
			executeCommand(rcmd, rargs);
		}
		
		if((avail = serDataAvailable(fd)) > 0)
		{
			lastmsgSize+= avail;	
			for(int i=0; i < avail; i++)
			{
				lastChar = serReadByte(fd);
				serialBuff[readIndex] = lastChar;
				readIndex++;
				printf("%c",lastChar);
			}
			
			if(lastChar == EOL)//Encountered END OF LINE char ~
			{
				readIndex=0;
				
				printf("\n");
				printf("[%d]Received %d bytes\n", msgcount, lastmsgSize);
				
				msgcount+=1;
				lastmsg = serialBuff;
				lastmsgSize = 0;
				
				interpretIncomingData(lastmsg);
			}
			
			
		}	
	}
	
	printf("Stopped by user. Exiting.");
	
	return 0;
}



