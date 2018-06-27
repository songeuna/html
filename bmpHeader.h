#ifndef __BMP_FILE_H__
#define __BMP_FILE_H__

typedef struct  __attribute__((__packed__)) {
    unsigned char bfType[2];                  /* BM ǥ�� : "BM" (2 ����) ���� */
    unsigned int bfSize;                       /* ������ ũ�� : 4 ����Ʈ ����  */
    unsigned short bfReserved1;        /* ������ Ȯ���� ���ؼ� �ʵ�(reserved) */
    unsigned short bfReserved2;        /* ������ Ȯ���� ���ؼ� �ʵ�(reserved) */
    unsigned int bfOffBits;                   /* ���� �̹��������� ������(offset) : ����Ʈ */
} BITMAPFILEHEADER;

typedef struct {
    unsigned int biSize;                       /* �� ����ü�� ũ�� : 4����Ʈ */
    unsigned int biWidth;                     /* �̹����� ��(�ȼ�����) : 4����Ʈ */
    unsigned int biHeight;                    /* �̹����� ����(�ȼ�����) : 4����Ʈ */
    unsigned short biPlanes;               /* ��Ʈ �÷��� �� (�׻�1) : 2����Ʈ */
    unsigned short biBitCount;            /* �ȼ� �� ��Ʈ �� : 2����Ʈ */
    unsigned int biCompression;         /* ���� ���� : 4����Ʈ */
    unsigned int SizeImage;                /* �̹����� ũ��(���� ���� ����Ʈ ����) : 4����Ʈ */
    unsigned int biXPelsPerMeter;      /* ���� �ػ�  : 4����Ʈ */
    unsigned int biYPelsPerMeter;      /* ���� �ػ� : 4����Ʈ */
    unsigned int biClrUsed;                 /* ���� ���Ǵ� ����� : 4����Ʈ */
    unsigned int biClrImportant;          /* �߿��� ���� �ε���(0 �� ��� ��ü) : 4����Ʈ */
} BITMAPINFOHEADER;

typedef struct {
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} RGBQUAD;

#endif /* __BMP_FILE_H__ */
