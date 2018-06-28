#include <stdio.h>
#include <errno.h>

int main(int argc, char * argv){
	FILE * stream = fopen("/dev/rasp_gpio_2", "w");

	if(NULL == stream){
		return -ENOSR;
	}

	if(EOF == fputc('1', stream)){
		return ferror(stream);
	}

	fclose(stream);

	return 0;
}
