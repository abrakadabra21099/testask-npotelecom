#define SINGLE_MSG_BODY_SIZE 1024
#define TEN_SERIES_MSG_COUNT 10
#define SERVER_LISTEN_PORT 53353
struct SINGLE_MSG_HEAD {
	 unsigned char MSG_OPERATION; 	//0 - передача сообщения, 
									//1 - запрос сообщения,
									//2 - подтверждение получения 
									//		ten серии
	 short int MSG_ID; //порядковый номер сообщения
	 unsigned short MSG_SIZE; //размер полезных данных
};
struct SINGLE_MSG {
	struct SINGLE_MSG_HEAD head; 
	unsigned char MSG_BODY [SINGLE_MSG_BODY_SIZE]; // данные сообщения			 
};

int get_sock();
int sent_one_packet(const int sock, const struct sockaddr_in addr, 
                    const void *msg, const int msglen);
