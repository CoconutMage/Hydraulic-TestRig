#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <canlib.h>
#include <kvaDbLib.h>
#include <time.h>
#include <yaml.h>

struct Sig
{
	char* name;
	int startBit;
	int len;
	double scale;
	double offset;
	double defaultVal;
	char* redisKey;
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
/*void readDBC(char* db_name);
void printDB(KvaDbHnd dbHnd);
void setUpChannel(canHandle* hnd, int chan = 0, int openFlags = 4, int bitRate = -2, unsigned int bitRateFlags = 4)
{
	hnd = canOpenChannel(chan, flags);
	canSetBusOutputControl(hnd, bitRateFlags);
	canSetBitrate(hnd, bitRate);
}
void closeChannel(canHandle* hnd)
{
	hnd.busOff();
	hnd.close();
}
void sleep(int nextTime);*/
void checkStatus(int status, char* str, redisContext* c, redisReply* reply, int idling)
{
	//TODO - Add more in depth error feedback here based on canStatus enum codes
	if (status != 0)
	{
		printf("Error in %s: %d\n", str, status);
		writeToDataStore(c, reply, "writemsg_status", 3);
		writeToDataStore(c, reply, "writemsg_status_code", status);
		exit(1);
	}
	else
	{
		writeToDataStore(c, reply, "writemsg_status", idling);
		writeToDataStore(c, reply, "writemsg_status_code", status);
	}
}
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
canStatus sendMsgToCan(canHandle hnd, Msg message)
{
	return canWrite(hnd, message.id, message.data, message.dlc, message.flags);
}
int main(int argc, char** argv)
{
	int programStatus = 0;

	redisContext* c;
	redisReply* reply;
	const char* hostname = "localhost";
	int port = 6379, i, j, numReads = -1;
	yaml_token_t  token;
	FILE* yfh;
	yaml_parser_t yamPars;
	struct Msg* messages;
	double signalVal;
	char* ptr;
	char* dbcFileName = "../../Dbcs/SendMsgs.dbc";

	canHandle hnd;
	canStatus status;

	KvaDbHnd dbHnd;
	KvaDbStatus dbStatus;
	KvaDbMessageHnd mh = 0;
	KvaDbSignalHnd sh = 0;

	//if args passed deffile obatin
	//else obtain from redis

	//Parse def file
	yfh = fopen(argv[1], "r");

	if (!yaml_parser_initialize(&yamPars)) fputs("Failed to initialize parser!\n", stderr);
	if (yfh == NULL) fputs("Failed to open file!\n", stderr);

	yaml_parser_set_input_file(&yamPars, yfh);

	do {
		yaml_parser_scan(&yamPars, &token);
		switch (token.type)
		{
			/* Stream start/end */
		//case YAML_STREAM_START_TOKEN: puts("STREAM START"); break;
		//case YAML_STREAM_END_TOKEN:   puts("STREAM END");   break;
			/* Token types (read before actual token) */
		case YAML_KEY_TOKEN:   printf("(Key token)   "); break;
		case YAML_VALUE_TOKEN: printf("(Value token) "); break;
			/* Block delimeters */
		//case YAML_BLOCK_SEQUENCE_START_TOKEN: puts("<b>Start Block (Sequence)</b>"); break;
		case YAML_BLOCK_ENTRY_TOKEN:          puts("<b>Start Block (Entry)</b>");    break;
		case YAML_BLOCK_END_TOKEN:            puts("<b>End block</b>");              break;
			/* Data */
		case YAML_BLOCK_MAPPING_START_TOKEN:  puts("[Block mapping]");            break;
		case YAML_SCALAR_TOKEN:
			printf("scalar %s : ", token.data.scalar.value);
			/*if (strcmp(token.data.scalar.value, "host_ip") == 0)
			{
				yaml_token_delete(&token);
				yaml_parser_scan(&yamPars, &token);
				printf("%s \n", token.data.scalar.value);
			}
			if (strcmp(token.data.scalar.value, "name") == 0)
			{
				yaml_token_delete(&token);
				yaml_parser_scan(&yamPars, &token);
				printf("%s \n", token.data.scalar.value);
			}*/
			break;
			/* Others */
		default:
			//printf("Got token of type %d\n", token.type);
		}
		if (token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);
	} while (token.type != YAML_STREAM_END_TOKEN);
	yaml_token_delete(&token);

	//Connect to redis
	printf("Connecting Redis Server\n");
	c = redisConnect(hostname, port);
	if (c == NULL)
	{
		printf("Connection Error!\n");
		exit(1);
	}

	//Set status values on server
	checkStatus(0, "Beggining Initialization", c, reply, c, reply, programStatus);
	programStatus = 4;

	//Initialize CAN
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
		messages = (struct Msg*)malloc(numReads * sizeof(struct Msg));
		numReads = 0;
	}

	//Validate that msg names match between dbc and def files

	//Create struct array for msgs, and signals
	//4 struct arrays needed for msgs of different send rates
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
			reads[numReads].signals[i].redisKey = (char*)malloc(200 * sizeof(char));
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

	//This is a hardcoded association instead of Yaml - Replace
	for (i = 0; i < numReads; i++)
	{
		for (j = 0; j < messages[i].numSigs; j++)
		{
			if (i == 0)
			{
				if (j == 0) strcpy(messages[i].signals[j].redisKey, "Cmd_CVD_Current");
				else if (j == 1) strcpy(messages[i].signals[j].redisKey, "Cmd_CVD_Enable");
			}
			else if (i == 1)
			{
				if (j == 0) strcpy(messages[i].signals[j].redisKey, "PositionRequest");
				else if (j == 1) strcpy(messages[i].signals[j].redisKey, "TorqueRequest");
				else if (j == 2) strcpy(messages[i].signals[j].redisKey, "HPU1_MotorSpeedRequest");
			}
			else if (i == 2)
			{
				if (j == 0) strcpy(messages[i].signals[j].redisKey, "PositionRequest");
				else if (j == 1) strcpy(messages[i].signals[j].redisKey, "TorqueRequest");
				else if (j == 2) strcpy(messages[i].signals[j].redisKey, "HPU2_MotorSpeedRequest");
			}
		}
	}

	time = clock();
	while (1)
	{
		if (clock() - time >= CLOCKS_PER_SEC * 0.01)
		{
			for (i = 0; i < numReads; i++)
			{
				status = kvaDbGetMsgById(dbHnd, reads[i].id, &mh);
				checkStatus(status, "reading message", c, reply, 2);
				for (j = 0; j < reads[i].numSigs; j++)
				{
					status = kvaDbGetSignalByName(mh, reads[i].signals[j], &sh);
					checkStatus(status, "get first signal", c, reply, 2);
					signalValue = strtof(readFromDataStore(c, reply, reads[i].signals[j].redisKey), &ptr);
					status = kvaDbStoreSignalValuePhys(sh, &reads[i].data, sizeof(reads[i].data), signalValue);
					checkStatus(status, "get float data", c, reply, 2);
				}
			}
			time = clock();
		}
	}

	redisFree(c);
	yaml_parser_delete(&yamPars);
	fclose(yfh);
	
	return 1;
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