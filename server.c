#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <libgen.h>

int portno;
char *map;
int fd;
int result;
long int sendSize;
int currSeq=0;
struct stat finfo;
int numPackets;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

void dostuff (int sockfd) {
    char buffer[256];
    char fileName[256];
    char* bname;
    bzero(buffer,256);
    int n;
    
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
	if (n < 0)
		error("ERROR reading from socket");
	printf("File Name: %s\n",buffer);
    bname=basename(buffer);
    sprintf(fileName,"%s",bname);
    
    bzero(buffer,256);
    sprintf(buffer,"Received Name");
    n = write(sockfd,buffer,strlen(buffer));
	if (n < 0)
		error("ERROR writing to socket");
    
    bzero(buffer,256);
	n = read(sockfd,buffer,255);
	if (n < 0)
		error("ERROR reading from socket");
	printf("File Size: %s\n",buffer);
    long int fileSize=atoi(buffer);
    
    //open file for writing
    char *map;
    int fd;
    int result;
    
    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    if (fd == -1) {
        error("open");
    }
    result = lseek(fd, fileSize-1, SEEK_SET);
    if (result == -1) {
        close(fd);
        error("lseek");
    }
    
    result = write(fd, "", 1);
    if (result != 1) {
        close(fd);
        error("write");
    }
    map = (char*)mmap(0, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        error("map");
    }
    //finished mmap setup
    
    char recvData[1472];
    char data[1464];
    long int recvSize=0;
    long int totPkts=(fileSize/1464);
    totPkts = totPkts +1;
    char pktsRcvd[1000000]={};
    int nackCounter=0;

    //udp setup
	int sockUDP, length;
	socklen_t fromlen;
	struct sockaddr_in server;
	struct sockaddr_in from;
	char buf[1024];
    
	sockUDP=socket(AF_INET, SOCK_DGRAM, 0);
	if (sockUDP < 0) error("Opening socket");
    
    int buffSize = 32*1024*1024;
    if(setsockopt(sockUDP, SOL_SOCKET, SO_SNDBUF, &buffSize, sizeof buffSize)<0){
        error("buff change error");
    }
    if(setsockopt(sockUDP, SOL_SOCKET, SO_RCVBUF, &buffSize, sizeof buffSize)<0){
        error("buff change error");
    }
    
	length = sizeof(server);
	bzero(&server,length);
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(portno+1);
	if (bind(sockUDP,(struct sockaddr *)&server,length)<0)
		error("binding");
	fromlen = sizeof(struct sockaddr_in);
    
    //parameters for select
    struct timespec tv;
    fd_set readfds;
    //set the timeout values
    tv.tv_sec = 0;
    tv.tv_nsec = 100000000;
    //sockfd has to be monitored for timeout
    FD_ZERO(&readfds);
    FD_SET(sockUDP, &readfds);
    
    while(1) {
        
        //wait fro recvfrom or timeout
        FD_ZERO(&readfds);
        FD_SET(sockUDP, &readfds);
        //printf("On top of select\n");
        int setRet = pselect(sockUDP+1, &readfds, NULL, NULL, &tv, NULL);
        
        //if (FD_ISSET(sockUDP, &readfds)) {
        if(setRet > 0 ){
            n = recvfrom(sockUDP,recvData,1472,0,(struct sockaddr *)&from, (socklen_t*)&length);
            if (n < 0) error("recvfrom");
            char seqC[8];
            memcpy(seqC,recvData,8);
            int seq=atoi(seqC);
            if(pktsRcvd[seq]==0){
                printf("Sequence Received %d: %d\n",numPackets ,seq);
                numPackets++;
                pktsRcvd[seq]=1;
                memcpy(data,recvData+8,sizeof(recvData)-8);
                //printf("seq num: %d\n", seq);
                
                //fseek(f,seq*1024,SEEK_SET);
                //fwrite(data,sizeof(data),1,f);
                if (seq < totPkts-1){
                    memcpy(&map[seq*1464],data,sizeof(data));
                    recvSize+=sizeof(data);
                }
                else{
                    printf("Writing last packet\n");
                    memcpy(&map[seq*1464],data,fileSize-((seq)*1464));
                    recvSize+=(fileSize-((seq)*1464));
                }
                //printf("Number of bytes: %ld\n",sizeof(data));
                if(recvSize>=fileSize)
                {
                    printf("File received\n");
                    printf("Filesize: %ld\n", fileSize);
                    printf("Recv size: %ld\n", recvSize);
                    //printf("Total Nack packets: %d\n",nackCounter);
                    
                    printf("Total Nack packets: %d\n",nackCounter);
                    //fclose(f);
                    if (munmap(map, fileSize) == -1) {
                        error("munmap");
                        
                    }
                    close(fd);
                    
                    int final[1];
                    final[0] = -1;
                    //sprintf(final,"Donexxx");
                    n = sendto(sockUDP,final,sizeof(final),0,(struct sockaddr *)&from, length);
                    if (n < 0) error("ERROR sending final packet");
                    n = sendto(sockUDP,final,sizeof(final),0,(struct sockaddr *)&from, length);
                    if (n < 0) error("ERROR sending final packet");
                    n = sendto(sockUDP,final,sizeof(final),0,(struct sockaddr *)&from, length);
                    if (n < 0) error("ERROR sending final packet");
                    break;
                }
            }
        }
        else {
            printf("Number of bytes rxed: %ld\n",recvSize);
            //printf("Entering timeout loop");
            int control[1470];
            long int reqs=0;
            int j;
            int num=0;
            //bzero(control,512);
            for(j=0;j<totPkts;j++) {
                if(pktsRcvd[j]==0) {
                    //sprintf(control,"%8d",j);
                    //printf("Sending NACK:%d\n",j);
                    control[num] = j;
                    num++;
                    
                    reqs++;
                    //if(reqs==256 || (recvSize > (fileSize-(256*1024)))) {
                    if(reqs==1470) {
                        nackCounter++;
                        num=0;
                        /*n = write(sockfd,control,sizeof(control));
                         if (n < 0)
                         error("ERROR writing to socket");
                         */
                        n = sendto(sockUDP,control,sizeof(control),0,(struct sockaddr *)&from, length);
                        if (n < 0) error("NACK sendto error");
                        
                        n = sendto(sockUDP,control,sizeof(control),0,(struct sockaddr *)&from, length);
                        if (n < 0) error("NACK sendto error");
                        
                        break;
                        
                    }
                }
            }
            if(j==totPkts)
            {
                nackCounter++;
                /*   n = write(sockfd,control,sizeof(control));
                 if (n < 0)
                 error("ERROR writing to socket");*/
                n = sendto(sockUDP,control,sizeof(control),0,(struct sockaddr *)&from, length);
                if (n < 0) error("NACK sendto error");
                
                n = sendto(sockUDP,control,sizeof(control),0,(struct sockaddr *)&from, length);
                if (n < 0) error("NACK sendto error");
                
                
            }
            // printf("Number of bytes rxed: %ld\n",recvSize);
            FD_ZERO(&readfds);
            FD_SET(sockUDP, &readfds);
            
        }
    }
	close(sockUDP);
	//udp
    close(sockfd);
	
	return;

}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
    
	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}
	
	//TCP setup
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	//TCP setup
	
	while (1) {
        printf("Waiting for connection...\n");
		newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
			error("ERROR on accept");
		pid = fork();
		if (pid < 0)
			error("ERROR on fork");
		if (pid == 0)  {
			close(sockfd);
			dostuff(newsockfd);
			exit(0);
		}
		else {
            close(newsockfd);
            exit(0);
        }
	} /* end of while */
	close(sockfd);
	return 0; /* we never get here */
}
