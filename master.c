#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
int main(int argc, char** argv)
{
	/*char inputFileOne[15], inputFileTwo[15], inputFileThree[15];
	char *progCMD = (char*) malloc(128 * sizeof(char));
	char inBuff[15];

	printf("Enter location for TestDef.csv:\n");
	fgets(inBuff, 15, stdin);
	sscanf(inBuff, "%s", inputFileOne);

	printf("Enter location for Test.dbc:\n");
	fgets(inBuff, 15, stdin);
	sscanf(inBuff, "%s", inputFileTwo);

	printf("Enter location for CANI config:\n");
	fgets(inBuff, 15, stdin);
	sscanf(inBuff, "%s", inputFileThree);

	strcpy (progCMD, "./TestInterface ");
	strcat (progCMD, inputFileOne);
	system(progCMD);*/

	char keyName[15];
	int val, choice;
	char inBuff[15];
	const char* hostname = "localhost";
	int port = 6379;
	redisContext* c;
	redisReply* reply;

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

	while(1)
	{
		printf("\n1 - Enable logging service\n2 - Exit logging service\n3 - Check read message service\n4 - Exit master (Does not exit slave programs)\n");

		fgets(inBuff, 15, stdin);
		sscanf(inBuff, "%d", &choice);

		if (choice == 1)
		{
			writeToDataStore(c, reply, "log_enable", 1);
		}
		else if (choice == 2)
		{
			writeToDataStore(c, reply, "log_exit", 1);
		}
		else if (choice == 3)
		{
			printf("Read message status: %d\n", atoi(readFromDataStore(c, reply, "readmsg_status")));
		}
		else if (choice == 4)
		{
			exit(1);
		}
		else printf("Invalid input\n");
	}

	return 0;
}
