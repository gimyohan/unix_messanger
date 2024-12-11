#include "headers.h"

#define MAX_MSQ_SIZE 10
#define MAX_BODY_SIZE (1<<10)
#define MAX_TALKER_NUM 5

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
int *shm_id, *shm_meta;
/*
	shared memory structure
	1. struct message[MAX_MSQ_SIZE] = {...};
	2. id set[MAX_TALKER_NUM+1] = {length of set, id1, id2, ..., idN}
	3. add[1] = {msg#, }
*/

int send_delay, receive_delay;
int id, msg_idx;
int sem; //sem[MAX_TALKER_NUM + 1 + 5] = {e, n_1, n_2, n_3, ... n_N, id_set, msg#, msg.N}
enum sem_num{
	E = 0,
	N = 0,
	ID = MAX_TALKER_NUM + 1,
	IDX,
	MSG
};

int semWait(int num){
	static struct sembuf buf = {-1, -1, 0};
	buf.sem_num = num;
	return semop(sem, &buf, 1);
}

int semSignal(int num){
	static struct sembuf buf = {-1, 1, 0};
	buf.sem_num = num;
	return semop(sem, &buf, 1);
}

void doSender(){
	struct sembuf buf; buf.sem_flg = 0;

	int number_of_talkers, idx, i;
	while(1){
		int n = read(0, body_buffer, MAX_BODY_SIZE-1);
		body_buffer[n-1] = 0;
		
		semWait(E);
		
		semWait(IDX);

		idx = *shm_meta;
		strcpy(shm_msg[idx].body, body_buffer);
		shm_msg[idx].sender_id = id;
		shm_msg[idx].time_to_live = number_of_talkers = shm_id[0];
		(*shm_meta)++;
		if(*shm_meta==MAX_MSQ_SIZE) *shm_meta = 0;
		idx = *shm_meta;

		if(send_delay) sleep(send_delay);

		semSignal(IDX);
		
		for(i=1;i<=MAX_TALKER_NUM;i++){
			if(shm_id[i]) semSignal(i);
		}
		
		if(number_of_talkers==1){
			printf("id=%d, talkers=%d, msg#=%d\n", id, number_of_talkers, idx);
		}
		if(strcmp(body_buffer, "talk_quit")==0) break;
	}
}

void doReceiver(){	
	struct sembuf buf; buf.sem_flg = 0;

	int sender_id, ttl;

	while(1){

		semWait(id);
		
		strcpy(body_buffer, shm_msg[msg_idx].body);
		sender_id = shm_msg[msg_idx].sender_id;

		semWait(MSG);

		shm_msg[msg_idx].time_to_live--;
		ttl = shm_msg[msg_idx].time_to_live;

		if(receive_delay) sleep(receive_delay);

		semSignal(MSG);

		if(ttl==0) semSignal(E);
		if(id!=sender_id){
			printf("[sender=%d & msg#=%d] %s\n", sender_id, msg_idx, body_buffer);
		}
		else if(strcmp(body_buffer, "talk_quit")==0) break;
		msg_idx++;
		if(msg_idx==MAX_MSQ_SIZE)msg_idx=0;
	}
}

void __debug_show_shm(){
	int i=0;
	for(i=0;i<MAX_MSQ_SIZE;i++){
		printf("===================\n");
		printf("msg queue%d:\n", i);
		printf("id:%d, ttl:%d\n", shm_msg[i].sender_id, shm_msg[i].time_to_live);
		printf("body:%s\n", shm_msg[i].body);
	}
	printf("--------------------\n");

	printf("current message pointer:%d\n", shm_meta[0]);
	printf("--------------------\n");

	printf("id[%d]\n", shm_id[0]);
	for(i=1;i<=MAX_TALKER_NUM;i++)printf("(%d) ", shm_id[i]);
	printf("\n");
	printf("--------------------\n");
}

void init(){
	key_t shm_key = ftok("key", 0), sem_key = ftok("key", 1);
	union semun sem_arg;

	//sheared memory
	shmid = shmget(shm_key, MAX_MSQ_SIZE*sizeof(struct message)+(MAX_TALKER_NUM+2)*sizeof(int), 0600|IPC_CREAT|IPC_EXCL);
	if(shmid==-1) shmid = shmget(shm_key, MAX_MSQ_SIZE*sizeof(struct message)+(MAX_TALKER_NUM+2)*sizeof(int), 0);
	//else init shared memory
	shm_msg = (struct message *)shmat(shmid, NULL, 0);
	shm_id = (int*)(shm_msg + MAX_MSQ_SIZE);
	shm_meta = (int*)(shm_id + MAX_TALKER_NUM + 1);

	//semaphore
	sem = semget(sem_key, MAX_TALKER_NUM + 4, 0600|IPC_CREAT|IPC_EXCL);
	if(sem==-1) sem = semget(sem_key, MAX_TALKER_NUM + 4, 0);
	else{ //init semaphore
		unsigned short arr[MAX_TALKER_NUM + 4] = {MAX_MSQ_SIZE, 0, };
		arr[MAX_TALKER_NUM + 1] = arr[MAX_TALKER_NUM + 2] = arr[MAX_TALKER_NUM + 3] = 1;
		sem_arg.array = arr;
		semctl(sem, 0, SETALL, sem_arg);
	}

	//check id
	int login_flag = 0, number_of_talkers;
	semWait(ID);
	number_of_talkers = shm_id[0];
	if(0<id&&id<=MAX_TALKER_NUM&&number_of_talkers<MAX_TALKER_NUM&&shm_id[id]==0){
		number_of_talkers = ++shm_id[0];
		shm_id[id] = id;
		login_flag = 1;
	}
	semSignal(ID);
	if(login_flag==0){
		printf("로그인 실패\n");
		exit(0);
	}

	semWait(IDX);
	msg_idx = shm_meta[0];
	semSignal(IDX);

	printf("id=%d, talkers=%d, msg#=%d\n", id, number_of_talkers, msg_idx);
	// __debug_show_shm();
}

void clear(){
	semWait(ID);

	if(shm_id[0]==1){
		shmctl(shmid, IPC_RMID, NULL);
		semctl(sem, MAX_TALKER_NUM + 4, IPC_RMID);
		return;
	}

	shm_id[0]--;
	shm_id[id] = 0;

	semSignal(ID);
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

	wait(0);
	wait(0);

	clear();

	return 0;
}