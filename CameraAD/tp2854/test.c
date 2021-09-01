#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "tp2854b.h"

int main()
{
    tp285x_resolution	temp = {0xff};
    tp285x_video_mode	mode_test = { 0x6 };

    int fd = open("/dev/tp2854",O_WRONLY);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    ioctl(fd, TP2850_SET_VIDEO_MODE, &mode_test);
    
    while (1)
	{
		ioctl(fd, TP285x_GET_CAMERA_RESOLUTION, &temp);

		printf("tp285x_resolution: %x %x \n", temp.chn_res[0]&0x7, temp.chn_res[1]&0x7);

		usleep(1000 * 1000);
    }
	
    close(fd);
	
    return 0;
}

