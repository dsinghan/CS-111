//NAME: Dhruv Singhania
//EMAIL: singhania_dhruv@yahoo.com
//ID: 105125631

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <netdb.h>
#include <poll.h>
#include "zlib.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

int ret; //return value for system calls
int port_flag = 0; //mandatory flag set by --port
int log_flag = 0; //flag set by --log
int compress_flag = 0; //flag set by --compress
int port_no; //port num set by --port
int logfd; //log file descriptor set by --log
struct termios termios_get; //original terminal settings
int sockfd; //sock file descriptor set by sock()
z_stream send_stream, rec_stream; //compression streams set by --compress

void error(char *msg); //error handling
void get_options(int argc, char *argv[]); //performs getopt_long
void set_terminal(); //sets terminal to non-canonical input mode with no echo
void restore_terminal(); //restores terminal to original settings
void create_socket(); //opens connection to the server
void parent_process();
void stdin_input();
void server_input();

int main(int argc, char *argv[])
{
  get_options(argc, argv);
  set_terminal();
  create_socket();
  parent_process();
  exit(0);
}

void error(char *msg)
{
  perror(msg);
  exit(1);
}

void get_options(int argc, char *argv[])
{
  int c; //holds value of getopt_long
  while(1) {
    static struct option long_options[] = //holds key for options
      {
       {"port", required_argument, 0, 'p'},
       {"log", required_argument, 0, 'l'},
       {"compress", no_argument, 0, 'c'},
       {0, 0, 0, 0}
      };
    int option_index = 0;
    c = getopt_long(argc, argv, "", long_options, &option_index);
    if(c == -1) //breaks when there are no more options
      {
	break;
      }
    switch(c)
      {
      case 'p': //--port case
	port_flag = 1;
	port_no = atoi(optarg);
	break;
      case 'l': //--log case
	log_flag = 1;
	logfd = open(optarg, O_WRONLY | O_CREAT, 0666);
	if(logfd < 0) //error handling
	  {
	    error("Error with open()");
	  }
	break;
      case 'c': //--compress case
	compress_flag = 1;
	send_stream.zalloc = Z_NULL;
	send_stream.zfree = Z_NULL;
	send_stream.opaque = Z_NULL;
	ret = deflateInit(&send_stream, Z_DEFAULT_COMPRESSION);
	if(ret != Z_OK)
	  {
	    error("Error with deflateInit()");
	  }
	rec_stream.zalloc = Z_NULL;
	rec_stream.zfree = Z_NULL;
	rec_stream.opaque = Z_NULL;
	ret = inflateInit(&rec_stream);
	if(ret != Z_OK)
	  {
	    error("Error with inflateInit()");
	  }
	break;
      case '?': //unrecognized option
	fprintf(stderr, "usage: lab1b-client --port='num' [--log='file'] [--compress]");
	exit(1);
      default:
	abort();
      }
  }
  if(port_flag == 0) //makes sure --port was used
    {
      fprintf(stderr, "usage: lab1b-client --port='num' [--log='file'] [--compress]");
      exit(1);
    }
}

void set_terminal()
{
  struct termios termios_set; //temp that stores new terminal settings
  ret = tcgetattr(0, &termios_get);
  if(ret < 0) //error handling
    {
      error("Error with tcgetattr()");
    }
  termios_set = termios_get;
  termios_set.c_iflag = ISTRIP; //terminal changes outlined in project
  termios_set.c_oflag = 0;
  termios_set.c_lflag = 0;
  termios_set.c_cc[VMIN] = 1;
  termios_set.c_cc[VTIME] = 0;
  ret = tcsetattr(0, TCSANOW, &termios_set);
  if(ret < 0) //error handling
    {
      error("Error with tcsetattr()");
    }
  atexit(restore_terminal); //calls restore_terminal before every exit
}

void restore_terminal()
{
  ret = tcsetattr(0, TCSANOW, &termios_get);
  if(ret < 0) //error handling
    {
      error("Error with tcsetattr()");
    }
}

void create_socket()
{
  struct sockaddr_in serv_addr;
  struct hostent *server;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) //error handling
    {
      error("Error with socket()");
    }
  server = gethostbyname("localhost");
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(port_no);
  ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if(ret < 0) //error handling
    {
      error("Error with connect()");
    }
}

void parent_process()
{
  struct pollfd fds[] =
    {
     {STDIN_FILENO, POLLIN, 0},
     {sockfd, POLLIN, 0}
    };
  while(1)
    {
      ret = poll(fds, 2, 0);
      if(ret < 0) //error handling
	{
	  error("Error with poll()");
	}
      if(fds[0].revents & POLLIN)
	{
	  stdin_input();
	}
      else if(fds[0].revents & POLLERR)
	{
	  error("Error with poll()");
	}
      else if(fds[1].revents & POLLIN)
	{
	  server_input();
	}
      else if(fds[1].revents & POLLERR || fds[1].revents & POLLHUP)
	{
	  exit(0);
	}
    }
  if(compress_flag)
    {
      inflateEnd(&rec_stream);
      deflateEnd(&send_stream);
    }
}

void stdin_input()
{
  char buf[256], temp[2];
  int bytes_read = read(STDIN_FILENO, buf, 256);
  if(bytes_read < 0) //error handling
    {
      error("Error with read()");
    }
  for(int i = 0; i < bytes_read; i++)
    {
      switch(buf[i])
	{
	case 10: //<lf> case
	case 13: //<cr> case
	  temp[0] = 13;
	  temp[1] = 10;
	  write(STDOUT_FILENO, temp, 2*sizeof(char));
	  break;
	default:
	  write(STDOUT_FILENO, &buf[i], sizeof(char));
	  break;
	}
    }
  if(compress_flag)
    {
      char compress_buf[256];
      send_stream.avail_in = bytes_read;
      send_stream.next_in = (Bytef *)buf;
      send_stream.avail_out = 256;
      send_stream.next_out = (Bytef *)compress_buf;
      do
	{
	  ret = deflate(&send_stream, Z_SYNC_FLUSH);
	  if(ret == Z_STREAM_ERROR)
	    {
	      error("Error with deflate()");
	    }
	} while(send_stream.avail_in > 0);
      write(sockfd, compress_buf, 256 - send_stream.avail_out);
      if(log_flag)
	{
	  dprintf(logfd, "SENT %d bytes: %s\r\n", 256 - send_stream.avail_out, compress_buf);
	}
    }
  else
    {
      write(sockfd, buf, bytes_read);
      if(log_flag)
	{
	  dprintf(logfd, "SENT %d bytes: %s\r\n", bytes_read, buf);
	}
    }
}

void server_input()
{
  char buf[256];
  int bytes_read = read(sockfd, buf, 256);
  if(bytes_read < 0) //error handling
    {
      error("Error with read()");
    }
  if(bytes_read == 0)
    {
      exit(1);
    }
  if(compress_flag)
    {
      char compress_buf[256];
      rec_stream.avail_in = bytes_read;
      rec_stream.next_in = (Bytef *)buf;
      rec_stream.avail_out = 256;
      rec_stream.next_out = (Bytef *)compress_buf;
      do
	{
	  inflate(&rec_stream, Z_SYNC_FLUSH);
	} while(rec_stream.avail_in > 0);
      write(STDOUT_FILENO, compress_buf, 256 - rec_stream.avail_out);
    }
  else
    {
      write(STDOUT_FILENO, buf, bytes_read);
    }
  if(log_flag)
    {
      dprintf(logfd, "RECEIVED %d bytes: %s\r\n", bytes_read, buf);
    }
}
