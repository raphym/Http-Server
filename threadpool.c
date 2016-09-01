//#define DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#include "threadpool.h"

/////////////////////////////////////////////////////////
                        //create_threadPool
/////////////////////////////////////////////////////////
threadpool* create_threadpool(int num_threads_in_pool)
{
  int i,rc;
  if(num_threads_in_pool < 1 || num_threads_in_pool> MAXT_IN_POOL )
  {
    fprintf(stderr,"incorrect number of threads\n");
    return NULL;
  }

  //create the threadPool
  threadpool *pool = (threadpool*)malloc(sizeof(threadpool));
  if(pool==NULL)
  {
    perror("error of malloc to create the pool\n");
    return NULL;
  }

  //ThreadPool's values
  if(pthread_mutex_init(&(pool->qlock), NULL)!=0 || pthread_cond_init(&(pool->q_not_empty),NULL)!=0 || pthread_cond_init(&(pool->q_empty), NULL)!=0 )
    {
      fprintf(stderr, "Error to init mutex or condition\n");
      free(pool);
      return NULL;
    }

  pool->num_threads=num_threads_in_pool;
  pool->qsize=0;
  pool->qhead=NULL;
  pool->qtail=NULL;
  pool->shutdown=0;
  pool->dont_accept=0;

  pthread_t *threadsArr = (pthread_t*)malloc(num_threads_in_pool*sizeof(pthread_t));
  if(threadsArr==NULL)
  {
    perror("Error to malloc the arrays's threads \n");
    free(pool);
    return NULL;
  }
  for(i=0; i<num_threads_in_pool ; i++)
  {
    rc = pthread_create (&threadsArr[i], NULL, do_work ,(void*)pool);
    if(rc!=0)
    {
      fprintf(stderr, "Error to create the thread number : %d \n",i);
      pool->num_threads=i;
      destroy_threadpool(pool);
      return NULL;
    }
  }
  pool->threads=threadsArr;

  return pool;
}

/////////////////////////////////////////////////////////
                        //Dispatch
/////////////////////////////////////////////////////////
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
  if(from_me==NULL || dispatch_to_here==NULL)
  {
    fprintf(stderr, "Your function is NULL or your pointer to pool is NULL\n");
    return;
  }

  //lock the mutex
  if(pthread_mutex_lock(&(from_me->qlock))!=0)
  {
    fprintf(stderr, "Error to lock mutex\n");
    return ;
  }

  //Check if we don't want to destruct the pool
  if(from_me->dont_accept==1)
  {
    if(pthread_mutex_unlock(&(from_me->qlock))!=0)
    {
      fprintf(stderr, "Error to unlock mutex\n");
      return ;
    }
    return ;
  }

  //contruct the job
  work_t *work = (work_t*)malloc(sizeof(work_t));
  if(work==NULL)
    {
      perror("Error to malloc a new job\n");
      return;
    }
  //Values of the new job
  work->routine = dispatch_to_here;
  work->arg = arg;
  work->next=NULL;

  //insert the job into the queue
  //manage the list
  //if the queue is empty
  if(from_me->qsize==0)
  {
    from_me->qhead=work;
    from_me->qtail=work;
    from_me->qsize++;
    //the queue was empty so we have to update the status to not empty
    if(pthread_cond_signal(&(from_me->q_not_empty))!=0)
    {
      fprintf(stderr, "Error cond_signal\n");
      return ;
    }
  }

  else//there is already jobs in the queue
  {
    from_me->qtail->next=work;
    from_me->qtail=work;
    from_me->qsize++;
  }

  //unlock the mutex
  if(pthread_mutex_unlock(&(from_me->qlock))!=0)
  {
    fprintf(stderr, "Error to unlock mutex\n");
    return ;
  }
}

/////////////////////////////////////////////////////////
                        //Do_WORK
/////////////////////////////////////////////////////////
void* do_work(void* p)
{
  if(p==NULL)
  {
    fprintf(stderr, "Error of pointer in do_work\n");
    return NULL;
  }
  threadpool *p1=p;
  int ret;

  while(1)
  {
    #if defined(DEBUG)
    // compiled only when DEBUG exists
    printf("id: %lu\n",pthread_self());
    #endif

    //lock the mutex
    if(pthread_mutex_lock(&(p1->qlock))!=0)
    {
      fprintf(stderr, "Error to lock mutex\n");
      return NULL;
    }

    //if the destroy is active
    if(p1->shutdown==1)
    {
      //unlock the mutex
      if(pthread_mutex_unlock(&(p1->qlock))!=0)
      {
        fprintf(stderr, "Error to unlock mutex\n");
        return NULL;
      }
      return NULL;
    }

    //if there is no work,the queue is empty so go to sleep
    if(p1->qsize==0)
    {
      if(pthread_cond_wait(&(p1->q_not_empty), &(p1->qlock))!=0)
      {
        fprintf(stderr, "Error cond_wait\n");
        return NULL;
      }
    }

    //if the destroy process was activated when the thread slept
    if(p1->shutdown==1)
    {
      //unlock the mutex
      if(pthread_mutex_unlock(&(p1->qlock))!=0)
      {
        fprintf(stderr, "Error to unlock mutex\n");
        return NULL ;
      }
      return NULL;
    }

    //prepare the execution of the function
    //manage the list
    work_t *work = p1->qhead;
    if(work!=NULL)
    {
      p1->qhead=p1->qhead->next;
      p1->qsize--;

      //if the queue is empty so the tail is NULL
      if(p1->qsize==0)
        p1->qtail=NULL;
    }

    //if the work is the last job and we want to destroy
    if(p1->qsize == 0 && p1->dont_accept == 1 )
    {
      //the funct destroy can destroy the pool because the list is now empty
      if(pthread_cond_signal(&(p1->q_empty))!=0)
      {
        fprintf(stderr, "Error cond_signal\n");
        return NULL;
      }
    }

    //unlock the mutex
    if(pthread_mutex_unlock(&(p1->qlock))!=0)
    {
      fprintf(stderr, "Error to unlock mutex\n");
      return NULL;
    }

    //Do the work : execute the function
    if(work!=NULL)
      {
        ret = work->routine(work->arg);
        free(work);
      }
  }
}

/////////////////////////////////////////////////////////
                        //DESTROY
/////////////////////////////////////////////////////////
void destroy_threadpool(threadpool* destroyme)
{
  if(destroyme==NULL)
  {
    fprintf(stderr, "Your pointer to pool is NULL\n");
    return ;
  }

  int i;
  //lock the mutex
  if(pthread_mutex_lock(&(destroyme->qlock))!=0)
  {
    fprintf(stderr, "Error to lock mutex\n");
    return ;
  }

  //Don't accept new jobs in the list
  destroyme->dont_accept=1;
  //if the list is not empty so wait the end of the jobs
  if(destroyme->qsize!=0)
  {
    //wait that the list is empty
    if(pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock))!=0)
    {
      fprintf(stderr, "Error cond_wait\n");
      return ;
    }
  }
  //the pool is in distruction process
  destroyme->shutdown=1;//the dispatch will not insert new jobs

  //send signal to all the threads
  if(pthread_cond_broadcast(&(destroyme->q_not_empty))!=0)
  {
    fprintf(stderr, "Error cond_broadcast\n");
    return ;
  }

  //unlock the mutex
  if(pthread_mutex_unlock(&(destroyme->qlock))!=0)
  {
    fprintf(stderr, "Error to unlock mutex\n");
    return ;
  }

  //wait all the threads
  for(i=0 ; i< (destroyme->num_threads) ; i++ )
  {
    int rc = pthread_join(destroyme->threads[i], NULL);
      if(rc!=0)
      {
        perror("ERROR THREAD's JOIN\n");
        return ;
      }
  }

  //destroy all mutex and condition
  if(pthread_mutex_destroy(&(destroyme->qlock))!=0 || pthread_cond_destroy(&(destroyme->q_not_empty)) !=0 || pthread_cond_destroy(&(destroyme->q_empty))!=0)
  {
    fprintf(stderr, "Error to destroy all the mutex (mutex , cond)\n");
    return ;
  }

  //free the threads and the pool
  free(destroyme->threads);
  free(destroyme);
}
/////////////////////////////////////////////////////////
                        //END
/////////////////////////////////////////////////////////
