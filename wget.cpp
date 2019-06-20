#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<fcntl.h>
#define ACCEPT "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8"
#define USERAGENT "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.132 Safari/537.36"
int  port = 80;
int pthread_num = 1;
char ip[1024];
char *source_page=NULL;
char *host;
char filename[255];
int avg_size;
int is_mul_thread = 1;
struct arg {
	int sockfd;
	int avg_num;
	int index;
};
//获得主机ip
int get_ip(char *url) {
	int result = 0;
	char *end = url;
	char *tmp;
	end = strstr(end, "//");
	end += 2;
	tmp = strchr(end, '/');
	int len = end - url + 1;
	char * ip_url = (char *)malloc(len);
	bzero(ip_url, len);
	strncpy(ip_url, end, tmp-end);
	struct hostent *ips;
	ips = gethostbyname(ip_url);
	if (ips == NULL) {
		printf("获取主机ip失败\n");
		result = -1;
		goto end;
	}
	 tmp=ips->h_addr_list[0];
	inet_ntop(ips->h_addrtype,tmp,ip,sizeof(ip));
end:
	host = ip_url;
	return result;
}
//获得资源目录
int get_source_page(char *url) {
	int result = 0;
	char *end = url;
	end = strstr(end, "//");
	end += 2;
	end = strchr(end, '/');
	int len = strlen(end);
	char *page = (char *)malloc(len + 1);
	strcpy(page, end);
	source_page = page;
	return result;
}
//创建套接字返回套接字
int create_socket() {
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("create socket error\n");
	}
	return sockfd;
}

//初始化addr
int init_addr(struct sockaddr_in &addr) {
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &addr.sin_addr);
	return 0;
}
//建立连接
int get_connect(int sockfd, struct sockaddr_in &addr) {
	int result = 0;
	if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
		printf("connect error\n");
		result = -1;
	}
	return result;
}

char *request_head() {
	char *req = "HEAD /%s HTTP/1.0\r\nHost:%s \r\nUser-Agent:%s\r\nAccept:%s\r\n\r\n";
	char *request = (char*)malloc(strlen(req) + strlen(source_page) + strlen(host) + strlen(USERAGENT)+strlen(ACCEPT));
	bzero(request, sizeof(request));
	if(source_page[0]=='/')
		source_page++;
	sprintf(request, req, source_page, host, USERAGENT,ACCEPT);
	return request;
}

//获取下载文件大小
int get_file_size(char *url, char *size, char *range) {
	int result = 0; //默认成功
	int ret;
	ret = get_ip(url);
	if (ret < 0) {
		result = -1;
		goto end;
	}
	ret = get_source_page(url);
	if (ret < 0) {
		result = -1;
		goto end;
	}

	int sockfd ;
	sockfd= create_socket();
	if (sockfd < 0) {
		result = -1;
		goto end;
	}
	struct sockaddr_in addr;
	init_addr(addr);
	ret = get_connect(sockfd, addr);
	if (ret < 0) {
		result = -1;
		printf("get connect error\n");
		goto end;
	}
	//组装请求头get请求
	char *req; 
	req= request_head();
	printf("%d\n",strlen(req)+1);
	//发送请求
	ret = send(sockfd, req, strlen(req)+1, 0);
	if (ret < 0) {
		printf("send error\n");
		result = -1;
		goto end;
	}
	//循环接收找到文件大小 及服务端是否支持多线程下载
	char buf[1024];
	char *tp, *pt;
	tp = NULL;
	pt = NULL;
	while ((ret = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
		if (tp == NULL) {
			tp = strstr(buf, "Content-Length:");
			if (tp != NULL) {
				int i;
				for (i = 0, tp++; tp[i] != '\n'; i++) {
					size[i] = tp[i];
				}
				size[i] = '\0';
			}
		}
		if (pt == NULL) {
			pt = strstr(buf, "Accept-Ranges:");
			if (pt != NULL) {
				int i;
				for (i = 0, pt++; pt[i] != '\n'; i++) {
					range[i] = pt[i];
				}
				range[i] = '\0';
			}
		}
	}
end:
	return sockfd;
}

//字符串转换为数字
void string_to_int( char *s, int &num) {
	char *tmp = s;
	num = atoi(s);
}
//数字转换为字符串
void int_to_string(int num, char *s) {
	int i = 0;
	char tmp[1024];
	while (num) {
		int t = num % 10;
		num /= 10;
		tmp[i] = t + '0';
		i++;
	}
	i--;
	for (int j = 0; i >= 0; i--, j++) {
		s[j] = tmp[i];
	}
}
//组装get请求
char * create_request( int start, int len) {
	int result = 0;
	if (is_mul_thread) {
		char *format = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nRange: %s-%s\r\nAccept: %s\r\n\r\n";
		char b[1024], e[1024];
		bzero(b, sizeof(b));
		bzero(e, sizeof(e));
		int_to_string(start, b);
		int_to_string(start + len, e);
		char *req = (char *)malloc(strlen(format) + strlen(source_page) + strlen(host) + strlen(USERAGENT) + strlen(b) + strlen(e)+strlen(ACCEPT));
		if(source_page[0]=='/')
			source_page++;
		sprintf(req, format, source_page, host, USERAGENT, b, e,ACCEPT);
		return req;
	}
	else {
		char *format = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: %s\r\n\r\n";
		char *reqt = (char *)malloc(strlen(format) + strlen(source_page) + strlen(host) + strlen(USERAGENT)+strlen(ACCEPT));
		if(source_page[0]=='/')
			source_page++;
		sprintf(reqt, format, source_page, host, USERAGENT,ACCEPT);
		printf("%d\n",strlen(source_page));
		return reqt;
	}

	return NULL;
}

void * func(void *a) {
	pthread_detach(pthread_self());
	//计算请求范围
	struct arg *tmp = (struct arg *)a;
	int index = tmp->index;
	
	int avg_num = tmp->avg_num;

	int ret=0,result=0;
	int fd ;
	fd= create_socket();
	if (fd < 0) {
		result = -1;
		goto end;
	}
	struct sockaddr_in addr;
	init_addr(addr);
	ret = get_connect(fd, addr);
	if (ret < 0) {
		result = -1;
		printf("get connect error\n");
		goto end;
	}
	//组装请求头
	char *req;
	req=create_request( index * avg_size, avg_num);

	//发送请求
	 ret = send(fd, req, strlen(req)+1, 0);
	if (ret < 0) {
		printf("send request error\n");
		goto end;
	}
	//循环读取数据
	char buf[1024];
	FILE *file;
	file = fopen(filename, "r+");
	if (file == NULL) {
		printf("没有此文件\n");
		goto end;
	}
	fseek(file, index * avg_size, SEEK_SET);
	//去除响应头
	char *data;
       	data= NULL;
	while ((ret = recv(fd, buf, sizeof(buf), 0)) > 0) {
		printf("%s\n",buf);
		data = strstr(buf, "\r\n\r\n");
		if (data != NULL)
			break;
	}
	if(ret<=0)
	printf("recv error  ret=%d\n",ret);
	//记录数据
	data += 4;
	fwrite(data, ret - (data - buf), 1, file);
	while ((ret = recv(fd, buf, sizeof(buf), 0)) > 0) {
		printf("%s\n",buf);
		fwrite(buf, ret, 1, file);
	}
end:
	if (req != NULL)
		free(req);
	if (file != NULL)
		fclose(file);
	return NULL;

}

int main(int argt ,char *argv[]) { //下载资源地址 线程数量 端口号
	if (argt <= 1) {
		printf("请输入要下载的资源地址，线程数量(默认为1)，端口号（默认为80）\n");
		return 0;
	}
	char *url = argv[1];
	if (argt == 3) {
		pthread_num = atoi(argv[2]);
	}
	else if (argt >= 4) {
		pthread_num = atoi(argv[2]);
		string_to_int(argv[3],port)
	}
	printf("begin\n");
	char size[10];
	char range[12];
	bzero(size, sizeof(size));
	bzero(range, sizeof(range));
	int sockfd = get_file_size(url, size, range);
	printf("filesize= %s  range=%s\n",size,range);
	int num = 0;
	struct arg ar;
	pthread_t pid;
	string_to_int(size, num);
		printf("please enter filename\n");
		scanf("%s", filename);
		FILE *file;
		file = fopen(filename, "w");
		fclose(file);
	if (strcmp(range, "none") == 0 || num == 0 || num == -1) {
		is_mul_thread = 0;
		ar.sockfd=sockfd;
		ar.index=0;
		pthread_create(&pid, NULL, func, (void *)&ar);
	}
	else {
		avg_size = num / pthread_num;
		
		for (int i = 0; i < pthread_num; i++) {
			if (i != pthread_num - 1) {
				ar.avg_num = avg_size;
				ar.sockfd = sockfd;
				ar.index = i;
			}
			else {
				ar.sockfd = sockfd;
				ar.index = i;
				ar.avg_num = num - avg_size * i;
			}
			pthread_create(&pid, NULL, func, (void *)&ar);
		}
	}

	close(sockfd);
	pthread_exit(NULL);
	return 0;
}




