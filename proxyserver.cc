#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdlib>

//#define PORT "8174"
#define BACKLOG 10
#define MAXDATASIZE 8096
#define MAXBUFFERSIZE 32768


void sigchld_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while(waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

class Client
{
 public:
  Client() = default;
  //*****OBS close sockfd vid destruction??
  ~Client() {close(sockfd);}

  int createSocketandConnect(const char*);
  int sendReq(const char*, int);
  int receiveFirst(char*, int&);
  int receive(char*, int&);

 private:
  bool checkForbiddenWords(std::string&);

  int sockfd;
  struct addrinfo hints, *p;
};

int Client::createSocketandConnect(const char* chost)
/*
create socket and connect to the given host)
*/
{
  struct addrinfo *servinfo;
  int rv;
  //this part mostly from Beej's guide
  //set up socket and connect
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if ((rv = getaddrinfo(chost, "80", &hints, &servinfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }
  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next)
    {
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
			   p->ai_protocol)) == -1) {
	perror("client: socket");
	continue;
      }
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
	{
	  close(sockfd);
	  perror("client: connect");
	  continue;
        }
      break;
    }
  if (p == NULL)
    {
      fprintf(stderr, "client: failed to connect\n");
      return 2;
    }
  freeaddrinfo(servinfo);
  return 0;
}

int Client::sendReq(const char* req, int recvB){
/*
Send incoming request forward to host
*/
  int sentB{};
  do
    {
      sentB = sentB + send(sockfd, req+sentB, recvB-sentB, 0);
    } while(sentB < recvB);

  return sentB;
}

int Client::receiveFirst(char* cbuf, int& recvLength){
/*
Receive first part from webserver, if text check for forbidden words
*/
  int stringStart{}, stringEnd{}, recvB{};
  std::string tempString{};
  char tempBuf[MAXDATASIZE];
  //receive data from remote server
  recvLength = recv(sockfd, cbuf, MAXDATASIZE-1, 0);
  recvB = recvLength;
  //handle issues if ==-1 or ==0

  //If status == 200(OK) check content, else just let pass
  std::string bufString(cbuf);
  std::cout << bufString << std::endl;
  std::transform(bufString.begin(),bufString.end(), bufString.begin(), ::tolower);
  tempString = bufString.substr(9, 3);
  std::cout << std::endl << "tempString status: " << tempString << std::endl;
  if(tempString != "200")
    return 0;
  else if (bufString.find("content-encoding") != -1)
    {

      return 0;
    }
  else
    {
      stringStart = bufString.find("content-type:");
      if(stringStart != -1)
	{
	  stringEnd = bufString.find("\r", stringStart);
	  tempString.assign(bufString, stringStart+14, stringEnd-stringStart-14);
	  //if content is text/plain, receive all and check for forbidden words, else return
	  if(tempString=="text/plain"|| tempString =="text/html")
	    {
	      while(recvB > 0)
		{
		  memset(tempBuf, '\0', MAXDATASIZE);
		  recvB = recv(sockfd, tempBuf, MAXDATASIZE-1,0);
		  recvLength += recvB;
		  strcat(cbuf, tempBuf);
		}
	      std::string textBuf(cbuf);
	      if(checkForbiddenWords(textBuf))
		{
		  cbuf = const_cast<char*>("HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error2.html\r\n\r\n");
		  std::cout << "forbidden, redirect return: " << cbuf << std::endl;
		  recvLength = strlen(cbuf);
		}
	    }
	}
    }
  return 0;
}

//recieve parts of data
int Client::receive(char* cbuf, int& recvLength){
  if((recvLength = recv(sockfd, cbuf, MAXDATASIZE-1, 0)) > 0)
    return 1;
  //std::cout << "Recievedbytes after first: " << recbyt << std::endl;
  return 0;
}

bool Client::checkForbiddenWords(std::string& input)
/*
checks the given string for forbidden words
*/
{
  std::transform(input.begin(),input.end(), input.begin(), ::tolower);
  std::string inappropriateWords[] = {"spongebob","britney spears","paris hilton","norrköping","norrkoping","norrk&ouml;ping"};
  for (int i=0; i<=5; i++) {
    if (input.find(inappropriateWords[i])!=std::string::npos)
      {
	std::cout << "found inappropriate word content: " << inappropriateWords[i] << std::endl;
	return true;
      }
  }
  return false;
}

void get_host(const char*, char*);
std::string get_url(const char*);
void set_connection_close(char*);
int is_inappropriate(std::string);
void userInputPort(char*);


int main(){

  int sockfd{}, new_fd{}, status{}, yes=1;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  struct sigaction sa;
  char port[6];

  //Setup hints(make sure it's empty)
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;


  //Get portnumber from user
  userInputPort(port);

  //Fill upp servinfo
  status = getaddrinfo(NULL, port, &hints, &servinfo);
  //*****KOLLA OM STATUS != 0, då felhantering

  for(p = servinfo; p != NULL; p = p->ai_next)
    {
      //Create socket
      sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      //******Felhantering om sockfd ==-1

      //Reuse address
      if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)==-1)
	{
	  std::cout << "error 2" <<std::endl;
	}
      //*******Felhantering om ==-1

      //Bind port to socket
      if(bind(sockfd, p->ai_addr, p->ai_addrlen)==-1)
	{
	  std::cout << "error 3" <<std::endl;
	}
      //******Felhantering om ==-1
    }

  freeaddrinfo(servinfo); // all done with this structure
  //******handle error if p==NULL (failed to bind)
  listen(sockfd,BACKLOG);
  //******handle error listen==-1


  //from beej
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
      perror("sigaction");
      exit(1);
    }


  std::cout << "Waiting for connections..." << std::endl;

  while(1)
    {
      char host[MAXDATASIZE];
      char cbuf[MAXBUFFERSIZE];
      int sentB{}, recvB{}, sentLength{}, recvType{}, recvLength{};

      //sin_size = sizeof their_addr;
      new_fd = accept(sockfd,(struct sockaddr *)&their_addr,&sin_size);
      //error handeling om new_fd==-1

      if(!fork())
	{
	  close(sockfd);

	  //Receive get-request from browser, until we recieved all of it
	  //***ryms hela i maxdatasize? annars ändra på recbuf initiering
	  recvB = recv(new_fd,cbuf, MAXDATASIZE-1, 0);
          //******error handeling if ==-1
	  if(recvB ==0)
	    exit(0);

	  //Get host from get-request
	  get_host(cbuf, host);
	  //Get url from GET-request
	  std::string url = get_url(cbuf);
	  std::cout << "testing get_url: " << url << std::endl;
	  //Check if url contain bad words
	  if(is_inappropriate(url) == 1)
	    {
	      //if url contain bad words => user is redirected to the webpage stated after "Location: "
	      char* redirectMsg = const_cast<char*>("HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html\r\n\r\n");
	      send(new_fd, redirectMsg, MAXDATASIZE, 0);
	      close(new_fd);
	      exit(0);
	    }

	  //if url is free from bad words. Connection type of the get-request from the browser is set to close.
	  //i.e we ask the server to close the connection once it has sent the whole requested package.
	  set_connection_close(cbuf);
	  std::cout << "testing set_connection_close: " << cbuf << std::endl;

	  //Initiate proxy's client socket which will handle communication with the remote server.
	  Client client{};
	  client.createSocketandConnect(host);
	  if( client.sendReq(cbuf, recvB) == -1)//Send getrequest to remote server
	    {
	      perror("proxy-client: send ");
	      exit(3);
	    }
	  //recieve first response and forward it to browser
	  memset(cbuf, '\0', MAXBUFFERSIZE);
	  sentB = 0;
	  client.receiveFirst(cbuf, recvLength);
	  do
	    {
	      sentB = sentB + send(new_fd, cbuf+sentB, recvLength-sentB, 0);
	      std::cout << "sentB: " << sentB << " recvLength: " << recvLength << std::endl;
	    } while(sentB < recvLength);
	  memset(cbuf, '\0', MAXBUFFERSIZE);
	  //if more data is waiting to be received from host server, get it and send to browser
	  while(client.receive(cbuf, recvLength) > 0)
	    {
	      sentB = 0;
	      do
		{
		  sentB = sentB + send(new_fd, cbuf+sentB, recvLength-sentB, 0);
		  //sentLength = sentLength+sentB;
		} while(sentB < recvLength);
	      memset(cbuf, '\0', MAXBUFFERSIZE);
	    }
	  close(new_fd);
	  exit(0);
	}
    }
}

void get_host(const char* getRequest, char* host) {
  /* this function extracts the host name from the getRequest. */
  std::string getString(getRequest);
  std::string searchString = "Host: ";
  std::size_t hostStart = getString.find(searchString) + searchString.length();
  std::size_t hostEnd = getString.find("\r\n", hostStart);
  if(hostStart == 1)
    std::cout << "no Host found" << std::endl;
  std::size_t length = getString.copy(host, hostEnd-hostStart, hostStart);
  host[length] = '\0';
}

std::string get_url(const char* getRequest) {
  std::string getreq = std::string(getRequest);
  std::size_t end = getreq.find("HTTP/1");
  return getreq.substr(4,end-5);
}

int is_inappropriate(std::string input) {
  /* checks input string for inappropriate words. Returns 1 if bad word is present and
  0 if not. */
  std::transform(input.begin(),input.end(), input.begin(), ::tolower);
  std::string inappropriateWords[] = {"spongebob","britney spears","paris hilton","norrköping","norrkoping","norrk&ouml;ping"};
  for (int i=0; i<=5; i++) {
    //std::transform(inappropriateWords[i].begin(),inappropriateWords[i].end(), inappropriateWords[i].begin(), ::tolower)
    if (input.find(inappropriateWords[i])!=std::string::npos) {
    std::cout << "found inappropriate word content: " << inappropriateWords[i] << std::endl;
    return 1;
    }
  }
  if (input.find("paris")!=std::string::npos and input.find("hilton")!=std::string::npos){
    std::cout << "found inappropriate word content: Paris Hilton " << std::endl;
    return 1;
  }
  else if (input.find("britney")!=std::string::npos and input.find("spears")!=std::string::npos) {
    std::cout << "found inappropriate word content: Britney spears " << std::endl;
    return 1;
}
   return 0;
}

void set_connection_close(char* recbuf) {
  /* this function replaces connection keep-alive with connection close
  if existant in rec buf else it does nothing. */
  std::string buff = std::string(recbuf);
  std::size_t start = buff.find("Connection: keep-alive");
  if(start != -1)
    {
      buff.replace(start+12,10,"close");
      buff.push_back('\0');
    }
  buff.copy(recbuf,buff.length(),0);
}

void userInputPort(char * port){
  /*get a valid port number from user*/
  bool portValid{false};
  char *pEnd;
  do
    {
      int j{};
      int k{};
      std::cout << "Please enter a port number between 1025 and 65535: " ;
      std::cin.get(port,7);
      j = std::cin.gcount();
      std::cin.ignore(9999, '\n');
      if(j < 6)
	{
	  for(int i = 0; i < j ; i++)
	    {
	      if(!isdigit(port[i]))
		  k++;
	    }

	  if(k == 0)
	    {
	      j = strtol(port, &pEnd, 10);
	      if(j < 1025)
		std::cout << "Too small" << std::endl;
	      else if(j > 65535)
		std::cout << "Too big" << std::endl;
	      else
		portValid =true;
	    }
	  else
	    std::cout << "Only numbers are allowed" << std::endl;
	}
      else
	std::cout << "Too long" << std::endl;
    }while(portValid == false);
}
