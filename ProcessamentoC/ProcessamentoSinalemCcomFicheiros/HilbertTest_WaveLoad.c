#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <complex.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define M_PI 3.14159265358979323846

#define SIZE_OF_BUFFER 65536

#define MAX_SINE 65536

#define S_RATE  (65536)
#define BUF_SIZE (S_RATE*4) /* 5 second buffer */

#define S_RATE_TONE  (48000)
#define BUF_SIZE_TONE (S_RATE_TONE*5)

#define S_RATE_TONE_ULTRA  (96000)

typedef struct payload_t {
    uint32_t id;
} payload;

#pragma pack (1)
/////////////////////// WAVE File Stuff /////////////////////
// An IFF file header looks like this
typedef struct _FILE_head
{
	unsigned char	ID[4];	// could be {'R', 'I', 'F', 'F'} or {'F', 'O', 'R', 'M'}
	unsigned int	Length;	// Length of subsequent file (including remainder of header). This is in
									// Intel reverse byte order if RIFF, Motorola format if FORM.
	unsigned char	Type[4];	// {'W', 'A', 'V', 'E'} or {'A', 'I', 'F', 'F'}
} FILE_head;


// An IFF chunk header looks like this
typedef struct _CHUNK_head
{
	unsigned char ID[4];	// 4 ascii chars that is the chunk ID
	unsigned int	Length;	// Length of subsequent data within this chunk. This is in Intel reverse byte
							// order if RIFF, Motorola format if FORM. Note: this doesn't include any
							// extra byte needed to pad the chunk out to an even size.
} CHUNK_head;

// WAVE fmt chunk
typedef struct _FORMAT {
	short				wFormatTag;
	unsigned short	wChannels;
	unsigned int	dwSamplesPerSec;
	unsigned int	dwAvgBytesPerSec;
	unsigned short	wBlockAlign;
	unsigned short	wBitsPerSample;
  // Note: there may be additional fields here, depending upon wFormatTag
} FORMAT;
#pragma pack()



#define DURATION 5


// Size of the audio card hardware buffer. Here we want it
// set to 1024 16-bit sample points. This is relatively
// small in order to minimize latency. If you have trouble
// with underruns, you may need to increase this, and PERIODSIZE
// (trading off lower latency for more stability)
#define BUFFERSIZE	(2*1024)

// How many sample points the ALSA card plays before it calls
// our callback to fill some more of the audio card's hardware
// buffer. Here we want ALSA to call our callback after every
// 64 sample points have been played
#define PERIODSIZE	(2*64)

// Points to loaded WAVE file's data
short			*WavePtr;

// Size (in frames) of loaded WAVE file's data
snd_pcm_uframes_t		WaveSize;

// Sample rate
unsigned short			WaveRate;

// Bit resolution
unsigned char			WaveBits;

// Number of channels in the wave file
unsigned char			WaveChannels;

// For WAVE file loading
static const unsigned char Riff[4]	= { 'R', 'I', 'F', 'F' };
static const unsigned char WaveF[4] = { 'W', 'A', 'V', 'E' };
static const unsigned char Fmt[4] = { 'f', 'm', 't', ' ' };
static const unsigned char Data[4] = { 'd', 'a', 't', 'a' };

/********************** compareID() *********************
 * Compares the passed ID str (ie, a ptr to 4 Ascii
 * bytes) with the ID at the passed ptr. Returns TRUE if
 * a match, FALSE if not.
 */

static unsigned char compareID(const unsigned char * id, unsigned char * ptr)
{
	register unsigned char i = 4;

	while (i--)
	{
		if ( *(id)++ != *(ptr)++ ) return(0);
	}
	return(1);
}


void hilbert(double complex *z, unsigned long n);

/********************** waveLoad() *********************
 * Loads a WAVE file.
 *
 * fn =			Filename to load.
 *
 * RETURNS: 0 if success, non-zero if not.
 *
 * NOTE: Sets the global "WavePtr" to an allocated buffer
 * containing the wave data, and "WaveSize" to the size
 * in sample points.
 */

static unsigned char waveLoad(const char *fn)
{
	const char				*message;
	FILE_head				head;
	register int			inHandle;

	if ((inHandle = open(fn, O_RDONLY)) == -1)
		message = "didn't open";

	// Read in IFF File header
	else
	{
		if (read(inHandle, &head, sizeof(FILE_head)) == sizeof(FILE_head))
		{
			// Is it a RIFF and WAVE?
			if (!compareID(&Riff[0], &head.ID[0]) || !compareID(&WaveF[0], &head.Type[0]))
			{
				message = "is not a WAVE file";
				goto bad;
			}

			// Read in next chunk header
			while (read(inHandle, &head, sizeof(CHUNK_head)) == sizeof(CHUNK_head))
			{
				// ============================ Is it a fmt chunk? ===============================
				if (compareID(&Fmt[0], &head.ID[0]))
				{
					FORMAT	format;

					// Read in the remainder of chunk
					if (read(inHandle, &format.wFormatTag, sizeof(FORMAT)) != sizeof(FORMAT)) break;

					// Can't handle compressed WAVE files
					if (format.wFormatTag != 1)
					{
						message = "compressed WAVE not supported";
						goto bad;
					}

					WaveBits = (unsigned char)format.wBitsPerSample;
					WaveRate = (unsigned short)format.dwSamplesPerSec;
					WaveChannels = format.wChannels;
				}

				// ============================ Is it a data chunk? ===============================
				else if (compareID(&Data[0], &head.ID[0]))
				{
					// Size of wave data is head.Length. Allocate a buffer and read in the wave data
					if (!(WavePtr = (short *)malloc(head.Length)))
					{
						message = "won't fit in RAM";
						goto bad;
					}

					if (read(inHandle, WavePtr, head.Length) != head.Length)
					{
						free(WavePtr);
						break;
					}

					// Store size (in frames)
					WaveSize = (head.Length * 8) / ((unsigned int)WaveBits * (unsigned int)WaveChannels);
					
					close(inHandle);
					return(0);
				}

				// ============================ Skip this chunk ===============================
				else
				{
					if (head.Length & 1) ++head.Length;  // If odd, round it up to account for pad byte
					lseek(inHandle, head.Length, SEEK_CUR);
				}
			}
		}

		message = "is a bad WAVE file";
bad:	close(inHandle);
	}
	
	printf("%s %s\n", fn, message);
	return(1);
}

// -------------------------------------------------- [ Section: Endianness ] -
int isBigEndian() {
    int test = 1;
    char *p = (char*)&test;

    return p[0] == 0;
}
void reverseEndianness(const long long int size, void* value){
    int i;
    char result[32];
    for( i=0; i<size; i+=1 ){
        result[i] = ((char*)value)[size-i-1];
    }
    for( i=0; i<size; i+=1 ){
        ((char*)value)[i] = result[i];
    }
}
void toBigEndian(const long long int size, void* value){
    char needsFix = !( (1 && isBigEndian()) || (0 && !isBigEndian()) );
    if( needsFix ){
        reverseEndianness(size,value);
    }
}
void toLittleEndian(const long long int size, void* value){
    char needsFix = !( (0 && isBigEndian()) || (1 && !isBigEndian()) );
    if( needsFix ){
        reverseEndianness(size,value);
    }
}

// ------------------------------------------------- [ Section: Wave Header ] -
typedef struct WaveHeader {
    // Riff Wave Header
    char chunkId[4];
    int  chunkSize;
    char format[4];

    // Format Subchunk
    char subChunk1Id[4];
    int  subChunk1Size;
    short int audioFormat;
    short int numChannels;
    int sampleRate;
    int byteRate;
    short int blockAlign;
    short int bitsPerSample;
    //short int extraParamSize;

    // Data Subchunk
    char subChunk2Id[4];
    int  subChunk2Size;

} WaveHeader;

WaveHeader makeWaveHeader(int const sampleRate, short int const numChannels, short int const bitsPerSample ){
    WaveHeader myHeader;

    // RIFF WAVE Header
    myHeader.chunkId[0] = 'R';
    myHeader.chunkId[1] = 'I';
    myHeader.chunkId[2] = 'F';
    myHeader.chunkId[3] = 'F';
    myHeader.format[0] = 'W';
    myHeader.format[1] = 'A';
    myHeader.format[2] = 'V';
    myHeader.format[3] = 'E';

    // Format subchunk
    myHeader.subChunk1Id[0] = 'f';
    myHeader.subChunk1Id[1] = 'm';
    myHeader.subChunk1Id[2] = 't';
    myHeader.subChunk1Id[3] = ' ';
    myHeader.audioFormat = 1; // FOR PCM
    myHeader.numChannels = numChannels; // 1 for MONO, 2 for stereo
    myHeader.sampleRate = sampleRate; // ie 44100 hertz, cd quality audio
    myHeader.bitsPerSample = bitsPerSample; // 
    myHeader.byteRate = myHeader.sampleRate * myHeader.numChannels * myHeader.bitsPerSample / 8;
    myHeader.blockAlign = myHeader.numChannels * myHeader.bitsPerSample/8;

    // Data subchunk
    myHeader.subChunk2Id[0] = 'd';
    myHeader.subChunk2Id[1] = 'a';
    myHeader.subChunk2Id[2] = 't';
    myHeader.subChunk2Id[3] = 'a';

    // All sizes for later:
    // chuckSize = 4 + (8 + subChunk1Size) + (8 + subChubk2Size)
    // subChunk1Size is constanst, i'm using 16 and staying with PCM
    // subChunk2Size = nSamples * nChannels * bitsPerSample/8
    // Whenever a sample is added:
    //    chunkSize += (nChannels * bitsPerSample/8)
    //    subChunk2Size += (nChannels * bitsPerSample/8)
    myHeader.chunkSize = 4+8+16+8+0;
    myHeader.subChunk1Size = 16;
    myHeader.subChunk2Size = 0;
    
    return myHeader;
}

// -------------------------------------------------------- [ Section: Wave ] -
typedef struct Wave {
    WaveHeader header;
    char* data;
    long long int index;
    long long int size;
    long long int nSamples;
} Wave;

Wave makeWave(int const sampleRate, short int const numChannels, short int const bitsPerSample){
    Wave myWave;
    myWave.header = makeWaveHeader(sampleRate,numChannels,bitsPerSample);
    return myWave;
}
void waveDestroy( Wave* wave ){
    free( wave->data );
}
void waveSetDuration( Wave* wave, const float seconds ){
    long long int totalBytes = (long long int)(wave->header.byteRate*seconds);
    wave->data = (char*)malloc(totalBytes);
    wave->index = 0;
    wave->size = totalBytes;
    wave->nSamples = (long long int) wave->header.numChannels * wave->header.sampleRate * seconds;
    wave->header.chunkSize = 4+8+16+8+totalBytes;
    wave->header.subChunk2Size = totalBytes;
}
void waveAddSample( Wave* wave, const double* samples ){
    int i;
    short int sample8bit;
    int sample16bit;
    long int sample32bit;
    char* sample;
    if( wave->header.bitsPerSample == 8 ){
        for( i=0; i<wave->header.numChannels; i+= 1){
            sample8bit = (short int) (127+127.0*samples[i]);
            toLittleEndian(1, (void*) &sample8bit);
            sample = (char*)&sample8bit;
            (wave->data)[ wave->index ] = sample[0];
            wave->index += 1;
        }
    }
    if( wave->header.bitsPerSample == 16 ){
        for( i=0; i<wave->header.numChannels; i+= 1){
            sample16bit = (int) (32767*samples[i]);
            //sample = (char*)&litEndianInt( sample16bit );
            toLittleEndian(2, (void*) &sample16bit);
            sample = (char*)&sample16bit;
            wave->data[ wave->index + 0 ] = sample[0];
            wave->data[ wave->index + 1 ] = sample[1];
            wave->index += 2;
        }
    }
    if( wave->header.bitsPerSample == 32 ){
        for( i=0; i<wave->header.numChannels; i+= 1){
            sample32bit = (long int) ((pow(2,32-1)-1)*samples[i]);
            //sample = (char*)&litEndianLong( sample32bit );
            toLittleEndian(4, (void*) &sample32bit);
            sample = (char*)&sample32bit;
            wave->data[ wave->index + 0 ] = sample[0];
            wave->data[ wave->index + 1 ] = sample[1];
            wave->data[ wave->index + 2 ] = sample[2];
            wave->data[ wave->index + 3 ] = sample[3];
            wave->index += 4;
        }
    }
}
void waveToFile( Wave* wave, const char* filename ){

    // First make sure all numbers are little endian
    toLittleEndian(sizeof(int), (void*)&(wave->header.chunkSize));
    toLittleEndian(sizeof(int), (void*)&(wave->header.subChunk1Size));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.audioFormat));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.numChannels));
    toLittleEndian(sizeof(int), (void*)&(wave->header.sampleRate));
    toLittleEndian(sizeof(int), (void*)&(wave->header.byteRate));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.blockAlign));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.bitsPerSample));
    toLittleEndian(sizeof(int), (void*)&(wave->header.subChunk2Size));

    // Open the file, write header, write data
    FILE *file;
    file = fopen(filename, "wb");
    fwrite( &(wave->header), sizeof(WaveHeader), 1, file );
    fwrite( (void*)(wave->data), sizeof(char), wave->size, file );
    fclose( file );

    // Convert back to system endian-ness
    toLittleEndian(sizeof(int), (void*)&(wave->header.chunkSize));
    toLittleEndian(sizeof(int), (void*)&(wave->header.subChunk1Size));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.audioFormat));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.numChannels));
    toLittleEndian(sizeof(int), (void*)&(wave->header.sampleRate));
    toLittleEndian(sizeof(int), (void*)&(wave->header.byteRate));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.blockAlign));
    toLittleEndian(sizeof(short int), (void*)&(wave->header.bitsPerSample));
    toLittleEndian(sizeof(int), (void*)&(wave->header.subChunk2Size));
}

int createSocket(int port)
{
    int sock;
    struct sockaddr_in server;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR: Socket creation failed\n");
        exit(1);
    }
    printf("Socket created\n");

    bzero((char *) &server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        printf("ERROR: Bind failed\n");
        exit(1);
    }
    printf("Bind done\n");

    listen(sock , 3);

    return sock;
}

void closeSocket(int sock)
{
    close(sock);
    return;
}

void sendMsg(int sock, void* msg, uint32_t msgsize)
{
    if (write(sock, msg, msgsize) < 0)
    {
        printf("Can't send message.\n");
        closeSocket(sock);
        exit(1);
    }
    printf("Message sent (%d bytes).\n", msgsize);
    return;
}

int main(int argc, char **argv){
	
	printf("loading\n");
    
	//socket variables	

	int PORT = 2300;
	int BUFFSIZE = 512;
	char buff[BUFFSIZE];
	int ssock, csock;
	int nread;
	struct sockaddr_in client;
	int clilen = sizeof(client);
	int songId = 0;
	
    ssock = createSocket(PORT);
    printf("Server listening on port %d\n", PORT);

 while (songId == 0)
    {
        csock = accept(ssock, (struct sockaddr *)&client, &clilen);
        if (csock < 0)
        {
            printf("Error: accept() failed\n");
            continue;
        }

        printf("Accepted connection from %s\n", inet_ntoa(client.sin_addr));
        bzero(buff, BUFFSIZE);
        while ((nread=read(csock, buff, BUFFSIZE)) > 0)
        {
            printf("\nReceived %d bytes\n", nread);
            payload *p = (payload*) buff;
			
            printf("Received contents: id=%d\n",
                    p->id);
	    songId = p->id;
        }
        printf("Closing connection to client\n");
        printf("----------------------------\n");
        closeSocket(csock);
    }
	
	clock_t begin = clock();
	
	int wavDur = 0;
	char* songName = "";	
	printf("songId = %d\n", songId);
	
	switch (songId) {
		case 1:
			printf("Song #1 will play soon.\n");
			songName = "everybodyschanging192.wav";
			wavDur = 218;
			break;
		case 2:
			printf("Song #2 will play soon.\n");
			wavDur = 240;
			songName = "macdemarco192.wav";
			break;
		case 3:
			printf("Song #3 will play soon.\n");
			wavDur = 360;
			songName = "odetothemets192.wav";
			break;		
		default:
			printf("Song #3 will play soon!\n");
			wavDur = 360;
			songName = "odetothemets192.wav";
			break;
	}
	
	
	
	int iCarrier = 0;
	int iSSB = 0;
	
	float fs = 192000.0;
	float fc = 40000.0;
	
	int norm = pow(2,(sizeof (short)*8)-1); // normalização do valor das amostras para +-1
	
	// Create a mono (1), 16-bit sound and set the duration
    Wave mySound = makeWave((int)fs,1,16);
    waveSetDuration( &mySound, wavDur);
	WavePtr = 0; 
	printf("songName = %s\n", songName);
	waveLoad(songName);
	
	int buf_len = 0;
	int writeIndex = 0;
	int waveIndex = 0;
	int windowSize = SIZE_OF_BUFFER*2;
	
	double complex circularBuffer[SIZE_OF_BUFFER] = { 0 };	// Empty circular bufferr
	
	double posPros[SIZE_OF_BUFFER] = { 0 };
	double anterior[SIZE_OF_BUFFER] = { 0 };
	double aux[SIZE_OF_BUFFER*2] = { 0 };
	double complex auxHilb[SIZE_OF_BUFFER*2] = { 0 };
	double anteriorPos[SIZE_OF_BUFFER] = { 0 };
	double buffer[SIZE_OF_BUFFER] = { 0 };
	
	double window[SIZE_OF_BUFFER*2]= { 0 };
	
	for(int i=0; i<windowSize; i++){
		window[i] = 0.5 * (1 - cos(2*M_PI*i/(windowSize-1))); // Hanning Window
	}
	
	while(waveIndex < WaveSize){
		
		if(buf_len != SIZE_OF_BUFFER){ // Write
			
			circularBuffer[writeIndex++] = (double complex) WavePtr[waveIndex++]/norm;
			//printf("circularBuffer[%d] = %lf\n", waveIndex, (double) circularBuffer[writeIndex]);
			
			buf_len++;
		} else { // Read
				for(int i=0; i<SIZE_OF_BUFFER; i++){
						aux[i]=anterior[i]* window[i];
						auxHilb[i]=aux[i];
				}
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){
						anterior[i] = circularBuffer[i];
						aux[i+SIZE_OF_BUFFER]= circularBuffer[i] * window[i+SIZE_OF_BUFFER];
						auxHilb[i+SIZE_OF_BUFFER]=aux[i+SIZE_OF_BUFFER];
				}
				
				hilbert(auxHilb, SIZE_OF_BUFFER*2);
							
				for(int i=0; i<SIZE_OF_BUFFER*2; i++){
					aux[i] = (aux[i]*cos(fc*2*M_PI/fs*(float)iSSB) + cimag(auxHilb[i])*sin(fc*2*M_PI/fs*(float)iSSB));
					iSSB++;
					if(iSSB == 7091832){
						iSSB = 0;
					}
				}
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){
					posPros[i]= cos(fc*2*M_PI/fs*(float)iCarrier)/3 + (aux[i]+anteriorPos[i])/3;
					iCarrier++;
					if(iCarrier == 7091832){
						iCarrier = 0;
					}
					
				}
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){
					anteriorPos[i]=aux[i+SIZE_OF_BUFFER];
					printf("posPros[%d] = %lf \n", i, posPros[i]);
					waveAddSample( &mySound, &posPros[i] );
				}
				
				
				buf_len = 0;
				writeIndex = 0;
		}
		
	}
	
	waveToFile( &mySound, "fileOutput.wav");
    waveDestroy( &mySound );  
    
	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	
	printf("time spent = %lf seconds\n", time_spent);
	
	return(0);
}
