
a.out: HilbertTest_WaveLoad.o hillb.o am_sysdep.o
	gcc -std=c99 -g HilbertTest_WaveLoad.o hillb.o am_sysdep.o -lm -lasound

hillb.o: hillb.c am_sysdep.h am_types.h math_const.h transform.h
	gcc -std=c99  -g -c hillb.c

am_sysdep.o: am_sysdep.c am_sysdep.h
	gcc -std=c99  -g -c am_sysdep.c

HilbertTest_WaveLoad.o: HilbertTest_WaveLoad.c
	gcc -std=c99 -g -c HilbertTest_WaveLoad.c -lm


