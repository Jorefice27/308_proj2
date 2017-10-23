#include "bank.h"
#include "server.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
// #include <file.io>

/*
*	Make sure to add some validation for unexpected inputs later on
* We'll need a mutex for the linked list and we'll need one for each bank account
* Will I need to do anything to prevent writter starvation with the linked list?
* Does numRequests need protection?
*/

void *processRequest(void *);
void *mainThread(void *);

// pthread_mutex_t mut;
// pthread_cond_t linked_list_cv;
pthread_cond_t list_cv;
pthread_mutex_t mut;

int j;
const int REQUEST_SIZE = 1000;
int numWorkers;
int numAccounts;
char* filename;
int numRequests;
bool end;

int parseRequest(char* input, char* request);
void processCheckRequest(int requestID, char* accountId);
void processTransactionRequest(int requestID);
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
} Request;

Request head = {0, NULL, NULL, 0};

void addToEnd(Request *head, int id, int numArgs, char* request);
void addToEmpty(Request *head, int id, int numArgs, char* request);
Request pop(Request *head);

int main(int argc, char **argv)
{
	initThreadStuff();
	numWorkers = atoi(argv[1]); //pareseInt()
	numAccounts = atoi(argv[2]);
	filename = argv[3];
	numRequests = 0;
	end = false;
	printf("Num worker_tid = %d\n", numWorkers);
	printf("Num accounts = %d\n", numAccounts);
	printf("Filename = %s\n", filename);
	fp = fopen(filename, "w");
	fclose(fp);
	pthread_t worker_tid[numWorkers];
  pthread_t main_tid;
	int thread_index[numWorkers];
	int i;

  pthread_create(&main_tid, NULL, mainThread, NULL);
	//initialize worker threads
	for(i = 0; i < numWorkers; i++)
	{
		thread_index[i] = i;
		pthread_create(&worker_tid[i], NULL, processRequest, (void *)&thread_index[i]);
	}

  pthread_join(main_tid, NULL);
	for(i = 0; i < numWorkers; i++)
	{
    pthread_join(worker_tid[i], NULL);
    printf("joined thread %d\n", i);
	}

  printf("closing server\n");
}

void initThreadStuff()
{
	pthread_mutex_init(&mut, NULL);
	pthread_cond_init(&list_cv, NULL);
	printf("inited\n");
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

	while(!end)
	{
		char* input = (char*) malloc(REQUEST_SIZE);
		char* request = (char*) malloc(REQUEST_SIZE);
		fgets(input, REQUEST_SIZE, stdin);
    if(input[0] != 10) //as long as it's not just a new line feed
		{
      printf("ID %d\n", ++numRequests);
      int numArgs = parseRequest(input, request);
  		pthread_mutex_lock(&mut);
  		addToEnd(&head, numRequests, numArgs, request);
  		pthread_mutex_unlock(&mut);
		  // printf("Request ID %d is %s\n", head.next->requestID, head.next->request);
    }

		free(input);
	}
}

void *processRequest(void *arg)
{
	int i;
	int id = *((int *) arg);
	while(!end)
	{
		pthread_mutex_lock(&mut);
		while(head.requestID == 0)
		{
			pthread_mutex_unlock(&mut);
			pthread_cond_wait(&list_cv, &mut);
		}
		Request r = pop(&head);

    char *token = strtok(r.request, " ");
    if(strcmp(token, "CHECK") == 0 && r.numArgs == 2)
    {
      processCheckRequest(r.requestID, strtok(NULL, " "));
    }
		else if(strcmp(token, "END") == 0)
    {
      //this doesn't do what it's supposed to so fix it
      end = true;
    }
    else if(strcmp(token, "TRANS") == 0)
    {
			processTransactionRequest(r.requestID);
    }

    pthread_mutex_unlock(&mut);
	}
}

void processCheckRequest(int requestID, char *accountId)
{
  if(accountId != NULL)
  {
    int id = atoi(accountId);
		if(id > 0 && id <= numAccounts)
		{
	    int bal = read_account(id);
			fp = fopen(filename, "a");
			fprintf(fp, "<%d> BAL <$%d>\n", requestID, bal);
			fclose(fp);
		}
  }
}

void processTransactionRequest(int requestID)
{
	int orig[10][2];
	int i = 0;
	int err = -1;
	char *token = strtok(NULL, " ");
	while(token != NULL)
	{
		int accountId = atoi(token);
		token = strtok(NULL, " ");
		int amt = atoi(token);
		token = strtok(NULL, " ");
		int bal = read_account(accountId);
		orig[i][0] = accountId;
		orig[i++][1] = bal;
		bal += amt;
		if(bal <= 0)
		{
			err = accountId;
			break;

		}

		write_account(accountId, bal);
	}

	if(err > 0)
	{
		//reset the original balances
		for(i; i >= 0; i--)
		{
			write_account(orig[i][0], orig[i][1]);
		}
		fp = fopen(filename, "a");
		fprintf(fp, "<%d> ISF <%d>\n", requestID, err);
		fclose(fp);
	}
	else
	{
		fp = fopen(filename, "a");
		fprintf(fp, "<%d> OK\n", requestID);
		fclose(fp);
	}
}

void closeServer()
{

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

// void writef(char* text)
// {
// 	fp = fopen(filename, "a");
// 	fprintf(fp, "%s\n", text);
// 	fclose(fp);
// }

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


  head->prev->next = temp;
  temp->prev = head->prev;
	temp->next = head;
	head->prev = temp;
	head->requestID++;
  printf("-----------------------------------------------\n");
  // printf("head.next.id = %d and head.next.request = %s\n", head->next->requestID, head->next->request);
	// printf("head.prev.id = %d and head.prev.request = %s\n", head->prev->requestID, head->prev->request);

}

void addToEmpty(Request *head, int id, int numArgs, char* request)
{
  //create request struct
	Request* temp = (Request*) malloc(sizeof(Request));
  temp->requestID = id;
  temp->numArgs = numArgs;
  temp->request = request;

	head->next = temp;
	head->prev = temp;
	temp->next = head;
	temp->prev = head;
	head->requestID++;
	pthread_cond_broadcast(&list_cv);
  // printf("head.next.id = %d and head.next.request = %s\n", head->next->requestID, head->next->request);
	// printf("head.prev.id = %d and head.prev.request = %s\n", head->prev->requestID, head->prev->request);
}

Request pop(Request *head)
{
	if(head->requestID > 0)
	{
		Request r = *head->next;
		head->next = r.next;
		head->next->prev = head;
		head->requestID--;
		return r;
	}
	// return NULL;
}
