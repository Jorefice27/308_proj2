#include "bank.h"
#include "server.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>


void *processRequest(void *);
void *mainThread(void *);

pthread_cond_t list_cv;
pthread_cond_t end_cv;
pthread_mutex_t mut;
pthread_mutex_t account_mut;
pthread_mutex_t *thread_status;
pthread_mutex_t file_mut;
int j;
const int REQUEST_SIZE = 1000;
int numWorkers;
int numAccounts;
char* filename;
int numRequests;
bool end;

int parseRequest(char* input, char* request);
void processCheckRequest(int requestID, char* accountId, struct timeval t1);
void processTransactionRequest(int requestID, struct timeval t1);
void insertionSort(int accounts[10][2], int length);
void closeServer();
void initThreadStuff();
FILE *fp;

typedef struct Request
{
	int requestID;
	struct Request *next;
	struct Request *prev;
	int numArgs;
	char* request;
	struct timeval t;
} Request;

Request head = {0, NULL, NULL, 0};

void addToEnd(Request *head, int id, int numArgs, char* request);
void addToEmpty(Request *head, int id, int numArgs, char* request);
Request pop(Request *head);

int main(int argc, char **argv)
{
	numWorkers = atoi(argv[1]); //pareseInt()
	numAccounts = atoi(argv[2]);
	filename = argv[3];
	numRequests = 0;
	end = false;
	strcat(filename, ".txt");
	fp = fopen(filename, "w");
	fclose(fp);

	initThreadStuff();
	pthread_t worker_tid[numWorkers];
  pthread_t main_tid;
	int thread_index[numWorkers];
	thread_status = malloc(sizeof(*thread_status) * numWorkers);
	int i;


  pthread_create(&main_tid, NULL, mainThread, NULL);
	//initialize worker threads
	for(i = 0; i < numWorkers; i++)
	{
		thread_index[i] = i;
		pthread_mutex_init(&thread_status[i], NULL);
		pthread_create(&worker_tid[i], NULL, processRequest, (void *)&thread_index[i]);
	}

  pthread_join(main_tid, NULL);
	for(i = 0; i < numWorkers; i++)
	{
    pthread_join(worker_tid[i], NULL);
	}

  printf("closing server\n");
}

void initThreadStuff()
{
	pthread_mutex_init(&mut, NULL);
	pthread_mutex_init(&file_mut, NULL);
  pthread_mutex_init(&account_mut);
	pthread_cond_init(&list_cv, NULL);
	pthread_cond_init(&end_cv, NULL);
}

void *mainThread(void *arg)
{
  int i;
  int account_ids[numAccounts];
  initialize_accounts(numAccounts);
	for(i = 0; i < numAccounts; i++)
	{
		account_ids[i] = i + 1;
	}

	while(true)
	{
		char* input = (char*) malloc(REQUEST_SIZE);
		char* request = (char*) malloc(REQUEST_SIZE);
		fgets(input, REQUEST_SIZE, stdin);
    if(input[0] != 10) //as long as it's not just a new line feed
		{
      printf("ID %d\n", numRequests + 1);
      int numArgs = parseRequest(input, request);
			if(strcmp(request, "END") == 0)
	    {
				end = true;
	      break;
	    }
  		pthread_mutex_lock(&mut);
  		addToEnd(&head, ++numRequests, numArgs, request);
  		pthread_mutex_unlock(&mut);
    }

		free(input);
	}

	printf("No more user input will be accepted but all %d remaining requests will be completed.\n", head.requestID);
	while(head.requestID != 0)
	{
		pthread_cond_wait(&end_cv, &mut);
	}
	for(i = 0; i < numWorkers; i++)
	{
		pthread_mutex_lock(&thread_status[i]);
		printf("Locked thread %d\n", i+1);
	}
	printf("All requests have been completed. Server closing.\n");
	exit(0);
}

void *processRequest(void *arg)
{
	int i;
	int id = *((int *) arg);
	while(true)
	{
		// pthread_mutex_lock(&mut);
		while(head.requestID == 0)
		{
			// pthread_mutex_unlock(&mut);
			pthread_cond_wait(&list_cv, &mut);
		}
		// pthread_mutex_lock(&mut);
		pthread_mutex_lock(&thread_status[id]);
		Request r = pop(&head);
		pthread_mutex_unlock(&mut);
    char *token = strtok(r.request, " ");
    if(strcmp(token, "CHECK") == 0 && r.numArgs == 2)
    {
      processCheckRequest(r.requestID, strtok(NULL, " "), r.t);
    }
    else if(strcmp(token, "TRANS") == 0)
    {
			processTransactionRequest(r.requestID, r.t);
    }
		pthread_mutex_unlock(&thread_status[id]);
	}
}

void processCheckRequest(int requestID, char *accountId, struct timeval t1)
{
  if(accountId != NULL)
  {
    int id = atoi(accountId);
		if(id > 0 && id <= numAccounts)
		{
			pthread_mutex_lock(&account_mut);
	    int bal = read_account(id);
			pthread_mutex_unlock(&account_mut);
			struct timeval t2;
			gettimeofday(&t2, NULL);
			double t = t2.tv_sec + ((double) t2.tv_usec / 1000000);
			t -= t1.tv_sec;
			t -= ((double) t1.tv_usec / 1000000);
			pthread_mutex_lock(&file_mut);
			fp = fopen(filename, "a");
			fprintf(fp, "<%d> BAL $%d TIME %f\n", requestID, bal, t);
			fclose(fp);
			pthread_mutex_unlock(&file_mut);
		}
  }
}

void processTransactionRequest(int requestID, struct timeval t1)
{
	int orig[10][2];
	int accounts[10][2]; //probably sort this list to avoid deadlock
	int i = 0;
	int err = -1;
	int length = 0;
	char *token = strtok(NULL, " ");
	//lock the mutex for each account and save the transaction for later
	while(token != NULL)
	{
		int id = atoi(token);
		token = strtok(NULL, " ");
		if(token == NULL)
		{
			break;
		}
		int amt = atoi(token);
		token = strtok(NULL, " ");
		int j;
		bool set = false;
		for(j = 0; j < i; j++)
		{
			if(accounts[j][0] == id)
			{
				accounts[j][1] += amt;
				set = true;
				j = i;
			}
		}
		if(!set)
		{
			accounts[i][0] = id;
			accounts[i][1] = amt;
			pthread_mutex_lock(&account_mut);
			i++;
			length ++;
		}
	}
	insertionSort(accounts, length);
	for(i = 0; i < length; i++)
	{
		orig[i][0] = accounts[i][0];
		int bal = read_account(accounts[i][0]);
		orig[i][1] = bal;
		bal += accounts[i][1];
		if(bal <= 0)
		{
			err = accounts[i][0];
			break;
		}
		write_account(accounts[i][0], bal);
	}
	struct timeval t2;
	gettimeofday(&t2, NULL);
	double t = t2.tv_sec + ((double) t2.tv_usec / 1000000);
	t -= t1.tv_sec;
	t -= ((double) t1.tv_usec / 1000000);

	if(err > 0)
	{
		//reset the original balances
		for(i; i >= 0; i--)
		{
			write_account(orig[i][0], orig[i][1]);
		}
		pthread_mutex_lock(&file_mut);
		fp = fopen(filename, "a");
		fprintf(fp, "<%d> ISF <%d> TIME %f\n", requestID, err, t);
		fclose(fp);
		pthread_mutex_unlock(&file_mut);
	}
	else
	{
		pthread_mutex_lock(&file_mut);
		fp = fopen(filename, "a");
		fprintf(fp, "<%d> OK TIME %f\n", requestID, t);
		fclose(fp);
		pthread_mutex_unlock(&file_mut);
	}
	//unlock the mutexes
	for(i = 0; i < length; i++)
	{
		pthread_mutex_unlock(&account_mut);
	}
}

void insertionSort(int accounts[10][2], int length)
{
	int i;
	for(i = 0; i < length; i++)
	{
		int id = accounts[i][0];
		int amt = accounts[i][1];
		int j = i - 1;

		while(j >= 0 && accounts[j][0] > id)
		{
			accounts[j+1][0] = accounts[j][0];
			accounts[j+1][1] = accounts[j][1];
			j--;
		}
		accounts[j+1][0] = id;
		accounts[j+1][1] = amt;
	}
}

int parseRequest(char* input, char* request)
{
	int numArgs = 0;
	int i;
	for(i = 0; i < REQUEST_SIZE; i++)
	{
		char c = input[i];
		if(c == '\0' || c == 10) //end of input, 10 = new line feed
		{
			request[i] = '\0';
			numArgs ++;
			break;
		}
		else if(c == ' ') // found whitespace
		{
			request[i] = c;
			numArgs++;
		}
		else
		{
			request[i] = c;
		}
	}
	return numArgs;
}

void addToEnd(Request *head, int id, int numArgs, char* request)
{
	if(head->requestID == 0)
	{
		addToEmpty(head, id, numArgs, request);
		return;
	}
  //create request struct
	Request* temp = (Request*) malloc(sizeof(Request));
  temp->requestID = id;
	temp->numArgs = numArgs;
	temp->request = request;
	gettimeofday(&temp->t, NULL);


  head->prev->next = temp;
  temp->prev = head->prev;
	temp->next = head;
	head->prev = temp;
	head->requestID++;
}

void addToEmpty(Request *head, int id, int numArgs, char* request)
{
  //create request struct
	Request* temp = (Request*) malloc(sizeof(Request));
  temp->requestID = id;
  temp->numArgs = numArgs;
  temp->request = request;
	gettimeofday(&temp->t, NULL);

	head->next = temp;
	head->prev = temp;
	temp->next = head;
	temp->prev = head;
	head->requestID++;
	pthread_cond_broadcast(&list_cv);
}

Request pop(Request *head)
{
	if(head->requestID > 0)
	{
		Request r = *head->next;
		head->next = r.next;
		head->next->prev = head;
		head->requestID--;

		if(head->requestID == 0 && end)
		{
			pthread_cond_broadcast(&end_cv);
		}
		return r;
	}
}
