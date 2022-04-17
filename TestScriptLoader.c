#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <time.h>

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
	//Does setting the test values on a timer cause any rows to be dropped?
	//If so consider a list for values
	int programStatus = 0;

	redisContext* c;
	redisReply* reply;
	const char* hostname = "localhost";
	int port = 6379;
	FILE* csvFile;
	char inBuff[15], line[1024];
	int val, i;
	char* token;
	const char delim[2] = ",";
	char** keys;
	char** row;
	long int time, oldTime;

	//Connect to redis
	printf("Connecting Redis Server\n");
	c = redisConnect(hostname, port);
	if (c == NULL)
	{
		printf("Connection Error!\n");
		exit(1);
	}

	printf("Enter number of columns in csv: ");
	fgets(inBuff, 15, stdin);
	sscanf(inBuff, "%d", &val);

	keys = (char**)malloc(val * sizeof(char) * 100);
	row = (char**)malloc(val * sizeof(char) * 100);

	csvFile = fopen(argv[1], "r");
	fgets(line, 1024, csvFile);
	token = strtok(line, delim);
	for (i = 0; i < val - 1 && token != NULL; i++)
	{
		token = strtok(NULL, delim);
		printf(" %s\n", token);
		strcpy(keys[i], token);
	}

	fgets(line, 1024, csvFile);
	token = strtok(line, delim);
	for (i = 0; i < val - 1 && token != NULL; i++)
	{
		token = strtok(NULL, delim);
		printf(" %s\n", token);
		strcpy(row[i], token);
	}

	time = clock();
	oldTime = 0;
	while (1)
	{
		if (clock() - time >= CLOCKS_PER_SEC * row[0])
		{
			oldTime += row[0];
			for (i = 0; i < val; i++)
			{
				writeToDataStore(c, reply, keys[i], atoi(row[i]));
			}

			fgets(line, 1024, csvFile);
			token = strtok(line, delim);
			for (i = 0; i < val - 1 && token != NULL; i++)
			{
				token = strtok(NULL, delim);
				printf(" %s\n", token);
				strcpy(row[i], token);
			}
			time = clock();
			printf("Time: %f\n", time);
		}
	}
	
	return 1;
}