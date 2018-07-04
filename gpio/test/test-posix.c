#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>

const char * file_gpio = "/dev/rasp_gpio_2";

//< value to write
const char gpio_value[] = {'1'};

//< buffer to read
char gpio_buffer[] = {'x'};

int main(int argc, char * argv[]){
	//< write gpio
	int fd = open(file_gpio, O_WRONLY);
	if(0 > fd){
		fprintf(stderr, "Open file [%s] failed!\n", file_gpio);
		fprintf(stderr, "Errno is [%d]!\n", errno);
		return fd;
	}

	if(0 > write(fd, gpio_value, sizeof(gpio_value))){
		fprintf(stderr, "Write file [%s] failed!\n", file_gpio);
		fprintf(stderr, "Errno is [%d]!\n", errno);
		return -1;
	}

	close(fd);

	//< read gpio
	fd = open(file_gpio, O_RDONLY);
	if(0 > fd){
		fprintf(stderr, "Open file [%s] failed!\n", file_gpio);
		fprintf(stderr, "Errno is [%d]!\n", errno);
		return fd;
	}

	if(0 > read(fd, gpio_buffer, sizeof(gpio_buffer))){
		fprintf(stderr, "Read file [%s] failed!\n", file_gpio);
		fprintf(stderr, "Errno is [%d]!\n", errno);
		return -1;
	}

	printf("Read gpio [%c]!\n", gpio_buffer[0]);

	close(fd);

	return 0;
}

