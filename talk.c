#include "headers.h"

#define MAX_SEM_SIZE 5
#define MAX_MSG_QUEUE 10
#define MAX_BODY_SIZE (1<<10)
#define MAX_TALKER_NUM 10
// const int MAX_MSG_QUEUE = 10, MAX_BODY_SIZE = 1<<10, MAX_TALKER_NUM = 20;

struct message{
	char body[MAX_BODY_SIZE];
	int sender_id;
	int time_to_live;
};

union semun{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};



char body_buffer[MAX_BODY_SIZE];
int shmid;
struct message *shm_msg;
int *shm_meta, *shm_id;
/*
	shared memory structure
	1. struct message[MAX_MSG_QUEUE] = {...};
	2. meta data[1] = {current message pointer(index)}
	3. id set[MAX_TALKER_NUM+1] = {length of set, id1, id2, ...}
*/

int send_delay, receive_delay;
int id, msg_idx;
int sem; //sem[5] = {e, n, id, msg#, msg}
enum sem_num{
	E = 0,
	N,
	ID,
	IDX,
	MSG
};

int semWait(enum sem_num num){
	static struct sembuf buf = {-1, -1, 0};
	buf.sem_num = num;
	return semop(sem, &buf, 1);
}

int semSignal(enum sem_num num){
	static struct sembuf buf = {-1, 1, 0};
	buf.sem_num = num;
	return semop(sem, &buf, 1);
}

void doSender(){
	struct sembuf buf; buf.sem_flg = 0;

	int number_of_talkers, idx;
	while(1){
		int n = read(0, body_buffer, MAX_BODY_SIZE-1);
		body_buffer[n] = 0;
		
		//semWait(e)
		semWait(E);
		// buf.sem_num = 0, buf.sem_op = -1;
		// semop(sem, &buf, 1);
		
		//semWait(msg)
		buf.sem_num = 4, buf.sem_op = -1;
		semop(sem, &buf, 1);
		
		//semWait(id)
		buf.sem_num = 2, buf.sem_op = -1;
		semop(sem, &buf, 1);

		//semWait(msg#)
		buf.sem_num = 3, buf.sem_op = -1;
		semop(sem, &buf, 1);

		idx = *shm_meta;
		strcpy(shm_msg[idx].body, body_buffer);
		shm_msg[idx].sender_id = id;
		shm_msg[idx].time_to_live = number_of_talkers = shm_id[0];
		(*shm_meta)++;
		if(*shm_meta==MAX_MSG_QUEUE) *shm_meta = 0;
		idx = *shm_meta;

		if(send_delay)sleep(send_delay);

		//semSignal(msg#)
		buf.sem_num = 3, buf.sem_op = 1;
		semop(sem, &buf, 1);

		//semSignal(id)
		buf.sem_num = 2, buf.sem_op = 1;
		semop(sem, &buf, 1);

		//semSignal(msg)
		buf.sem_num = 4, buf.sem_op = 1;
		semop(sem, &buf, 1);
		
		//semSignal(n)
		buf.sem_num = 1, buf.sem_op = 1;
		semop(sem, &buf, 1);
		

		if(number_of_talkers==1){
			printf("id=%d, talkers=%d, msg#=%d\n", id, number_of_talkers, idx);
		}
		if(strcmp(body_buffer, "talk_quit\n")==0) break;

	}
}

void doReceiver(){	
	struct sembuf buf; buf.sem_flg = 0;

	int sender_id, ttl;

	while(1){
		//semWait(n)
		buf.sem_num = 1, buf.sem_op = -1;
		semop(sem, &buf, 1);

		//semWait(msg)
		buf.sem_num = 4, buf.sem_op = -1;
		semop(sem, &buf, 1);
		
		shm_msg[msg_idx].time_to_live--;
		strcpy(body_buffer, shm_msg[msg_idx].body);
		sender_id = shm_msg[msg_idx].sender_id;
		ttl = shm_msg[msg_idx].time_to_live;
		if(receive_delay) sleep(receive_delay);

		//semSignal(msg)
		buf.sem_num = 4, buf.sem_op = 1;
		semop(sem, &buf, 1);

		if(ttl){
			//semSignal(n)
			buf.sem_num = 1, buf.sem_op = 1;
			semop(sem, &buf, 1);
		}
		else{
			//semSignal(e)
			buf.sem_num = 0, buf.sem_op = 1;
			semop(sem, &buf, 1);
		}
		
		if(id!=sender_id){
			printf("[sender=%d & msg#=%d] %s", sender_id, msg_idx, body_buffer);
		}
		msg_idx++;
		if(msg_idx==MAX_MSG_QUEUE)msg_idx=0;
	}
}

void __debug_show_shm(){
	int i=0;
	for(i=0;i<MAX_MSG_QUEUE;i++){
		printf("===================\n");
		printf("msg queue%d:\n", i);
		printf("id:%d, ttl:%d\n", shm_msg[i].sender_id, shm_msg[i].time_to_live);
		printf("body:%s\n", shm_msg[i].body);
	}
	printf("--------------------\n");

	printf("current message pointer:%d\n", shm_meta[0]);
	printf("--------------------\n");

	printf("id[%d]\n", shm_id[0]);
	for(i=1;i<=shm_id[0];i++)printf("(%d) ", shm_id[i]);
	printf("\n");
	printf("--------------------\n");
}

void init(){
	key_t shm_key = ftok("key", 0), sem_key = ftok("key", 1);
	union semun sem_arg;
	int number_of_talkers;

	//sheared memory
	shmid = shmget(shm_key, MAX_MSG_QUEUE*sizeof(struct message)+(MAX_TALKER_NUM+2)*sizeof(int), 0600|IPC_CREAT|IPC_EXCL);
	if(shmid==-1) shmid = shmget(shm_key, MAX_MSG_QUEUE*sizeof(struct message)+(MAX_TALKER_NUM+2)*sizeof(int), 0);
	//else init shared memory
	shm_msg = (struct message *)shmat(shmid, NULL, 0);
	shm_meta = (int*)(shm_msg + MAX_MSG_QUEUE);
	shm_id = (int*)(shm_meta + 1);


	//semaphore
	sem = semget(sem_key, MAX_SEM_SIZE, 0600|IPC_CREAT|IPC_EXCL);
	if(sem==-1) sem = semget(sem_key, MAX_SEM_SIZE, 0);
	else{ //init semaphore
		unsigned short arr[MAX_SEM_SIZE] = {MAX_MSG_QUEUE, 0, 1, 1, 1};
		sem_arg.array = arr;
		semctl(sem, 0, SETALL, sem_arg);
	}

	struct sembuf buf; buf.sem_flg = 0;
	//todo: id check
	//check max talkers
	int login_flag = 0;
	buf.sem_num = 2, buf.sem_op = -1;
	semop(sem, &buf, 1);
	//critical section of id set
	number_of_talkers = shm_id[0];
	if(number_of_talkers<MAX_TALKER_NUM){
		int i, dup = 0;
		for(i=1;i<=number_of_talkers;i++){
			if(shm_id[i]==id)dup = 1;
		}
		if(!dup){
			number_of_talkers++;
			shm_id[0]++;
			shm_id[shm_id[0]] = id;
			login_flag = 1;
		}
	}
	buf.sem_op = 1;
	int ret = semop(sem, &buf, 1);
	if(login_flag==0||ret==-1){
		printf("로그인 실패\n");
		exit(0);
	}

	//set message index
	buf.sem_num = 3;
	buf.sem_op = -1;
	semop(sem, &buf, 1);
	//critical section of current message index
	msg_idx = shm_meta[0];
	buf.sem_op = 1;
	semop(sem, &buf, 1);


	printf("id=%d, talkers=%d, msg#=%d\n", id, number_of_talkers, msg_idx);
	// __debug_show_shm();
}

void clear(){
	struct sembuf buf; buf.sem_flg = 0;

	//semWait(id)
	buf.sem_num = 2, buf.sem_op = -1;
	semop(sem, &buf, 1);

	if(shm_id[0]==1){
		shmctl(shmid, IPC_RMID, NULL);
		semctl(sem, MAX_SEM_SIZE, IPC_RMID);
		return;
	}

	int i, f;
	for(i=1, f=0;i<=shm_id[0];i++){
		if(shm_id[i]==id)f=1;
		if(f&&i<shm_id[0])shm_id[i] = shm_id[i+1];
	}
	shm_id[0]--;

	//semSignal(id)
	buf.sem_num = 2, buf.sem_op = 1;
	semop(sem, &buf, 1);
}

int main(int argc, char **argv){

	if(argc==1)return 1; id = atoi(argv[1]);
	if(argc>=3&&strcmp(argv[2], "-d")==0){
		send_delay = receive_delay = 1;
		if(argc>=4)send_delay = atoi(argv[3]);
		if(argc>=5)receive_delay = atoi(argv[4]);
		printf("send delay: %d, receive delay: %d\n", send_delay, receive_delay);
	}

	init();

	int receiver_id, sender_id;
	receiver_id = fork();
	if(receiver_id==0){
		doReceiver();
		exit(0);
	}
	sender_id = fork();
	if(sender_id==0){
		doSender();
		exit(0);
	}

	waitpid(sender_id, 0, 0);
	kill(receiver_id, SIGINT);
	wait(0);

	clear();

	return 0;
}