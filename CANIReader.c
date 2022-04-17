#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <canlib.h>
#include <kvaDbLib.h>
#include <time.h>

//TODO - Take function and struct defs that are common across scripts and put in a header file for ease of editing
//Do I even need to store signals or does the db handle all of that
struct Sig
{
	char* name;
	int startBit;
	int len;
	double scale;
	double offset;
	double defaultVal;
};
struct Msg
{
	char* name;
	unsigned int id;
	unsigned int flags;
	unsigned int dlc;
	unsigned char* data;
	int numSigs;
	struct Sig* signals;
	unsigned long timeStamp;
};

void checkStatus(int status, char* str, redisContext* c, redisReply* reply, int idling)
{
	//TODO - Add more in depth error feedback here based on canStatus enum codes
	if (status != 0)
	{
		printf("Error in %s: %d\n", str, status);
		writeToDataStore(c, reply, "readmsg_status", 3);
		writeToDataStore(c, reply, "readmsg_status_code", status);
		exit(1);
	}
	else
	{
		writeToDataStore(c, reply, "readmsg_status", idling);
		writeToDataStore(c, reply, "readmsg_status_code", status);
	}
}
void printMessages(struct Msg* reads, int numReads);
void writeToDataStore(redisContext* c, redisReply* reply, char* key, double val)
{
	reply = redisCommand(c, "SET %s %f", key, val);
	freeReplyObject(reply);
}
char* readFromDataStore(redisContext* c, redisReply* reply, char* key)
{
	reply = redisCommand(c, "GET %s", key);
	//printf("Value obtained for key %s: %s\n", keyName, reply->str);
	strcpy(val, reply->str);
	freeReplyObject(reply);
}
void readFromBus(redisContext* c, redisReply* reply, canHandle hnd, int numReads, struct Msg* reads, KvaDbHnd dbHnd)
{
	//Loop through all msgs, read msg, loop through msg signals, call decode function, upload to data store
	long int time = clock();
	//TODO - Make read interval a variable timestep
	double readInterval = 0.01;
	int i, j;
	canStatus status;
	unsigned long timeStamp;
	char inBuff[15];
	KvaDbMessageHnd mh = 0;
	KvaDbSignalHnd sh = 0;
	double val = 0;
	long id;
	char *keyName = (char*) malloc(450 * sizeof(char));
	char *sigName = (char*) malloc(450 * sizeof(char));
	int exitSignal = 0;

	while (1 == 1)
	{
		if (clock() - time >= CLOCKS_PER_SEC * readInterval)
		{
			if (atoi(readFromDataStore(c, reply, "readmsg_exit")) == 1) exit(1);
			for (i = 0; i < numReads; i++)
			{
				//if (reads[i].id == 1024 || reads[i].id == 1029) continue;
				//printf("%s %u:\n", reads[i].name, reads[i].id);
				status = canReadSpecific(hnd, reads[i].id, reads[i].data, &reads[i].dlc, &reads[i].flags, &reads[i].timeStamp);
				//printf("Stat: %d\n", status);
				if (status != -2)
				{
					checkStatus(status, "reading message", c, reply, 2);
					//printf("%s Time: %lf\n", reads[i].data, reads[i].timeStamp);
					//decodeMessage(reads[i], c, reply);

					strcpy(keyName, reads[i].name);
					strcat(keyName, "_TimeStamp_Read");
					writeToDataStore(c, reply, keyName, reads[i].timeStamp);

					status = kvaDbGetMsgById(dbHnd, reads[i].id, &mh);
					checkStatus(status, "get msg", c, reply, 2);
					//printf("Status %d %s\n", status, reads[i].name);
					status = kvaDbGetFirstSignal(mh, &sh);
					checkStatus(status, "get first signal", c, reply, 2);

					while (status == kvaDbOK)
					{
						status = kvaDbGetSignalValueFloat(sh, &val, reads[i].data, sizeof(reads[i].data));
						checkStatus(status, "get float data", c, reply, 2);
						strcpy(keyName, reads[i].name);
						strcat(keyName, "_");
						status = kvaDbGetSignalName(sh, sigName, 450);
						strcat(keyName, sigName);
						strcat(keyName, "_Read");
						//printf("%s\n", keyName);
						writeToDataStore(c, reply, keyName, val);
						status = kvaDbGetNextSignal(mh, &sh);
					}
				}
			}
			time = clock();
			break;
		}
		else if (clock() - time >= CLOCKS_PER_SEC * 3)
		{
			printf("Reading...\n");
			time = clock();
		}
	}
}
int main(int argc, char** argv)
{
	//TODO 12/13/2021
	//Implement reading of defFile and dbcFile names from arguments and redis store if no args given
	//Rework checkStatus function to call writeToDataStore automatically when error code given;
	//Add readmsg_status_code key to command dictionary for more in depth error tracing

	int choice, numReads = -1, i, chanNum, programStatus = 0;
	char inBuff[15];
	const char* hostname = "localhost";
	int port = 6379;
	char* dbcFileName = "ReadMsgsBackup.dbc";
	//First array is msg name, second is signal name
	//For file parsing replace with malloc arrays
	//Each signal gets a portion of the bytes allocated to each message
	struct Msg* reads;

	redisContext* c;
	redisReply* reply;

	canHandle hnd;
	canStatus status;

	KvaDbHnd dbHnd;
	KvaDbStatus dbStatus;
	KvaDbMessageHnd mh = 0;
	KvaDbSignalHnd sh = 0;

	checkStatus(0, "Beggining Initialization", c, reply, c, reply, programStatus);
	programStatus = 4;

	//Initialize Redis connection
	printf("Connecting Redis Server\n");
	c = redisConnect(hostname, port);
	if (c == NULL)
	{
		printf("Connection Error!\n");
		writeToDataStore(c, reply, "readmsg_status", 3);
		exit(1);
	}
	reply = redisCommand(c, "PING");
	printf("Ping: %s\n", reply->str);
	freeReplyObject(reply);
	
	//Read dbc file
	printf("Reading: %s\n",dbcFileName); 
	status = kvaDbOpen(&dbHnd);
	checkStatus(status, "opening db", c, reply, c, reply, programStatus);
	status = kvaDbReadFile(dbHnd, dbcFileName);
	checkStatus(status, "reading db", c, reply, c, reply, programStatus);

	canInitializeLibrary();
	hnd = canOpenChannel(0, 0);
	status = canGetNumberOfChannels(&chanNum);
	status = canSetBusParams(hnd, canBITRATE_250K, 0, 0, 0, 0, 0);
	checkStatus(status, "bitrate", c, reply, c, reply, programStatus);
	printf("Channels: %d\n", chanNum);
	status = canBusOn(hnd);

	if (hnd < 0)
	{
		char msg[64];
		canGetErrorText((canStatus)hnd, msg, sizeof(msg));
		fprintf(stderr, "canOpenChannel failed %s\n", msg);
		exit(1);
	}

	if (numReads == -1)
	{
		numReads = 10;
		reads = (struct Msg*)malloc(numReads * sizeof(struct Msg));
		numReads = 0;
	}

	status = kvaDbGetFirstMsg(dbHnd, &mh);
	checkStatus(status, "loading first message", c, reply, c, reply, programStatus);
	while (status == kvaDbOK)
	{
		int numSigs;
		char inBuff[15];
		struct Msg message;

		message.name = (char*)malloc(200 * sizeof(char));
		message.data = (unsigned char*)malloc(8 * sizeof(unsigned char));
		message.id = 0;

		status = kvaDbGetMsgName(mh, message.name, 200);//sizeof(message.name));
		checkStatus(status, "obtaining message name", c, reply, c, reply, programStatus);
		message.numSigs = 0;
		
		numSigs = 20;
		message.numSigs = numSigs;
		message.signals = (struct Sig*)malloc(numSigs * sizeof(struct Sig));

		reads[numReads] = message;

		status = kvaDbGetMsgId(mh, &reads[numReads].id, &reads[numReads].flags);
		checkStatus(status, "getting msg id", c, reply, c, reply, programStatus);
		printf("Opening %s at %u\n", reads[numReads].name, reads[numReads].id);
		status = kvaDbGetFirstSignal(mh, &sh);
		checkStatus(status, "loading first signal", c, reply, c, reply, programStatus);
		
		i = 0;
		while (status == kvaDbOK)
		{
			reads[numReads].signals[i].name = (char*)malloc(200 * sizeof(char));
			reads[numReads].signals[i].defaultVal = 0;
			status = kvaDbGetSignalName(sh, reads[numReads].signals[i].name, 200);
			checkStatus(status, "getting signal name", c, reply, c, reply, programStatus);
			status = kvaDbGetSignalValueSize(sh, &reads[numReads].signals[i].startBit, &reads[numReads].signals[i].len);
			checkStatus(status, "getting signal start/size", c, reply, c, reply, 1);
			status = kvaDbGetSignalValueScaling(sh, &reads[numReads].signals[i].scale, &reads[numReads].signals[i].offset);
			checkStatus(status, "getting signal scale", c, reply, c, reply, programStatus);

			status = kvaDbGetNextSignal(mh, &sh);
			if (status != -5) checkStatus(status, "getting next signal", c, reply, c, reply, programStatus);
			i++;
		}
		reads[numReads].numSigs = i;
		status = kvaDbGetNextMsg(dbHnd, &mh);
		if (status != -4) checkStatus(status, "getting next signal", c, reply, c, reply, programStatus);

		numReads++;
	}
	programStatus = 2;

	while (1 == 1)
	{
		//printf("\n1 - Start reading CAN bus\n2 - Print messages loaded from dbc\n3 - Flush datastore\n4 - Exit\n");
		
		/*fgets(inBuff, 15, stdin);
		sscanf(inBuff, "%d", &choice);

		if (choice == 1)
		{
			readFromBus(c, reply, hnd, numReads, reads, dbHnd);
		}
		else if (choice == 2)
		{
			printMessages(reads, numReads);
		}
		else if (choice == 3)
		{
			reply = redisCommand(c, "FLUSHALL");
			printf("Data store flushed\n");
			freeReplyObject(reply);
		}
		else if (choice == 4)
		{
			exit(1);
		}
		else printf("Invalid input\n");*/

		printf("Waiting for start signal...\n");
		sleep(1);

		if (atoi(readFromDataStore(c, reply, "readmsg_onbus")) == 1)
		{
			readFromBus(c, reply, hnd, numReads, reads, dbHnd);
		}
	}

	redisFree(c);

	return 0;
}

//Code likely depreciated
void printMessages(struct Msg* reads, int numReads)
{
	int i, j;
	printf("\nReads:\n");
	for (i = 0; i < numReads; i++)
	{
		printf("Message: %s, ID: %u\n", reads[i].name, reads[i].id);
		for (j = 0; j < reads[i].numSigs; j++)
		{
			printf("	Signal: %s,", reads[i].signals[j].name);
			printf(" Start: %d,", reads[i].signals[j].startBit);
			printf(" Length: %d\n", reads[i].signals[j].len);
		}
	}
}