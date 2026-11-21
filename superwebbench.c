/*
 * Modified by XuTongle 2012-01-20
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   superwebbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, pthread failed
 *    4 - resourse limits
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <rpc/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include "socket.h"


#define PROGRAM_VERSION "0.1"
#define METHOD_GET	0
#define METHOD_HEAD	1
#define METHOD_OPTIONS	2
#define METHOD_TRACE	3
#define HTTP_09		0
#define HTTP_10		1
#define HTTP_11		2

#define BUF_SIZE	1024
#define URL_SIZE	1500
#define REQUEST_SIZE	2048
#define START_SLEEP	10000

#define DEFAULT_PROXY	0
#define DEFAULT_CLIENT	1
#define DEFAULT_TIME	30
#define DEFAULT_FORCE	0
#define DEFAULT_PORT	80
#define DEFAULT_METHOD	METHOD_GET

#define DEFAULT_FORE_RELOAD	0
#define DEFAULT_STACK_SIZE	32768
#define DEFAULT_HTTP_VER	HTTP_11
/* globals */
struct parameter{
	int force;
	int force_reload;
	int proxy;
	int http_ver;
	int method;
	int port;
	int clients;
	int bench_time;
	char	*host;
	char	*request;
};
struct thread_arg{
	char *host;
	char *request;
	int  force;	
	int  port;
	int  http_ver;
	unsigned long speed;
	unsigned long failed;
	unsigned long httperr;
	unsigned long bytes;
};
static char const * const gs_http_method[] =
{
	"GET","HEAD","OPTION","TRACE",
};

/* prototypes */
static void *bench_thread(void *para);
static int bench(struct parameter *para);
static void build_request(const char *url, struct parameter *para);
static int  http_response_check(const char *response);
static int parse_opt(int argc, char *argv[], struct parameter *para);
static int resource_set(int clients);


static int 
resource_set(int clients)
{
	struct rlimit lim;
	static const int used_lim = 128;
	
	getrlimit(RLIMIT_NOFILE, &lim);//get file des limit
	if (lim.rlim_cur < (rlim_t)(used_lim + clients))
	{	//set file des limit
		lim.rlim_cur = used_lim + clients;
		lim.rlim_max = used_lim + clients;
		if (setrlimit(RLIMIT_NOFILE, &lim))
			return -1;
	}
	
	getrlimit(RLIMIT_NPROC, &lim);//get file des limit
	if (lim.rlim_cur < (rlim_t)(used_lim + clients))
	{	//set file des limit
		lim.rlim_cur = used_lim + clients;
		lim.rlim_max = used_lim + clients;
		if (setrlimit(RLIMIT_NPROC, &lim))
			return -2;
	}
	
	return 0;
}
/* NOT USE
static void
exit_handler(int signal)
{
        if (signal == SIGUSR1)
		pthread_exit(NULL);
}
*/
static void
usage(void)
{
        fprintf(stderr,
                "superwebbench [option]... URL\n"
		"  -f|--force                      Don't wait for reply from server.\n"
                "  -r|--reload                     Send reload request - Pragma: no-cache.\n"
                "  -t|--time <sec>                 Run benchmark for <sec> seconds. Default 30.\n"
                "  -p|--proxy <server:port>        Use proxy server for request.\n"
                "  -c|--clients <n>                Run <n> HTTP clients at once. Default one.\n"
                "  -9|--http09                     Use HTTP/0.9 style requests.\n"
                "  -1|--http10                     Use HTTP/1.0 protocol.\n"
                "  -2|--http11                     Use HTTP/1.1 protocol.\n"
                "  --get                           Use GET request method.\n"
                "  --head                          Use HEAD request method.\n"
                "  --options                       Use OPTIONS request method.\n"
                "  --trace                         Use TRACE request method.\n"
                "  -?|-h|--help                    This information.\n"
                "  -V|--version                    Display program version.\n"
               );
};   
static int 
parse_opt(int argc, char *argv[], struct parameter *para)
{
	const struct option long_options[] = {
		{"force",	no_argument,		&para->force,		1},
		{"reload",	no_argument,		&para->force_reload,	1},
		{"get",		no_argument,		&para->method,	METHOD_GET},
		{"head",	no_argument,		&para->method,	METHOD_HEAD},
		{"options",	no_argument,		&para->method,	METHOD_OPTIONS},
		{"trace",	no_argument,		&para->method,	METHOD_TRACE},
		{"help",	no_argument,		NULL,		'?'},
		{"http09",	no_argument,		NULL,		'9'},
		{"http10",	no_argument,		NULL,		'1'},
		{"http11",	no_argument,		NULL,		'2'},
		{"version",	no_argument,		NULL,		'V'},
		{"time",	required_argument,	NULL,		't'},
		{"proxy",	required_argument,	NULL,		'p'},
		{"clients",	required_argument,	NULL,		'c'},
		{NULL,		0,			NULL,		0}
	};
        
	int opt = 0;
        int options_index = 0;
        char *tmp = NULL;

        if (argc == 1) {
                usage();
                return 0;
        }

        while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF ) {
                switch(opt) {
			case  0 :
				break;
			case 'f':
				para->force = 1;
				break;
			case 'r':
				para->force_reload = 1;
				break;
			case '9':
				para->http_ver = HTTP_09;
				break;
			case '1':
				para->http_ver = HTTP_10;
				break;
			case '2':
				para->http_ver = HTTP_11;
        	                break;
	                case 'V':
	                        printf(PROGRAM_VERSION"\n");
	                        return 0;
	                case 't':
	                        para->bench_time=atoi(optarg);
	                        break;
	                case 'p':
                	        para->proxy=1;
	                        /* proxy server parsing server:port */
        	                tmp=strrchr(optarg,':');
                        	if (tmp==NULL) {
	                                break;
	                        }
        	                if(tmp==optarg) {
                	                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                        	        return -1;
	                        }
        	                if (tmp==optarg+strlen(optarg)-1) {
                	                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                        	        return -1;
	                        }
        	                *tmp='\0';
                	        if ( (para->port=atoi(tmp+1) < 0)) {
					fprintf(stderr,"Error in option --proxy %s Port number is invaild.\n",optarg);
					return -1;
				}
                        	break;
	                case ':':
        	        case 'h':
                	case '?':
	                        usage();
        	                return 0;
                	        break;
	                case 'c':
        	                para->clients=atoi(optarg);
                	        break;
		}
	}

        if (optind==argc) {
                fprintf(stderr,"superwebbench: Missing URL!\n");
                usage();
                return 2;
        }

        if (para->clients <= 0)
		para->clients = DEFAULT_CLIENT;
        if (para->bench_time <= 0) 
		para->bench_time = DEFAULT_TIME;
	return 1;
}

int
main(int argc, char *argv[])
{
	int ret;
	char host[MAXHOSTNAMELEN];
	char request[REQUEST_SIZE];
	struct parameter para =
	{
		DEFAULT_FORCE, DEFAULT_FORE_RELOAD,
		DEFAULT_PROXY, DEFAULT_HTTP_VER,
		DEFAULT_METHOD,DEFAULT_PORT,
		DEFAULT_CLIENT,DEFAULT_TIME,
		host,	request,
	};
        // parse options
        if  ( (ret=parse_opt(argc, argv, &para)) <=0 )
		return  ret<0 ? 2 : 0;
        
	build_request(argv[argc-1], &para);
	/* Copyright */
        printf(	"\nSuperWebBench - Advanced Simple Web Benchmark "PROGRAM_VERSION"\n"
                "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
		"Modified By Davelv 2011-11-03\n"
               );
	//check resourse limits
	if ( (ret=resource_set(para.clients)) <0 )
	{
		fprintf(stderr, "\nSet %s  limit failed. \n"
			"Try less clients or use higher authority or set \"ulimit -n -u\" manualy\n",
			ret==-1 ? "NOFILE" : "NPROC");
		return 4;
	}
        /* print bench info */
        printf("\nBenchmarking:%s %s", gs_http_method[para.method], argv[argc-1]);
        switch(para.http_ver) {
        case 0:
                puts(" (using HTTP/0.9)"); break;
	case 1:
		puts(" (using HTTP/1.0)"); break;
        case 2:
                puts(" (using HTTP/1.1)"); break;
        }
        /* check avaibility of target server */
        ret = Socket(para.host, para.port);
        if (ret<0) {
                fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
                return 1;
        }   
        close(ret);
        return bench(&para);
}

void 
build_request(const char *url, struct parameter *para)
{
        char tmp[10];
        int i;

        bzero(para->host,MAXHOSTNAMELEN);
        bzero(para->request,REQUEST_SIZE);

        if (para->force_reload && para->proxy && para->http_ver<HTTP_10) para->http_ver=HTTP_10;
        if (para->method==METHOD_HEAD && para->http_ver<HTTP_10) para->http_ver=HTTP_10;
        if (para->method==METHOD_OPTIONS && para->http_ver<HTTP_11) para->http_ver=HTTP_11;
        if (para->method==METHOD_TRACE && para->http_ver<HTTP_11) para->http_ver=HTTP_11;

        strcpy(para->request, gs_http_method[para->method]);
	strcat(para->request," ");

        if (NULL==strstr(url,"://")) {
                fprintf(stderr, "\n%s: is not a valid URL.\n",url);
                exit(2);
        }
        if (strlen(url)>URL_SIZE) {
                fprintf(stderr,"URL is too long.\n");
                exit(2);
        }
        if (!para->proxy)
                if (0!=strncasecmp("http://",url,7)) {
                        fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
                        exit(2);
                }
        /* protocol/host delimiter */
        i=strstr(url,"://")-url+3;
        /* printf("%d\n",i); */

        if (strchr(url+i,'/')==NULL) {
                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                exit(2);
        }
        if (para->proxy) {
		strcat(para->request,url);
	} else {
                /* get port from hostname */
                if (index(url+i,':')!=NULL &&
                                index(url+i,':')<index(url+i,'/')) {
                        strncpy(para->host,url+i,strchr(url+i,':')-url-i);
                        bzero(tmp,10);
                        strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
                        /* printf("tmp=%s\n",tmp); */
                        para->port=atoi(tmp);
                        if (para->port==0) para->port=DEFAULT_PORT;
                	} 
		else {
                        strncpy(para->host,url+i,strcspn(url+i,"/"));
                }
                strcat(para->request+strlen(para->request),url+i+strcspn(url+i,"/"));
        }
        if (para->http_ver==1)
                strcat(para->request," HTTP/1.0");
        else if (para->http_ver==2)
                strcat(para->request," HTTP/1.1");
        strcat(para->request,"\r\n");
        if (para->http_ver>0)
                strcat(para->request,"User-Agent: SuperWebBench "PROGRAM_VERSION"\r\n");
        if (!para->proxy && para->http_ver>0) {
                strcat(para->request,"Host: ");
                strcat(para->request,para->host);
                strcat(para->request,"\r\n");
        }
        if (para->force_reload && para->proxy) {
                strcat(para->request,"Pragma: no-cache\r\n");
        }
        if (para->http_ver>1)
                strcat(para->request,"Connection: close\r\n");
        /* add empty line at end */
        if (para->http_ver>0) strcat(para->request,"\r\n");
        // printf("Req=%s\n",para->request);
}

/* vraci system rc error kod */
static int 
bench(struct parameter *para)
{
	int i;
	unsigned long long speed=0;
	unsigned long long failed=0;
	unsigned long long httperr=0;
	unsigned long long bytes=0;
	// set thread args
	struct thread_arg *args = malloc (sizeof(struct thread_arg)*para->clients);
	if (args == NULL){
                fprintf(stderr,"Malloc thread args failed. Aborting benchmark.\n");
                return 3;
        }	
	memset(args, 0, sizeof(args));
	for (i=0; i<para->clients; i++)
	{
		args[i].host = para->host;
		args[i].port = para->port;
		args[i].request = para->request;
		args[i].http_ver = para->http_ver;
		args[i].force = para->force;
	}
	//theads
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if (pthread_attr_setstacksize(&attr, DEFAULT_STACK_SIZE)){;//stack size 32K
		fprintf(stderr,"Set stack size failed . Aborting benchmark.\n");
		return 3;
	}
	pthread_t *threads= malloc(sizeof(pthread_t)*(para->clients));
	if (threads == NULL){
		fprintf(stderr,"Malloc threads failed. Aborting benchmark.\n");
		return 3;
	}
        printf("\n%d clients, running %d sec",para->clients, para->bench_time);
        if (para->force) printf(", early socket close");
        if (para->proxy) printf(", via proxy server %s:%d",para->host,para->port);
        if (para->force_reload) printf(", forcing reload");
        printf(".\n");
	//new thread
	for (i=0; i<para->clients; i++)
	{
		int ret_p =pthread_create(threads+i, &attr, bench_thread, args+i);
		if (ret_p)
		{
			printf("pthread create error %d on %d\n", ret_p, i);
			i-- , para->clients--;
			//exit (-1);
		}
	}
	//main thread sleep
	sleep(para->bench_time);
	//calc the result
	for (i=0; i<para->clients; i++)
	{
		speed += args[i].speed;
		httperr += args[i].httperr;
		failed += args[i].failed;
		bytes += args[i].bytes;
	}
        printf("\nSpeed=%llu pages/sec, %llu bytes/sec.\nRequests: %llu ok, %llu http error, %llu failed.\n",
               (speed+httperr+failed)/para->bench_time, bytes/para->bench_time , speed, httperr, failed);
	return 0;
}

#define ERR_PROCESS(a, b, c) {a++; close(b); goto c;}
void *
bench_thread(void *arg)
{
	usleep(START_SLEEP);
	struct thread_arg *p_arg = (struct thread_arg *)arg;
        int rlen = strlen(p_arg->request);
        char buf[BUF_SIZE];
        int s,i,cnt;
        
	while(1) {
outloop:	s=Socket(p_arg->host,p_arg->port);
                if (s<0) {
                        p_arg->failed++;
                        continue;
                }
                if (rlen!=write(s,p_arg->request,rlen)) {
                        ERR_PROCESS (p_arg->failed, s, outloop);
                }
                if (p_arg->http_ver==HTTP_09)
                        if (shutdown(s,1)) {
                        	ERR_PROCESS (p_arg->failed, s, outloop);
                        }
                if (p_arg->force == 0) {
                        /* read all available data from socket */
			cnt = 0;
                        while (1) {
				i = read(s, buf, BUF_SIZE);
                                if (i < 0) {
                        		ERR_PROCESS (p_arg->failed, s, outloop);
				} 
				else if (i == 0) {
					if (cnt == 0) {//first read
                        			ERR_PROCESS (p_arg->httperr, s, outloop);
					} 
					break;
				}
                                else{	//check http status
                                        p_arg->bytes += i;
					if ((cnt++ == 0)&&(http_response_check(buf)<0)) {
                        			ERR_PROCESS (p_arg->httperr, s, outloop);
					}
				}
                        }
                }
                if (close(s)) {
                        p_arg->failed++;
                        continue;
                }
                p_arg->speed++;
        }
	//never return  from here
	return NULL;
}
int
http_response_check(const char *response)
{
	int status=0;
	float version;
	//HTTP/
	response+=5;
	if ( (sscanf(response, "%f %d",&version,&status)==2) &&
		(status>=200 && status <300))
		return 0;

	else	return -1;

}
