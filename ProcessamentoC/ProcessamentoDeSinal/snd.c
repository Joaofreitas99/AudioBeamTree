/*
 *  This small demo sends a simple sinusoidal wave to your speakers.
 */
 
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

#define alloca(x) __builtin_alloca(x)

#define M_PI 3.14159265358979323846

#define SIZE_OF_BUFFER 8192

#define MAX_SINE 65536

#define S_RATE  (65536)
#define BUF_SIZE (S_RATE*4) /* 5 second buffer */

static char *device = "plughw:0,0";         /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */
static unsigned int rate = 192000;           /* stream rate */
static unsigned int channels = 1;           /* count of channels */
static unsigned int buffer_time = 500000;       /* ring buffer length in us */
static unsigned int period_time = 100000;       /* period time in us */
static int verbose = 0;                 /* verbose flag */
static int resample = 1;                /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */
 
static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

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

typedef struct payload_t {
    uint32_t id;
} payload;

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
 
 
static int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size = SIZE_OF_BUFFER;
    
     
    
    int err, dir;
 
    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
 
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;
 
    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
        if (err < 0) {
            printf("Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
 
/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (verbose)
        printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}
 
/*
 *   Transfer method - write only
 */

void hilbert(double complex *z, unsigned long n);
	
static int write_loop(snd_pcm_t *handle,
              signed short *samples,
              snd_pcm_channel_area_t *areas)
{
    signed short *ptr;
    int err, cptr;
 
        ptr = samples;
        cptr = SIZE_OF_BUFFER;
        while (cptr > 0) {
            err = snd_pcm_writei(handle, ptr, cptr);
            if (err == -EAGAIN)
                continue;
            if (err < 0) {
                if (xrun_recovery(handle, err) < 0) {
                    printf("Write error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                break;  /* skip one period */
            }
            ptr += err * channels;
            cptr -= err;
        }
	
	return 1;
    
}
 
 
/*
 *
 */
 
struct transfer_method {
    const char *name;
    snd_pcm_access_t access;
    int (*transfer_loop)(snd_pcm_t *handle,
                 signed short *samples,
                 snd_pcm_channel_area_t *areas);
};
 
static struct transfer_method transfer_methods[] = {
    { "write", SND_PCM_ACCESS_RW_INTERLEAVED, write_loop },
    { NULL, SND_PCM_ACCESS_RW_INTERLEAVED, NULL }
};

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


 
 
int main(int argc, char *argv[])
{
	printf("loading\n");
	
	int iCarrier = 0;
	int iSSB = 0;
	
	float fs = rate;
	float fc = 39500.0;
	
	int norm = pow(2,(sizeof (short)*8)-1); // normalização do valor das amostras para +-1
	
	WavePtr=0;
	    
	//socket variables	
	int PORT = 2300;
	int BUFFSIZE = 512;
	char buff[BUFFSIZE];
	int ssock, csock;
	int nread;
	struct sockaddr_in client;
	int clilen = sizeof(client);
	int songId = 0;
	
	char* songName = "";    
	WavePtr = 0;

// criação dos sockets para a receção do id da musica
    ssock = createSocket(PORT);
    printf("Server listening on port %d\n", PORT);

    
// deteção e receção da informação recebida no socket
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

	printf("songId = %d\n", songId);

/*----------------------------------------------*/
	
/* aós deteção e receção do id, esse id é utilizado para afetação da variavel relativa ao nome da musica */
	
	switch (songId) {
		case 1:
			printf("Song #1 will play soon.\n");
			songName = "everybodyschanging192.wav";
			break;
		case 2:
			printf("Song #2 will play soon.\n");
			songName = "macdemarco192.wav";
			break;
		case 3:
			printf("Song #3 will play soon.\n");
			songName = "odetothemets192.wav";
			break;		
		default:
			printf("Song #3 will play soon!\n");
			songName = "everybodyschanging192.wav";
			break;
	}

	/*-----------------------------------------*/
	
	
	/* variáveis relativas ao processamento*/
	
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
	
	double window[SIZE_OF_BUFFER*2]= { 0 };
	
	for(int i=0; i<windowSize; i++){
		window[i] = 0.5 * (1 - cos(2*M_PI*i/(windowSize-1))); // Hanning Window
	}
	
// snd variables

    snd_pcm_t *handle;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    int err;
    int method = 0;
    signed short *samples;
    unsigned int chn;
    snd_pcm_channel_area_t *areas;
 
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);

	/* definição dos parâmetros relativos à placa de som*/
    
    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
        printf("Output failed: %s\n", snd_strerror(err));
        return 0;
    }
 
    printf("Playback device is %s\n", device);
    printf("Stream parameters are %uHz, %s, %u channels\n", rate, snd_pcm_format_name(format), channels);
    printf("Using transfer method: %s\n", transfer_methods[method].name);
 
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }
     /* parâmetros do hardware */
    if ((err = set_hwparams(handle, hwparams, transfer_methods[method].access)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

 /* parâmetros do software */
    if ((err = set_swparams(handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
 
    if (verbose > 0)
        snd_pcm_dump(handle, output);
 

/*alojamento em memoria do array a enviar para a placa de som */
    samples = malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
    if (samples == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }
    
    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    if (areas == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }
    for (chn = 0; chn < channels; chn++) {
        areas[chn].addr = samples;
        areas[chn].first = chn * snd_pcm_format_physical_width(format);
        areas[chn].step = channels * snd_pcm_format_physical_width(format);
    }
    
    waveLoad(songName);

	/* algoritmo de processamento*/

	while(waveIndex < WaveSize){ // array relativo ao processamento
		
		if(buf_len != SIZE_OF_BUFFER){ // leitura do ficheiro em blocos
			
			circularBuffer[writeIndex++] = (double complex) WavePtr[waveIndex++]/norm;
			
			buf_len++;
		} else { 	// processamento do bloco lido

				for(int i=0; i<SIZE_OF_BUFFER; i++){ //aplicação da janela ao bloco
						aux[i]=anterior[i]* window[i];
						auxHilb[i]=aux[i];
						
				}
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){	/* criação do buffer a enviar ao hilbert (dobro do tamanho) */
				    anterior[i] = circularBuffer[i];
				    aux[i+SIZE_OF_BUFFER]= circularBuffer[i] * window[i+SIZE_OF_BUFFER];
				    auxHilb[i+SIZE_OF_BUFFER]=aux[i+SIZE_OF_BUFFER];
				}
				
				hilbert(auxHilb, SIZE_OF_BUFFER*2); //hilbert
							
				for(int i=0; i<SIZE_OF_BUFFER*2; i++){	/* SSB */
					aux[i] = (aux[i]*cos(fc*2*M_PI/fs*(float)iSSB) - cimag(auxHilb[i])*sin(fc*2*M_PI/fs*(float)iSSB));
					iSSB++;
					
				}
				
				if(iSSB == 1179648){	// reset do contador do cosseno da SSB
					    iSSB = 0;
					}
				
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){ /* adição da portadora */
					posPros[i]= cos(fc*2*M_PI/fs*(float)iCarrier)/3 + (aux[i]+anteriorPos[i])/3;
					iCarrier++;
					
					
				}
				
				if(iCarrier == 1179648){ //reset do contador do cosseno da portadora
				    
					    iCarrier = 0;
					}
				
				for(int i=0; i<SIZE_OF_BUFFER; i++){  
				    anteriorPos[i]=aux[i+SIZE_OF_BUFFER];	// guardar a segunda metade do processamento de hilbert
				    samples[i] = ceil(posPros[i]*norm);		//converter as amostras de double (64 bits) para o formato compativel com a coluna (16 bits)
				}
				
				err = transfer_methods[method].transfer_loop(handle, samples, areas); // chamada da função para o envio dos blocos para a placa de som
				
				if (err < 0)
					printf("Transfer failed: %s\n", snd_strerror(err));
				
				buf_len = 0;
				writeIndex = 0;
		}
		
	}
    
 
    free(areas);
    free(samples);
    snd_pcm_close(handle);
    return 0;
}
