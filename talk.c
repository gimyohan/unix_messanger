#include "headers.h"

const int message_queue_size = 10, max_body_size = 1<<10, max_talker_number = 20;

struct message{
	char body[max_body_size];
	int sender_id;
};

union semun{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

char body_buffer[max_body_size];
struct message *shm_msg;
int *shm_meta; //int[2]={number of talkers, current message pointer(index)}
int id, msg_idx;
int msg_sem, meta_sem, pc_sem; //pc_sem[2] = {n, e}

void doSender(){
	while(1){
		int n = read(0, body_buffer, max_body_size-1);
		body_buffer[n] = 0;
		
		
	}
}

void consume(){
	
}

void produce(){

}

void doReceiver(){
	union semun sem_arg;
	struct sembuf buf;
	buf.sem_flg = 0;
	while(1){
		//semWait(n)
		buf.sem_num = 0, buf.sem_op = -1;
		semop(pc_sem, &buf, 1);
		
		buf.sem_num = msg_idx;
		
		if(shm_msg[msg_idx].sender_id!=id) printf("[sender=%d & msg#=%d] %s\n", shm_msg[msg_idx].sender_id, msg_idx, shm_msg[msg_idx].body);
		buf.sem_op = -1;
		semop(msg_sem, &buf, 1);
		if(semctl(msg_sem, msg_idx, GETVAL)==0) consume();

		msg_idx++;
		if(msg_idx==message_queue_size)msg_idx = 0;
	}
}

void init(){
	key_t shm_key = ftok("key", 0), msg_sem_key = ftok("key", 1), meta_sem_key = ftok("key", 2), pc_sem_key = ftok("key", 3);
	union semun sem_arg;
	struct sembuf buf; buf.sem_flg = 0;

	//sheared memory
	int shmid = shmget(shm_key, message_queue_size*sizeof(struct message)+2*sizeof(int), 0600|IPC_CREAT|IPC_EXCL);
	if(shmid==-1) shmget(shm_key, message_queue_size*sizeof(struct message)+2*sizeof(int), 0);
	//else init shared memory
	shm_msg = (struct message *)shmat(shmid, NULL, 0);
	shm_meta = (int*)(shm_msg + message_queue_size);

	//semaphore
	msg_sem = semget(msg_sem_key, message_queue_size, 0600|IPC_CREAT|IPC_EXCL);
	if(msg_sem==-1) semget(msg_sem_key, message_queue_size, 0);
	else{ //init semaphore
		unsigned short arr[10] = {0, };
		sem_arg.array = arr;
		semctl(msg_sem, 0, SETALL, sem_arg);
	}

	meta_sem = semget(meta_sem_key, 2, 0600|IPC_CREAT|IPC_EXCL);
	if(meta_sem==-1)semget(meta_sem_key, 2, 0);
	else{
		unsigned short arr[2] = {1, 1};
		sem_arg.array = arr;
		semctl(meta_sem, 0, SETALL, sem_arg);
	}

	pc_sem = semget(pc_sem_key, 2, 0600|IPC_CREAT|IPC_EXCL);
	if(pc_sem==-1)semget(pc_sem_key, 2, 0);
	else{
		unsigned short arr[2] = {0, message_queue_size};
		sem_arg.array = arr;
		semctl(pc_sem, 0, SETALL, sem_arg);
	}

	//todo: id check
	//check max talkers
	int max_flag = 0;
	buf.sem_num = 0;
	buf.sem_op = -1;
	semop(meta_sem, &buf, 1);
	//critical section of number of talkers
	if(shm_meta[0]==max_talker_number) max_flag = 1;
	else shm_meta[1]++;
	buf.sem_op = 1;
	semop(meta_sem, &buf, 1);

	if(max_flag){
		printf("자리 없음\n");
		exit(0);
	}

	//set message index
	buf.sem_num = 1;
	buf.sem_op = -1;
	semop(meta_sem, &buf, 1);
	//critical section of current message index
	msg_idx = shm_meta[1];
	buf.sem_op = 1;
	semop(meta_sem, &buf, 1);

	if(fork()==0){
		doReceiver();
	}
	else{
		doSender();
	}
}

int main(int argc, char **argv){

	if(argc==1)return 1; id = atoi(argv[1]);

	init();

	return 0;
}