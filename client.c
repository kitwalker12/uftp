#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>

int portno;
char *map;
int fd;
int result;
long int sendSize;
int currSeq=0;
struct stat finfo;
int numPackets=0;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

void tcplistner (int sock, int sockUDP, struct sockaddr_in from, socklen_t fromlen) {
    char buffer[256];
    int n,i;
	bzero(buffer,256);
    int control[1470];
    printf("Entering UDP listner\n");
    while(1) {
        //printf("Waiting for UDP NACK...\n");
        bzero(control,1470);
        /*n = read(sock,control,sizeof(control));
         if (n < 0) error("ERROR reading from socket");*/
        n = recvfrom(sockUDP,control,sizeof(control),0,(struct sockaddr *)&from,&fromlen);
        if (n < 0) error("ERROR in recieving NACK");
        //printf("NACK: %d\n",control[0]);
        //printf("NACK: \n");
        if (control[0] == -1) {
            printf("File Sent\n");
            return;
        }
        else {
            
            char seqC[8];
            //memcpy(seqC,control,8);
            //int seq=atoi(seqC);
            char data[1464];
            char sendData[1472];
            char seqChar[9];
            for(i=0;i<1470;i++){
                //fseek(f,control[i]*1024,SEEK_SET);
                //int bytes_read=fread(data,1024,1,f);
                //printf("Entered Loop\n");
                if (control[i] != numPackets){
                    memcpy(data,&map[control[i]*1464],1464);
                    /* if(bytes_read==0)
                     {
                     return;
                     }*/
                    sprintf(seqChar,"%8d",control[i]);
                    memcpy(sendData,seqChar,8);
                    memcpy(sendData+8,data,sizeof(data));
                    printf("Sending sequence: %d\n",control[i]);
                    n = sendto(sockUDP,sendData,sizeof(sendData),0,(struct sockaddr *)&from,fromlen);
                    if (n  < 0) error("sendto");
                    //usleep(5);
                }
                else{
                    memcpy(data,&map[control[i]*1464],finfo.st_size-((numPackets-1)*1464));
                    /* if(bytes_read==0)
                     {
                     return;
                     }*/
                    sprintf(seqChar,"%8d",control[i]);
                    memcpy(sendData,seqChar,8);
                    memcpy(sendData+8,data,sizeof(data));
                    printf("Sending sequence: %d\n",control[i]);
                    n = sendto(sockUDP,sendData,sizeof(sendData),0,(struct sockaddr *)&from,fromlen);
                    if (n  < 0) error("sendto");
                    
                }
            }//for
        }//else
    }//while
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *serv;
    
    
    
	char buffer[256];
	if (argc < 4) {
		fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
		exit(0);
	}
	
	//setup TCP
	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	serv = gethostbyname(argv[1]);
	if (serv == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)serv->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          serv->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	//TCP setup

    //printf("Please enter the TCP message: ");
	bzero(buffer,256);
	//fgets(buffer,255,stdin);
	sprintf(buffer,"%s",argv[3]);
    n = write(sockfd,buffer,strlen(buffer));
	if (n < 0)
		error("ERROR writing to socket");
	
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
	if (n < 0)
		error("ERROR reading from socket");
	printf("Server Message: %s\n",buffer);
    
    //open file
    fd = open(argv[3], O_RDONLY);
    if (fd == -1) {
        error("open");
    }
    //get file details
	if (-1 == stat(argv[3], &finfo)) {
		error("error stating file!\n");
		exit(0);
	}
	bzero(buffer,256);
	sprintf(buffer, "%lld", (long long) finfo.st_size);
    //send file details
    printf("Sending File Size: %s\n",buffer);
	n = write(sockfd,buffer,strlen(buffer));
	if (n < 0)
		error("ERROR writing to socket");
    
    
	//udp
	int sockUDP;
	unsigned int length;
	struct sockaddr_in server, from;
	struct hostent *hp;
    int nackCounter = 0;
    bool isFileSent=false;
    socklen_t fromlen;
    fromlen = sizeof(struct sockaddr_in);
    
	sockUDP= socket(AF_INET, SOCK_DGRAM, 0);
	if (sockUDP < 0) error("socket");
    
    int buffSize = 32*1024*1024;
    
    if (setsockopt(sockUDP, SOL_SOCKET, SO_SNDBUF, &buffSize, sizeof buffSize)<0){
        error("buff change error");
    }
    
    if (setsockopt(sockUDP, SOL_SOCKET, SO_RCVBUF, &buffSize, sizeof buffSize)<0){
        error("buff change error");
    }
    
    
	server.sin_family = AF_INET;
	hp = gethostbyname(argv[1]);
	if (hp==0) error("Unknown host");
    
	bcopy((char *)hp->h_addr,
          (char *)&server.sin_addr,
          hp->h_length);
	server.sin_port = htons(portno+1);
	length=sizeof(struct sockaddr_in);
    
    printf("Starting send\n");
    
    map = (char*)mmap(0, finfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        error("map");
    }    
    numPackets = finfo.st_size/1464;
    numPackets = numPackets+1;
    
	while (1) {
        long int size=finfo.st_size;
        //long int totPkts=(size/1024)+1;
        for(long int i=0;i<size;i++) {
            char data[1464];
            char sendData[1472];
            char seqChar[9];
            /*if((i%200)==0){
             usleep(250);
             }*/
            //fseek(f,currSeq*1024,SEEK_SET);
            //int bytes_read=fread(data,1024,1,f);
            //sendSize += 1024;
            if(currSeq == numPackets-1)
            {
                isFileSent=true;
                break;
            }
            
            if (currSeq < numPackets){ //all packets except the last packet
                sprintf(seqChar,"%8d",currSeq);
                memcpy(sendData,seqChar,8);
                memcpy(sendData+8,&map[currSeq*1464],1464);
                printf("Sending sequence: %d\n",currSeq);
                n = sendto(sockUDP,sendData,sizeof(sendData),0,(struct sockaddr *)&server,fromlen);
                if (n  < 0) error("sendto");
                //usleep(5);
                currSeq++;
            }
            else{ //the last packet.
                printf("Sending the last packet\n");
                sprintf(seqChar,"%8d",currSeq);
                memcpy(sendData,seqChar,8);
                memcpy(sendData+8,&map[currSeq*1464],finfo.st_size-(currSeq*1464));
                printf("Sending last sequence: %d\n",currSeq);
                n = sendto(sockUDP,sendData,sizeof(sendData),0,(struct sockaddr *)&server,fromlen);
                if (n  < 0) error("sendto last seq");
                currSeq++;
                //isFileSent=true;
                //break;
            }
        }
        if(isFileSent)
        {
            int pid=fork();
            if (pid < 0)
                error("ERROR on fork");
            if (pid == 0)  {
                tcplistner(sockfd, sockUDP, server, fromlen);
                exit(0);
            }
            break;
        }
    }//end while
    
}