
#ifndef __TP2854_H__
#define __TP2854_H__

enum 
{
    TP2854B_720P60 = 0x0,
    TP2854B_720P50 = 0x1,
    TP2854B_1080P30 = 0x2,
    TP2854B_1080P25 = 0x3,
    TP2854B_720P30 = 0x4,
    TP2854B_720P25 = 0x5,
    TP2854B_SD = 0x6,
    TP2854B_OTFM= 0x7,	/* Other formats */
};

enum 
{
    VIDEO_UNPLUG,
    VIDEO_IN,
    VIDEO_LOCKED,
    VIDEO_UNLOCK
};

enum 
{
    SCAN_DISABLE = 0,
    SCAN_AUTO,
    SCAN_TVI,
    SCAN_HDA,
    SCAN_HDC,
    SCAN_MANUAL,
    SCAN_TEST
};

enum 
{
    PTZ_TVI = 0,
    PTZ_HDA_1080P = 1,
    PTZ_HDA_720P = 2,
    PTZ_HDA_CVBS = 3,
    PTZ_HDC = 4,
    PTZ_HDA_3M18 = 5,  // HDA QXGA18
    PTZ_HDA_3M25 = 6,  // HDA QXGA25,QXGA30
    PTZ_HDA_4M25 = 7,  // HDA QHD25,QHD30,5M20
    PTZ_HDA_4M15 = 8,  // HDA QHD15,5M12.5
    PTZ_HDC_QHD = 9,   // HDC QHD25,QHD30
    PTZ_HDC_FIFO = 10, // HDC 1080p,720p FIFO
    PTZ_HDC_8M12 = 11, // HDC 8M12.5
    PTZ_HDC_8M15 = 12, // HDC 8M15
    PTZ_HDC_6M20 = 13  // HDC 6M20
};

enum 
{
    PTZ_RX_TVI_CMD,
    PTZ_RX_TVI_BURST,
    PTZ_RX_ACP1,
    PTZ_RX_ACP2,
    PTZ_RX_ACP3,
    PTZ_RX_TEST,
    PTZ_RX_HDC1,
    PTZ_RX_HDC2
};
		
enum
{
	TP2850 = 0x2850,
	TP2854 = 0x2854,
};

typedef enum __tag_TP285x_MODE_E
{
	TP2850_HDA720P25 			= 0x00,
	TP2850_HDA1080P25 			= 0x01,
	TP2854_HDA_720P25_4LANES 	= 0x02,
	TP2854_HDA_1080P25_4LANES 	= 0x03,
	TP2910_HDA_1080P 			= 0x04,
	TP2910_HDA_720P 			= 0x05,
	TP2854_HDA_720P25_2LANES 	= 0x06,
	TP2854_HDA_1080P25_2LANES 	= 0x07			
}TP285x_MODE_E;
			
enum
{
	VIDEO_PAGE = 0,
	MIPI_PAGE = 8
};

enum
{
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
};

enum 
{
	STD_TVI,
	STD_HDA,
	STD_HDC,
	STD_HDA_DEFAULT,
	STD_HDC_DEFAULT
};

enum
{
	PAL,
	NTSC,
	HD25,
	HD30,
	FHD25,
	FHD30,
};

enum
{
	MIPI_2CH4LANE_297M, //up to 2x1080p25/30
	MIPI_2CH4LANE_594M, //up to 2x1080p50/60
	MIPI_2CH2LANE_594M, //up to 2x1080p25/30  
	MIPI_2CH4LANE_148M, //up to 2x720p25/30
	MIPI_2CH2LANE_297M, //up to 2x720p25/30 		 
};

#define SUCCESS				0
#define FAILURE				-1
			
#define BRIGHTNESS			0x10
#define CONTRAST			0x11
#define SATURATION			0x12
#define HUE					0X13
#define SHARPNESS			0X14
	
typedef struct _tp285x_register
{
	unsigned int reg_addr;
	unsigned int value;
	unsigned char ch;
}tp285x_register;
			
			
typedef struct _tp285x_video_mode
{
	unsigned char mode;		//TP285x_MODE_E
}tp285x_video_mode;
				
typedef struct _tp285x_image_adjust
{
	unsigned int	chn;	//[0 ~ 3]: 表示通道0~3, 0xFF:表示所有通道
	unsigned int 	hue;
	unsigned int 	contrast;
	unsigned int 	brightness;
	unsigned int 	saturation;
	unsigned int 	sharpness;	
}tp285x_image_adjust;

typedef struct tp2854_resolution
{
	unsigned char chn_res[4];	//0---720p60, 1---720p50, 2---1080p30, 3---1080p25, 4---720p30, 5---720p2, 6---SD, 0xFF---VideoLoss
}tp285x_resolution;

/* IOCTL Definitions */
#define TP2850_IOC_MAGIC            	'v'
			
#define TP2850_READ_REG					_IOWR(TP2850_IOC_MAGIC, 1, tp285x_register)
#define TP2850_SET_VIDEO_MODE			_IOW(TP2850_IOC_MAGIC,  2, tp285x_video_mode)
#define TP2850_GET_VIDEO_MODE			_IOWR(TP2850_IOC_MAGIC, 3, tp285x_video_mode)
#define TP2850_SET_IMAGE_ADJUST			_IOW(TP2850_IOC_MAGIC,  4, tp285x_image_adjust)
#define TP2850_GET_IMAGE_ADJUST			_IOWR(TP2850_IOC_MAGIC, 5, tp285x_image_adjust)
#define TP2850_WRITE_REG				_IOW(TP2850_IOC_MAGIC,  6, tp285x_register)
#define TP2850_DUMP_REG					_IOW(TP2850_IOC_MAGIC,  7, tp285x_register)
#define TP285x_GET_CAMERA_RESOLUTION	_IOW(TP2850_IOC_MAGIC,  8, tp285x_resolution)
#define TP285x_SET_CAMERA_RESOLUTION	_IOW(TP2850_IOC_MAGIC,  9, tp285x_resolution)

#endif

