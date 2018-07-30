ALL: WSN

WSN: WSN.c
	mpicc -o WSN WSN.c

clean: 
	/bin/rm -f WSN *.o *~
