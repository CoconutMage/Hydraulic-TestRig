HIREDIS_PATH=Libraries/hiredis
LINUXCAN_PATH=linuxcan
CANLIB=libcanlib.so
DBCLIB=libkvadblib.so
HIREDISLIB=libhiredis.a
YAMLLIB=yaml

all: clean main reader writer

main:
	gcc CANI.c -Ilinuxcan -o CANI -l:libcanlib.so -l:libkvadblib.so -ILibraries/hiredis -l:libhiredis.a
	gcc testInterface.c -ILibraries/hiredis -o TestInterface -l:libhiredis.a
	gcc master.c -o master
clean:
	rm -f CANI
	rm -f TestInterface
	rm -f master
	rm -f CANIReader
	rm -f CANIWriter
	rm -f OutputLogger
reader:
	gcc CANIReader.c -I${LINUXCAN_PATH} -I${HIREDIS_PATH} -o CANIReader -l${CANLIB} -l${DBCLIB} -l${HIREDISLIB}
	gcc OutputLogger.c -I${LINUXCAN_PATH} -I${HIREDIS_PATH} -o OutputLogger -l${CANLIB} -l${DBCLIB} -l:${HIREDISLIB}
writer:
	gcc CANIWriter.c -I${LINUXCAN_PATH} -I${HIREDIS_PATH} -o CANIWriter -l:${CANLIB} -l:${DBCLIB} -l:${HIREDISLIB} -l${YAMLLIB}
