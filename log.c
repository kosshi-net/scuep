

void log_init(){



}


void logf(char*format, ...){
	char buffer[1024];
	va_list args;
	va_start (args, format);
	vsprintf (buffer,format, args);



	perror (buffer);

	va_end (args);
}


