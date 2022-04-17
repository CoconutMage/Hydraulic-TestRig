#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <canlib.h>
#include <kvaDbLib.h>
#include <time.h>

struct Sig
{
	char* name;
	int startBit;
	int len;
	double scale;
	double offset;
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

void printMessages(struct Msg* reads, int numReads)
{
	int i, j;
	printf("\nReads:\n");
	for (i = 0; i < numReads; i++)
	{
		printf("Message: %s, ID: %d", reads[i].name, reads[i].id);
		for (j = 0; j < reads[i].numSigs; j++)
		{
			printf("\n	Signal: %s, Start: %d, Length: %d", reads[i].signals[j].name, reads[i].signals[j].startBit, reads[i].signals[j].len);
		}
	}
}
void checkStatus(int status, char* str)
{
	if (status != 0)
	{
		printf("Error in %s: %d\n", str, status);
		exit(1);
	}
}
char* readFromDataStore(redisContext* c, redisReply* reply, char* key)
{
	reply = redisCommand(c, "GET %s", key);
	//printf("Value obtained for key %s: %s\n", keyName, reply->str);
	strcpy(val, reply->str);
	freeReplyObject(reply);
}
int main()
{
	int choice, numReads = -1, i, testLength, j, logging = 0;
	char inBuff[15];
	const char* hostname = "localhost";
	int port = 6379;
	char *dbcFileName = "ReadMsgsBackup.dbc";
	//First array is msg name, second is signal name
	//For file parsing replace with malloc arrays
	//Each signal gets a portion of the bytes allocated to each message
	struct Msg* reads;
	struct Msg* writes;
	long int time;
	//TODO - Make read interval a variable timestep
	double value, readInterval = 0.01;
	char* name;
	FILE* fpt;

	redisContext* c;
	redisReply* reply;

	canHandle hnd;
	canStatus status;

	KvaDbHnd dbHnd;
	KvaDbStatus dbStatus;
	KvaDbMessageHnd mh = 0;
	KvaDbSignalHnd sh = 0;

	/*printf("Please enter the name of the dbc file\n");
	fgets(inBuff, 15, stdin);
	sscanf(inBuff, "%s", dbcFileName);*/

	status = kvaDbOpen(&dbHnd);
	checkStatus(status, "opening db");
	status = kvaDbReadFile(dbHnd, dbcFileName);
	checkStatus(status, "reading db");
	printf("DBC File read\n");

	//Initialize Redis connection
	c = redisConnect(hostname, port);
	if (c == NULL)
	{
		printf("Connection Error!\n");
		exit(1);
	}
	reply = redisCommand(c, "PING");
	printf("Ping: %s\n", reply->str);
	freeReplyObject(reply);

	fpt = fopen("Output.csv", "w+");

	if (numReads == -1)
	{
		/*printf("How many read messages will there be total\n");
		fgets(inBuff, 15, stdin);
		sscanf(inBuff, "%d", &numReads);*/
		numReads = 10;
		reads = (struct Msg*)malloc(numReads * sizeof(struct Msg));
		numReads = 0;
	}

	status = kvaDbGetFirstMsg(dbHnd, &mh);
	checkStatus(status, "loading first message");
	//printf("Loading Messages\n");
	while (status == kvaDbOK)
	{
		int numSigs;
		char inBuff[15];
		struct Msg message;

		message.name = (char*)malloc(200 * sizeof(char));
		message.data = (unsigned char*)malloc(8 * sizeof(unsigned char));
		message.id = 0;

		status = kvaDbGetMsgName(mh, message.name, 200);//sizeof(message.name));
		checkStatus(status, "obtaining message name");
		message.numSigs = 0;

		//printf("Enter how many signals the %d message carries: ", (numReads + 1));
		/*fgets(inBuff, 15, stdin);
		sscanf(inBuff, "%d", &numSigs);*/
		numSigs = 20;
		message.numSigs = numSigs;
		message.signals = (struct Sig*)malloc(numSigs * sizeof(struct Sig));

		reads[numReads] = message;

		status = kvaDbGetMsgId(mh, &reads[numReads].id, &reads[numReads].flags);
		checkStatus(status, "getting msg id");
		//printf("Here2\n");
		status = kvaDbGetFirstSignal(mh, &sh);
		checkStatus(status, "loading first signal");

		i = 0;
		while (status == kvaDbOK)
		{
			reads[numReads].signals[i].name = (char*)malloc(200 * sizeof(char));
			status = kvaDbGetSignalName(sh, reads[numReads].signals[i].name, 200);
			checkStatus(status, "getting signal name");
			status = kvaDbGetSignalValueSize(sh, &reads[numReads].signals[i].startBit, &reads[numReads].signals[i].len);
			checkStatus(status, "getting signal start/size");
			status = kvaDbGetSignalValueScaling(sh, &reads[numReads].signals[i].scale, &reads[numReads].signals[i].offset);
			checkStatus(status, "getting signal scale");
			//printf("Here:\n");
			status = kvaDbGetNextSignal(mh, &sh);
			if (status != -5) checkStatus(status, "getting next signal");
			i++;
		}
		//printf("\nGetting Next MSG: %d\n", numReads);
		reads[numReads].numSigs = i;
		status = kvaDbGetNextMsg(dbHnd, &mh);
		//printf("STATUS: %d\n", status);
		if (status != -4)checkStatus(status, "getting next signal");

		numReads++;
	}

	//Set up headers on output csv file
	name = (char*)malloc(450 * sizeof(char));
	for (i = 0; i < numReads; i++)
	{
		//printf("HAppening\n");
		strcpy(name, reads[i].name);
		strcat(name, "_TimeStamp");

		fprintf(fpt, "%s,", name);

		for (j = 0; j < reads[i].numSigs; j++)
		{
			strcpy(name, reads[i].name);
			strcat(name, "_");
			strcat(name, reads[i].signals[j].name);

			fprintf(fpt, "%s,", name);
		}
		fprintf(fpt, " ,");
	}
	fprintf(fpt, "\n");

	time = clock();
	//Validate reply
	/*reply = redisCommand(c, "GET %s", "Test_Length");
	if (reply->str == NULL) testLength = 10;
	else testLength = atoi(reply->str);
	freeReplyObject(reply);*/
	while (1)
	{
		if (clock() - time >= CLOCKS_PER_SEC * readInterval)
		{
			logging = atoi(readFromDataStore(c, reply, "log_enable"));
			if (logging == 1)
			{
				for (i = 0; i < numReads; i++)
				{
					strcpy(name, reads[i].name);
					strcat(name, "_TimeStamp_Read");
					printf("Name: %s\n", name);
					reply = redisCommand(c, "RPOP %s", name);
					if (reply->str != NULL) value = atoi(reply->str);
					else continue;
					freeReplyObject(reply);
					printf("Message: %s, Time: %f\n", reads[i].name, value);
					fprintf(fpt, "%f,", value);

					for (j = 0; j < reads[i].numSigs; j++)
					{
						strcpy(name, reads[i].name);
						strcat(name, "_");
						strcat(name, reads[i].signals[j].name);
						strcat(name, "_Read");

						reply = redisCommand(c, "RPOP %s", name);
						if (reply->str != NULL) value = atoi(reply->str);
						else continue;
						printf("	Signal: %s, Value: %f\n", reads[i].signals[j].name, value);
						freeReplyObject(reply);
						fprintf(fpt, "%f,", value);
					}
					fprintf(fpt, " ,");
				}
				fprintf(fpt, "\n");
				time = clock();

				if (atoi(readFromDataStore(c, reply, "log_exit")) == 1)
				{
					printf("DONE!\n");
					fclose(fpt);
					break;
				}
			}
		}
	}
}
