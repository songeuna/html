#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>                                               /* Low level I/O�� ���ؼ� ��� */
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>                                     /* <videodev2.h> ������ ���ؼ� ��� */
#include <linux/videodev2.h>                             /* Video4Linux2�� ���� ������� */
#include <linux/fb.h>

#include "bmpHeader.h"

#define MEMZERO(x) memset(&(x), 0, sizeof(x))    /* ���� �ʱ�ȭ�� ���� ��ũ�� */

#define VIDEODEV        "/dev/video0"               /* Pi Camera�� ���� ����̽� ���� */
#define FBDEV               "/dev/fb0"                    /* �����̹��۸� ���� ����̽� ���� */
#define WIDTH               640                              /* ĸ�Ĺ��� ������ ũ�� */
#define HEIGHT             480                
#define NUMCOLOR	  3

/* Video4Linux���� ����� ���� ������ ���� ���� */
struct buffer {
    void * start;
    size_t length;
};

static int fd = -1;                                                  /* Pi Camera�� ��ġ�� ���� ��ũ���� */
struct buffer *buffers = NULL;                             /* Pi Camera�� MMAP�� ���� ���� */
static int fbfd = -1;                                               /* �����ӹ����� ���� ��ũ���� */
static short *fbp = NULL;                                     /* �����ӹ����� MMAP�� ���� ���� */
static struct fb_var_screeninfo vinfo;                   /* �����ӹ����� ���� ������ ���� ����ü */

static void processImage(const void *p);
static int readFrame();
static void initRead(unsigned int buffer_size);

#ifdef __cplusplus extern "C" 
{ 
#endif

#define widthbytes(bits)   (((bits)+31)/32*4)  // for padding bit

void saveImage(unsigned char *inimg)
{
	RGBQUAD palrgb[256];
	FILE *fp;
        int imgfd;
	BITMAPFILEHEADER bmpHeader;
	BITMAPINFOHEADER bmpInfoHeader;

        MEMZERO(bmpHeader);
	bmpHeader.bfType[0] = 'B';
	bmpHeader.bfType[1] = 'M';
	bmpHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpHeader.bfSize += sizeof(RGBQUAD) * 256;
	bmpHeader.bfSize += WIDTH*HEIGHT*NUMCOLOR;
	bmpHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); // 54;
	bmpHeader.bfOffBits += sizeof(RGBQUAD) * 256; 

        MEMZERO(bmpInfoHeader);
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER); //40;
	bmpInfoHeader.biWidth = WIDTH;
	bmpInfoHeader.biHeight = HEIGHT;
	bmpInfoHeader.biPlanes = 1;
	bmpInfoHeader.biBitCount = NUMCOLOR*8;
	bmpInfoHeader.SizeImage = WIDTH*HEIGHT*bmpInfoHeader.biBitCount/8;
	bmpInfoHeader.biXPelsPerMeter = 0x0B12;
	bmpInfoHeader.biYPelsPerMeter = 0x0B12;

	if((fp = fopen("capture.bmp", "wb")) == NULL) {
		fprintf(stderr, "Error : Failed to open file...\n");
		exit(EXIT_FAILURE);
	}
#if 0 
	//////////////////////// BITMAPFILEHEADER
	fwrite(&bmpHeader.bfType, sizeof(char), 2, fp);
	fwrite(&bmpHeader.bfSize, sizeof(unsigned int), 1, fp);
	fwrite(&bmpHeader.bfReserved1, sizeof(unsigned short int), 1, fp);
	fwrite(&bmpHeader.bfReserved2, sizeof(unsigned short int), 1, fp);
	fwrite(&bmpHeader.bfOffBits, sizeof(unsigned int), 1, fp);
#else
         /* BMP ����(BITMAPFILEHEADER) ���� ���� */ 
         fwrite((void*)&bmpHeader, sizeof(bmpHeader), 1, fp);
#endif

#if 0
      	/////////////////////////// BITMAPINFOHEADER
	fwrite(&bmpInfoHeader.biSize, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biWidth, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biHeight, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biPlanes, sizeof(unsigned short int), 1, fp);
	fwrite(&bmpInfoHeader.biBitCount, sizeof(unsigned short int), 1, fp);
	fwrite(&bmpInfoHeader.biCompression, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.SizeImage, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biXPelsPerMeter, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biYPelsPerMeter, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biClrUsed, sizeof(unsigned int), 1, fp);
	fwrite(&bmpInfoHeader.biClrImportant, sizeof(unsigned int), 1, fp);
#else
        fwrite((void*)&bmpInfoHeader, sizeof(bmpInfoHeader), 1, fp);
#endif
	
      	/////////////////////////// RGBQUAD
	fwrite(palrgb, sizeof(RGBQUAD), 256, fp);
	
      	/////////////////////////// Image Data
	fwrite(inimg, sizeof(unsigned char), WIDTH*HEIGHT*3, fp);

	fclose(fp);
}

static void initDevice()
{   
    struct v4l2_capability cap;                                 /* ���� ��ġ�� ���� ����� �����Ѵ�. */
    struct v4l2_format fmt;
    unsigned int min;

    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {       /* ��ġ�� V4L2�� �����ϴ��� ���� */
        if(errno == EINVAL) {
            perror("/dev/video0 is no V4L2 device");
            exit(EXIT_FAILURE);
        } else {
            perror("VIDIOC_QUERYCAP");
            exit(EXIT_FAILURE);
        }
    }

    /* ��ġ�� ���� ĸ�� ����� �ִ��� ���� */
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {  /* fill the blank */
        perror("/dev/video0 is no video capture device");
        exit(EXIT_FAILURE);
    }

    MEMZERO(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("VIDIOC_S_FMT");
        exit(EXIT_FAILURE);
    }

    /* ������ �ּ� ũ�⸦ ���� */
    min = fmt.fmt.pix.width * 2;
    if(fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if(fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    /* ���� �б⸦ ���� �ʱ�ȭ */
    initRead(fmt.fmt.pix.sizeimage); 
}

static void initRead(unsigned int buffer_size) 
{
    /* �޸𸮸� �Ҵ��Ѵ� */
    buffers = (struct buffer*)calloc(1, sizeof(*buffers));
    if(!buffers) {
        perror("Out of memory");
        exit(EXIT_FAILURE);
    }

    /* ���۸� �ʱ�ȭ �Ѵ�. */
    buffers[0].length = buffer_size;
    buffers[0].start = malloc(buffer_size);
    if(!buffers[0].start) {
        perror("Out of memory");
        exit(EXIT_FAILURE);
    }
} 

#define NO_OF_LOOP    2//100

static void mainloop() 
{
    unsigned int count = NO_OF_LOOP;
    while(count-- > 0) {                                                /* 100ȸ �ݺ� */
        for(;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            /* fd_set ����ü�� �ʱ�ȭ�ϰ� ���� ��ġ�� ���� ��ũ���͸� �����Ѵ�. */
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Ÿ�Ӿƿ�(Timeout)�� 2�ʷ� �����Ѵ�. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);      /* ���� �����Ͱ� �ö����� ��� */
            switch(r) {
                case -1:                                                     /* select( ) �Լ� �������� ó�� */
                      if(errno != EINTR) {
                          perror("select( )");
                          exit(EXIT_FAILURE);
                      }
                      break;
                case 0:                                                     /* Ÿ�Ӿƿ����� ó�� */
                      perror("select timeout");
                      exit(EXIT_FAILURE);
                      break;
            };

            if(readFrame()) break;                                 /* ������ �������� �о ǥ�� */
        };
    };
	

}

static int readFrame( ) 
{
  if(read(fd,buffers[0].start,buffers[0].length)<0){ /* fill the blank */
        perror("read( )");
        exit(EXIT_FAILURE);
  }

  processImage(buffers[0].start);

  return 1;
}

/* unsigned char�� ������ �Ѿ�� �ʵ��� ��� �˻縦 �����. */
int clip(int value, int min, int max)
{
    return(value > max ? max : value < min ? min : value);
}

/* YUYV�� RGB16���� ��ȯ�Ѵ�. */
static void processImage(const void *p)
{
    unsigned char* in =(unsigned char*)p;
    int width = WIDTH;
    int height = HEIGHT;
    unsigned char inimg[3*WIDTH*HEIGHT];
 
    unsigned short pixel;
    int istride = WIDTH*2;          /* �̹����� ���� �Ѿ�� ���� �������� ���������� ���� */
    int x, y, j;
    int y0, u, y1, v, r, g, b;
    long location = 0;

    int count=0;

    for(y = 0; y < height; y++) {
        for(j = 0, x = 0; j < vinfo.xres * 2; j += 4, x += 2) {
            if(j >= width*2) {                 /* ������ ȭ�鿡�� �̹����� �Ѿ�� �� ������ ó�� */
                 location++; location++;
                 continue;
            }
/* fill the blank */
	// YUYV 
	y0 = in[j];
        u = in[j+1]-128;
	y1 = in[j+2];
        v = in[j+3]-128;

	// YUV -> RGB
	r = clip((298*y0 + 409*v + 128) >> 8, 0, 255);
	g = clip((298*y0 - 100*u - 208*v + 128) >> 8, 0, 255);
	b = clip((298*y0 + 516*u + 128) >> 8, 0, 255);
	if(r < 100) {
	   b = g = 0;
	} else {
	  b = g = r;
	}
	
	inimg[(height-y-1)*width*3+count++] = b;
	inimg[(height-y-1)*width*3+count++] = g;
	inimg[(height-y-1)*width*3+count++] = r;
	pixel = ( (r>>3)<<11 )|( (g>>2)<<5 )|(b>>3);
	fbp[location++] = pixel;
 	
	r = clip((298*y1 + 409*v + 128) >> 8, 0, 255);
	g = clip((298*y1 - 100*u - 208*v + 128) >> 8, 0, 255);
	b = clip((298*y1 + 516*u + 128) >> 8, 0, 255);
	
	if(r < 100) {
	   b = g = 0;
	} else {
	  b = g = r;
	}
	inimg[(height-y-1)*width*3+count++] = b;
	inimg[(height-y-1)*width*3+count++] = g;
	inimg[(height-y-1)*width*3+count++] = r;
	pixel = ( (r>>3)<<11 )|( (g>>2)<<5 )|(b>>3);
	fbp[location++] = pixel;

       };
       in += istride;
       count = 0;
    };
    saveImage(inimg);
}

static void uninitDevice() 
{
    long screensize = vinfo.xres * vinfo.yres * 2;

    free(buffers[0].start);
    free(buffers);
    munmap(fbp, screensize);
}

int captureImage(int sockfd)
{
    long screensize = 0;

    /* ��ġ ���� */
    /* Pi Camera ���� */
    fd = open(VIDEODEV, O_RDWR| O_NONBLOCK, 0);
    if(fd == -1) {
        perror("open( ) : video devive");
        return EXIT_FAILURE;
    }

    /* �����ӹ��� ���� */
    fbfd = open(FBDEV, O_RDWR); /* fill the blank */
    if(fbfd == -1) {
        perror("open( ) : framebuffer device");
        return EXIT_FAILURE;
    }

    /* �����ӹ����� ���� �������� */
    if(ioctl(fbfd,FBIOGET_VSCREENINFO,&vinfo)==-1){ /* fill the blank */
         perror("Error reading variable information.");
         exit(EXIT_FAILURE);
    }

    /* mmap( ) : �����ӹ��۸� ���� �޸� ���� Ȯ�� */
    screensize = vinfo.xres * vinfo.yres * 2;
    fbp=(short *)mmap(NULL,screensize,PROT_READ|PROT_WRITE,MAP_SHARED,fbfd,0);   /* fill the blank */
    if((int)fbp == -1) {
        perror("mmap() : framebuffer device to memory");
        return EXIT_FAILURE;
    }
    memset(fbp, 0, screensize);

    initDevice();                                                        /* ��ġ �ʱ�ȭ */ 
    mainloop();                                                         /* ĸ�� ���� */
    uninitDevice();                                                    /* ��ġ ���� */

    /* ��ġ �ݱ� */
    close(fbfd);
    close(fd);

    return EXIT_SUCCESS;                                    /* ���ø����̼� ���� */
}

#ifdef __cplusplus 
}
#endif
