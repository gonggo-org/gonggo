#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#include <gear/confvar.h>
#include <gear/work.h>

static void usage(void);

#define BUFLEN 50

int main(int argc, char *argv[]){
	bool help, stop;
	char *confpath, buff[BUFLEN];
	int c, ret, pid_filenum;
	ConfVar *cv;
	pid_t pid;
	const char *pid_file;
	FILE *fp;

	help = false;
	stop = false;
	confpath = NULL;
	while(1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"conf", required_argument, 0, 0},
			{"help", no_argument, 0, 0},
			{"stop", no_argument, 0, 0},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "c:h", long_options, &option_index);
		if( c == -1 )
			break;
		switch(c) {
			case 0:
				if(strcmp(long_options[option_index].name, "conf") == 0)
					confpath = optarg;
				else if(strcmp(long_options[option_index].name, "help") == 0)
					help = true;
				else if(strcmp(long_options[option_index].name, "stop") == 0)
					stop = true;
				break;
			case 'c':
				confpath = optarg;
				break;
			case 'h':
				help = true;
				break;
			default:
				break;
		}
	}

	if(help) {
		usage();
		return 0;
	}

	if( confpath == NULL) {
		printf("configuration path should be defined\n");
		usage();
		return 1;
	}

	char* error = NULL;
	cv = confvar_validate(confpath, &error);
	if(cv==NULL) {
		if(error!=NULL) {
			printf("%s\n", error);
			free(error);
		}
		return 2;
	}

	ret = 0;

	if(stop) {
		pid_file = confvar_value(cv, CONF_PIDFILE);
		fp = fopen(pid_file, "r");
		if(fp==NULL) {
			printf("can not read %s\n", pid_file);
			ret = 2;			
		} else {
			fgets(buff, BUFLEN, fp);
			fclose(fp);
			pid = atoi(buff);
			kill(pid, SIGTERM);
		}
	} else {
		pid = fork();
		if( pid == -1 ) {
			perror("Forking Error 1");
			confvar_destroy(cv);
			return 3;
		}

		if( pid == 0 ) {//child
			pid_file = confvar_value(cv, CONF_PIDFILE);
			pid_filenum = open(pid_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

			do {
				if( pid_filenum == -1 ) {
					perror("Forking Error 2");
					ret = 3;
					break;
				}

				if( lockf(pid_filenum, F_TLOCK, 0) == -1) {
					perror("Forking Error 3");
					ret = 3;
					break;
				}

				pid = getpid();
				snprintf(buff, BUFLEN, "%d", pid);
				buff[BUFLEN-1] = '\0';
				if( write(pid_filenum, buff, strlen(buff)) == -1 ) {
					perror("Forking Error 4");
					ret = 3;
					break;
				}

				fsync(pid_filenum);

				setsid();
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);

				ret = work(pid, cv);

				close(pid_filenum);
				remove(pid_file);
			} while(false);
		}//if( pid == 0 ) {//child
	}

	confvar_destroy(cv);

	return ret;
}

static void usage() {
	printf("Usage: gonggo [OPTION]\n\n"
		"To launch gonggo background service:\n"
		"gonggo -c CONFIGPATH\n"
		"or\n"
		"gonggo --conf=CONFIGPATH\n"
		"\n"
		"To stop gonggo background service:\n"
		"gonggo -c CONFIGPATH --stop\n"
		"or\n"
		"gonggo --conf=CONFIGPATH --stop\n\n"
		"CONFIGPATH is directory and filename in which the service configuration is defined.\n"
		"Exit status:\n"
        " 0  if OK,\n"
        " 1  if one or more mandatory options are missing\n"
		" 2  if configuration is invalid\n"
        " 3  if this launcher process forking is failed\n"
		);
	return;
}
