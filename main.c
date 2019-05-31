//
//  main.c
//  cpu_scheduler
//
//  Copyright © 2019 조현규. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* 최대 프로세스 개수 */
#define MAX_PROCESS_CNT 20
/* Process 큐 ID */
#define PROCESS_QUEUE_ID 0
/* Job 큐 ID */
#define JOB_QUEUE_ID 1
/* Ready 큐 ID */
#define READY_QUEUE_ID 2
/* Waiting 큐 ID */
#define WAITING_QUEUE_ID 3
/* Completed 큐 ID */
#define COMPLETED_QUEUE_ID 4

/* boolean 타입 선언 */
typedef enum { false, true } bool;

/* mode 명칭 */
const char* mode_name[] = {"프로그램 종료", "FCFS", "Non-Preemptive SJF", "Preemptive SJB", "Non-Preemptive Priority", "Preemptive Priority", "Round Robin", "처음으로.."};
/* 이미 선택했던 모드 Array */
int chosen_mode[6];

/* process 구조체 선언 */
typedef struct Process process;
struct Process {
  int pid, execution_time, cpu_burst_time, io_burst_time, arrival_time, priority, cpu_running_time, io_running_time, waiting_time, turnaround_time;
};
/* 프로세스 개수 */
int process_cnt;

/* 큐 Array 선언 */
const char* queue_name[] = {"Process", "Job", "Ready", "Waiting", "Completed"};
int queue_size[5] = {0, 0, 0, 0, 0};
process *queue[5][MAX_PROCESS_CNT];

/* 스케줄링 시작 시간 */
int scheduling_start;
/* 스케줄링 종료 시간 */
int scheduling_end;
/* 현재 CPU에서 running 상태인 프로세스 */
process *running_proc = NULL;
/* running 프로세스가 running 상태가 된 시점부터 경과된 시간 */
int running_proc_time = 0;
/* cpu가 IDLE 상태였던 시간 */
int idle_time = 0;

/* evaluate 결과 구조체 선언 */
typedef struct Result result;
struct Result {
  int mode;
  float avg_waiting_time, avg_turnaround_time, cpu_utilization;
};
/* evaluation 결과 큐 선언 */
result *result_queue[6];
int result_size = 0;

/* 함수 선언부 */
/* -- 큐 관리 -- */
void init_queue(int queue_id);
int queue_index(int queue_id, int pid);
bool already_in_queue(int queue_id, int pid);
process *enqueue(int queue_id, process *proc);
process *dequeue(int queue_id, process *proc);
void flush_queue(int queue_id);
void print_queue(int queue_id);
void create_process(int pid);
void sort_process_queue(void);
void prepare_job_queue(void);
void flush_result_queue(void);

/* -- 스케줄링 알고리즘 -- */
process *fcfs(void);
process *sjf(bool preemptive);
process *priority(bool preemptive);
process *round_robin(int quantum);
void schedule(int mode, int quantum);

/* -- evaluate 및 결과 출력 -- */
void evaluate(int mode, int quantum);
void print_result(void);

/* -- 코드 설정 관리 -- */
bool already_chosen_mode(int mode);
void config(void);

/* -- 큐 관리 -- */
/* 큐 initialize */
void init_queue(int queue_id) {
  queue_size[queue_id] = 0;
  for(int i = 0; i < MAX_PROCESS_CNT; i++)
    queue[queue_id][i] = NULL;
}

/* 큐의 인덱스 값을 조회하여 리턴한다 */
int queue_index(int queue_id, int pid) {
  for(int i = 0; i < queue_size[queue_id]; i++)
    if(pid == queue[queue_id][i]->pid) return i;
  
  return -1;
}

/* 큐에 프로세스가 존재하는지 확인한다 */
bool already_in_queue(int queue_id, int pid) {
  if(queue_index(queue_id, pid) == -1) return false;
  
  return true;
}

/* 큐에 프로세스를 삽입한다 */
process *enqueue(int queue_id, process *proc) {
  if(queue_size[queue_id] >= MAX_PROCESS_CNT) {
    printf("%s queue is full\n", queue_name[queue_id]);
    return NULL;
  }
  if(already_in_queue(queue_id, proc->pid)) {
    printf("Process#%d is already in %s queue\n", proc->pid, queue_name[queue_id]);
    return NULL;
  }
  
  queue[queue_id][queue_size[queue_id]] = proc;
  queue_size[queue_id]++;
  return proc;
}

/* 큐에서 프로세스를 제거한다 */
process *dequeue(int queue_id, process *proc) {
  if(queue_size[queue_id] <= 0) {
    printf("%s queue is empty\n", queue_name[queue_id]);
    return NULL;
  }
  if(!already_in_queue(queue_id, proc->pid)) {
    printf("Process#%d is not in %s queue\n", proc->pid, queue_name[queue_id]);
    return NULL;
  }
  
  int index = queue_index(queue_id, proc->pid);
  process *temp = queue[queue_id][index];
  
  for(int i = index; i < queue_size[queue_id] - 1; i++)
    queue[queue_id][i] = queue[queue_id][i+1];
  queue[queue_id][queue_size[queue_id] - 1] = NULL;
  queue_size[queue_id]--;
  
  return temp;
}

/* 사용이 끝난 큐를 플러시한다 */
void flush_queue(int queue_id) {
  for(int i = 0; i < queue_size[queue_id]; i++) {
    free(queue[queue_id][i]);
    queue[queue_id][i] = NULL;
  }
  queue_size[queue_id] = 0;
}

/* 큐 내의 프로세스 목록을 출력한다 */
void print_queue(int queue_id) {
  for(int i = 0; i < queue_size[queue_id]; i++){
    process *proc = queue[queue_id][i];
    printf("pid: %d | execution_time: %d | cpu_burst_time: %d | io_burst_time: %d | cpu_running_time: %d | io_running_time: %d |arrival_time: %d | priority: %d\n", proc->pid, proc->execution_time, proc->cpu_burst_time, proc->io_burst_time, proc->cpu_running_time, proc->io_running_time, proc->arrival_time, proc->priority);
  }
}

/* 새로운 프로세스를 생성하여 Process 큐에 삽입한다. */
void create_process(int pid) {
  process *temp = (process *)malloc(sizeof(process));
  temp->pid = pid;
  temp->execution_time = rand() % 6 + 5; // 5 ~ 10
  temp->cpu_burst_time = rand() % 5 + 1; // 1 ~ 5
  temp->io_burst_time = rand() % 5 + 1; // 1 ~ 5
  temp->arrival_time = rand() % 10; // 0 ~ 9
  temp->priority = rand() % 10 + 1; // 1 ~ 10
  temp->cpu_running_time = 0;
  temp->io_running_time = 0;
  temp->waiting_time = 0;
  temp->turnaround_time = 0;
  
  enqueue(PROCESS_QUEUE_ID, temp);
}

/* Process 큐를 프로세스의 arrival_time 기준으로 정렬 */
void sort_process_queue() {
  int i, j;
  process *remember;
  for (i = 1; i < queue_size[PROCESS_QUEUE_ID]; i++)
  {
    remember = queue[PROCESS_QUEUE_ID][(j=i)];
    while ( --j >= 0 && remember->arrival_time < queue[PROCESS_QUEUE_ID][j]->arrival_time ) {
      queue[PROCESS_QUEUE_ID][j+1] = queue[PROCESS_QUEUE_ID][j];
    }
    queue[PROCESS_QUEUE_ID][j+1] = remember;
  }
}

/* 정렬된 Process 큐 내의 프로세스들을 Job 큐에 넣는다 */
void prepare_job_queue() {
  flush_queue(JOB_QUEUE_ID);
  
  for(int i = 0; i < queue_size[PROCESS_QUEUE_ID]; i++) {
    process *job = (process *)malloc(sizeof(process));
    
    job->pid = queue[PROCESS_QUEUE_ID][i]->pid;
    job->execution_time = queue[PROCESS_QUEUE_ID][i]->execution_time;
    job->cpu_burst_time = queue[PROCESS_QUEUE_ID][i]->cpu_burst_time;
    job->io_burst_time = queue[PROCESS_QUEUE_ID][i]->io_burst_time;
    job->arrival_time = queue[PROCESS_QUEUE_ID][i]->arrival_time;
    job->priority = queue[PROCESS_QUEUE_ID][i]->priority;
    job->cpu_running_time = queue[PROCESS_QUEUE_ID][i]->cpu_running_time;
    job->io_running_time = queue[PROCESS_QUEUE_ID][i]->io_running_time;
    job->waiting_time = queue[PROCESS_QUEUE_ID][i]->waiting_time;
    job->turnaround_time = queue[PROCESS_QUEUE_ID][i]->turnaround_time;
    
    queue[JOB_QUEUE_ID][i] = job;
    queue_size[JOB_QUEUE_ID]++;
  }
}

/* 사용이 끝난 결과 큐를 플러시한다 */
void flush_result_queue() {
  for(int i = 0; i < result_size; i++) {
    free(result_queue[i]);
    result_queue[i] = NULL;
  }
  result_size = 0;
}

/* -- 스케줄링 알고리즘 -- */
/* First Come First Served 알고리즘 */
process *fcfs() {
  // 레디 큐가 비어있는 경우 현재 running 중인 프로세스 리턴
  if(!queue[READY_QUEUE_ID][0]) return running_proc;
  // non-preemptive이므로 running 프로세스가 존재하면 그대로 리턴
  if(running_proc) return running_proc;
  
  // 레디 큐에 가장 먼저 들어온 프로세스를 dequeue해서 리턴
  return dequeue(READY_QUEUE_ID, queue[READY_QUEUE_ID][0]);
}

/* Shortest Job First 알고리즘 */
process *sjf(bool preemptive) {
  // 레디 큐가 비어있는 경우 현재 running 중인 프로세스 리턴
  if(!queue[READY_QUEUE_ID][0]) return running_proc;
  
  process *shortest_proc = queue[READY_QUEUE_ID][0];
  // 레디 큐를 execution_time, arrival_time 기준으로 오름차순 정렬
  for(int i = 0; i < queue_size[READY_QUEUE_ID]; i++) {
    int shortest_remain = shortest_proc->execution_time;
    int next_remain = queue[READY_QUEUE_ID][i]->execution_time;
    int shortest_arrival = shortest_proc->arrival_time;
    int next_arrival = queue[READY_QUEUE_ID][i]->arrival_time;
    
    if(shortest_remain < next_remain) continue;
    else if(shortest_remain > next_remain) shortest_proc = queue[READY_QUEUE_ID][i];
    else if(shortest_arrival < next_arrival) shortest_proc = queue[READY_QUEUE_ID][i];
  }
  
  // 현재 running 상태인 프로세스가 없는 경우 레디 큐에서 shortest 프로세스를 dequeue해서 리턴
  if(!running_proc) return dequeue(READY_QUEUE_ID, shortest_proc);
  if(!preemptive) return running_proc;
  
  // Preemptive 로직
  int shortest_remain = shortest_proc->execution_time;
  int running_remain = running_proc->execution_time;
  int shortest_arrival = shortest_proc->arrival_time;
  int running_arrival = running_proc->arrival_time;
  
  if(shortest_remain > running_remain) return running_proc;
  else {
    // shortest와 running의 도착시간이 같은 경우 굳이 context switch 하지 않음
    if((shortest_remain == running_remain) && (shortest_arrival >= running_arrival))
      return running_proc;
    
    enqueue(READY_QUEUE_ID, running_proc);
    return dequeue(READY_QUEUE_ID, shortest_proc);
  }
}

/* 우선순위 알고리즘(priority 값이 작을수록 우선순위는 높음) */
process *priority(bool preemptive) {
  // 레디 큐가 비어있는 경우 현재 running 중인 프로세스 리턴
  if(!queue[READY_QUEUE_ID][0]) return running_proc;
  
  process *prior_proc = queue[READY_QUEUE_ID][0];
  // 레디 큐를 priority, arrival_time 기준으로 오름차순 정렬
  for(int i = 0; i < queue_size[READY_QUEUE_ID]; i++) {
    int prior_priority = prior_proc->priority;
    int next_priority = queue[READY_QUEUE_ID][i]->priority;
    int prior_arrival = prior_proc->arrival_time;
    int next_arrival = queue[READY_QUEUE_ID][i]->arrival_time;
    
    if(prior_priority < next_priority) continue;
    else if(prior_priority > next_priority) prior_proc = queue[READY_QUEUE_ID][i];
    else if(prior_arrival < next_arrival) prior_proc = queue[READY_QUEUE_ID][i];
  }
  
  // 현재 running 상태인 프로세스가 없는 경우 레디 큐에서 우선순위가 가장 높은 프로세스를 dequeue해서 리턴
  if(!running_proc) return dequeue(READY_QUEUE_ID, prior_proc);
  if(!preemptive) return running_proc;
  
  // Preemptive 로직
  int prior_priority = prior_proc->priority;
  int running_priority = running_proc->priority;
  int shortest_arrival = prior_proc->arrival_time;
  int running_arrival = running_proc->arrival_time;
  
  if(prior_priority > running_priority) return running_proc;
  else {
    // shortest와 running의 도착시간이 같은 경우 굳이 context switch 하지 않음
    if((prior_priority == running_priority) && (shortest_arrival >= running_arrival))
      return running_proc;
    
    enqueue(READY_QUEUE_ID, running_proc);
    return dequeue(READY_QUEUE_ID, prior_proc);
  }
}

/* Round Robin 알고리즘 */
process *round_robin(int quantum) {
  // 레디 큐가 비어있는 경우 현재 running 중인 프로세스 리턴
  if(!queue[READY_QUEUE_ID][0]) return running_proc;
  
  process *first_proc = queue[READY_QUEUE_ID][0];
  // running 프로세스가 없으면 레디 큐에 가장 먼저 들어온 프로세스를 dequeue해서 리턴
  if(!running_proc) return dequeue(READY_QUEUE_ID, first_proc);
  // running 시간이 퀀텀보다 작으면 계속 run
  if(running_proc_time < quantum) return running_proc;
  
  // running 프로세스를 레디 큐에 넣고 레디 큐에 가장 먼저 들어온 프로세스를 dequeue해서 리턴
  enqueue(READY_QUEUE_ID, running_proc);
  return dequeue(READY_QUEUE_ID, first_proc);
}

/* 프로세스를 모드에 맞게 스케줄링한다 */
void schedule(int mode, int quantum) {
  // 프로세스 큐의 프로세스 목록을 잡 큐로 이전. 잡 큐는 arrival_time 기준으로 정렬되어있음.
  prepare_job_queue();
  
  // schedule 시작 시작 값을 arrival_time의 최소값으로 지정
  scheduling_start = queue[JOB_QUEUE_ID][0]->arrival_time;
  
  // 간트 차트 출력 시작
  printf("[%s 알고리즘 스케줄링]\n", mode_name[mode]);
  printf("간트 차트 출력: \n");
  // clock 수 만큼 eter 돌면서 스케줄링 진행
  int clock = 0;
  while(queue_size[COMPLETED_QUEUE_ID] < process_cnt) {
    process *temp_proc = NULL;
           
    // 현재 clock에 도착한 프로세스를 Job queue에서 Ready queue로 이동
    for(int i = 0; i < queue_size[JOB_QUEUE_ID]; i++) {
      if(queue[JOB_QUEUE_ID][i]->arrival_time == clock) {
        temp_proc = dequeue(JOB_QUEUE_ID, queue[JOB_QUEUE_ID][i]);
        i--;
        
        enqueue(READY_QUEUE_ID, temp_proc);
      }
    }
    
    // 마지막 running 프로세스를 변수에 저장
    process *last_proc = running_proc;
    
    // 스케줄링 알고리즘을 이용하여 다음 running 프로세스 선정
    switch(mode) {
      case 1 : running_proc = fcfs(); break;
      case 2 : running_proc = sjf(false); break;
      case 3 : running_proc = sjf(true); break;
      case 4 : running_proc = priority(false); break;
      case 5 : running_proc = priority(true); break;
      case 6 : running_proc = round_robin(quantum); break;
      default : running_proc = NULL;
    }
    
    // 새로운 프로세스가 스케줄링되어 처리되는 경우 처리시간 초기화
    if(last_proc != running_proc) running_proc_time = 0;
    
    // 스케줄링되지 않은 레디 큐 내 프로세스 waiting_time, turnaround_time 증가
    for(int i = 0; i < queue_size[READY_QUEUE_ID]; i++) {
      if(!queue[READY_QUEUE_ID][i]) continue;
      
      queue[READY_QUEUE_ID][i]->waiting_time++;
      queue[READY_QUEUE_ID][i]->turnaround_time++;
    }
    
    // waiting queue 내 프로세스의 IO 작업 진행
    for(int i = 0; i < queue_size[WAITING_QUEUE_ID]; i++) {
      if(!queue[WAITING_QUEUE_ID][i]) continue;
      
      queue[WAITING_QUEUE_ID][i]->io_running_time++;
      queue[WAITING_QUEUE_ID][i]->turnaround_time++;
      
      // IO 작업 완료 시 레디 큐로 이동
      if(queue[WAITING_QUEUE_ID][i]->io_running_time >= queue[WAITING_QUEUE_ID][i]->io_burst_time) {
        // io_running_time을 0으로 재초기화
        queue[WAITING_QUEUE_ID][i]->io_running_time = 0;
        temp_proc = dequeue(WAITING_QUEUE_ID, queue[WAITING_QUEUE_ID][i]);
        i--;
        enqueue(READY_QUEUE_ID, temp_proc);
      }
    }

    // running 프로세스 CPU 작업 진행
    if(running_proc) {
      running_proc->execution_time--;
      running_proc->cpu_running_time++;
      running_proc->turnaround_time++;
      running_proc_time++;
      
      // 간트 차트 출력
      printf("[%d]:P%d - ", clock, running_proc->pid);
      
      // execution_time만큼의 모든 작업 완료 시 완료 큐로 이동
      if(running_proc->execution_time <= 0) {
        enqueue(COMPLETED_QUEUE_ID, running_proc);
        running_proc = NULL;
      }
      // cpu running 시간이 다 된 경우 cpu running 시간을 0으로 재초기화
      else if(running_proc->cpu_running_time >= running_proc->cpu_burst_time) {
        running_proc->cpu_running_time = 0;
        
        // io 작업이 필요한 프로세시인 경우 waiting 큐로 이동
        if(running_proc->io_burst_time >= 0) {
          enqueue(WAITING_QUEUE_ID, running_proc);
          running_proc = NULL;
        }
      }
    }
    else {
      idle_time++;
      // 간트 차트 출력
      printf("[%d]:IDLE - ", clock);
    }
    
    clock++;
  }
  // 간트 차트 종료
  printf("END\n");
  // 스케줄링 로직 종료 시간 저장
  scheduling_end = clock - 1;
  // 스케줄링 알고리즘 evaluate
  evaluate(mode, quantum);
  // running_proc, idle_time flush
  free(running_proc);
  running_proc = NULL;
  running_proc_time = 0;
  idle_time = 0;
  // 사용한 Job, Ready, Waiting, Completed 큐를 flush
  for(int i = 1; i < 5; i++)
    flush_queue(i);
}

/* -- Evaluate 및 결과 출력 -- */
/* 스케줄링 알고리즘 Evaluation */
void evaluate(int mode, int quantum) {
  float total_waiting = 0, total_turnaround = 0;
  float cpu_utilized_time = scheduling_end - idle_time;
  float total_time = scheduling_end - scheduling_start;
  
  for(int i = 0; i < queue_size[COMPLETED_QUEUE_ID]; i++) {
    total_waiting += queue[COMPLETED_QUEUE_ID][i]->waiting_time;
    total_turnaround += queue[COMPLETED_QUEUE_ID][i]->turnaround_time;
  }
  
  result *eval_result = (result *)malloc(sizeof(result));
  eval_result->mode = mode;
  eval_result->avg_waiting_time = total_waiting/queue_size[COMPLETED_QUEUE_ID];
  eval_result->avg_turnaround_time = total_turnaround/queue_size[COMPLETED_QUEUE_ID];
  eval_result->cpu_utilization = cpu_utilized_time/total_time * 100;
  result_queue[result_size] = eval_result;
  result_size++;
}

/* evaluation 결과 출력 */
void print_result() {
  for(int i = 0; i < result_size; i++)
    printf("[%s] avg_waiting_time: %.2f | avg_turnaround_time: %.2f | cpu_utilization: %.2f\n", mode_name[result_queue[i]->mode], result_queue[i]->avg_waiting_time, result_queue[i]->avg_turnaround_time, result_queue[i]->cpu_utilization);
}

/* -- 코드 설정 관리 -- */
/* 모드가 이미 선택된 모드인지 여부 */
bool already_chosen_mode(int mode) {
  for(int i = 0; i < 6; i++)
    if(chosen_mode[i] == mode) return true;
  
  return false;
}

/* CPU 스케줄링 설정 세팅 */
void config() {
  printf("\n");
  
  // 시간을 이용하여 난수 생성
  srand((unsigned)time(NULL));
  
  // chosen_mode 초기화
  for(int i = 0; i < 6; i++)
    chosen_mode[i] = -1;
  
  // queue 초기화
  for(int i = 0; i < 5; i++)
    init_queue(i);
  
  // result queue 플러시
  flush_result_queue();
  
  // process 개수를 5~10 중 랜덤 값으로 지정
  process_cnt = rand() % 6 + 5;
  for(int i = 0; i < process_cnt; i++)
    create_process(i+1);
  
  sort_process_queue();
  
  printf("Random Process Size: %d\n", queue_size[PROCESS_QUEUE_ID]);
  printf("Job queue (order by arrival_time asc): \n");
  print_queue(PROCESS_QUEUE_ID);
}

/* 메인 함수 */
int main() {
  int mode, quantum = 0;
  
  config();
  
  while(1) {
    printf("\n");
    for(int i = 0; i < 8; i++) {
      if(i == 0 || i == 7 || !already_chosen_mode(i))
        printf("[%d]: %s\n", i, mode_name[i]);
      else
        printf("[v]: %s\n", mode_name[i]);
    }
    printf("\n");
    printf("=> mode를 입력하세요: ");
    scanf("%d",&mode);
    while (getchar() != '\n');
    printf("\n===============================================\n");
    
    if(already_chosen_mode(mode)) {
      printf("[ERROR] 이미 선택했던 mode입니다.\n");
      printf("===============================================\n");
      continue;
    }
    
    switch(mode) {
      case 0 : exit(0);
      case 1 : chosen_mode[0] = 1; break;
      case 2 : chosen_mode[1] = 2; break;
      case 3 : chosen_mode[2] = 3; break;
      case 4 : chosen_mode[3] = 4; break;
      case 5 : chosen_mode[4] = 5; break;
      case 6 :
        chosen_mode[5] = 6;
        printf("=> Time quantum을 입력하세요(1~10): ");
        scanf("%d", &quantum);
        while (getchar() != '\n');
        if(quantum > 10 || quantum < 1) {
          chosen_mode[5] = -1;
          printf("[ERROR] 올바른 quantum 값을 입력해주세요.\n");
          continue;
        }
        break;
      case 7 :
        config();
        continue;
      default :
        printf("[ERROR] 올바른 모드를 입력해주세요.\n");
        printf("===============================================\n");
        continue;
    }
    
    schedule(mode, quantum);
    
    // evaluation 결과 출력
    printf("===============================================\n");
    printf("Evaluation 결과: \n\n");
    print_result();
    printf("\n");
    printf("===============================================\n");
  }
}
