#include <stdio.h>
#include <errno.h>

const char * file_gpio = "/dev/rasp_gpio_2";
const char gpio_state  = '1';

int main(int argc, char * argv){
	//< write gpio
	FILE * stream = fopen(file_gpio, "w");

	if(NULL == stream){
		return -ENOSR;
	}

	if(EOF == fputc(gpio_state, stream)){
		return ferror(stream);
	}

	fclose(stream);

	//< read gpio
	stream = fopen(file_gpio, "r");

	if(NULL == stream){
		return -ENOSR;
	}

	char c = fgetc(stream);
	printf("Read [%c] from %s!\n", c, file_gpio);

	fclose(stream);

	return 0;
}

