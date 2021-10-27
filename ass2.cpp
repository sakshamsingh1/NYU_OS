#include <iostream>
#include <queue>
#include <fstream>
#include <sstream>
#include <map>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <utility>
#include <list>

using namespace std;

int MAX_PRIO=4;
//Process
class Process{
public:
    int id, arrival_time, total_cpu_time, max_cpu_burst, max_io_burst,
        rem_cpu_time, run_queue_in_time, static_priority,
        prev_cb, prev_io, io_block_time, cpu_wait_time, finish_time,
        dynamic_prio, next_event_ts, prev_run_time, supposed_to_sub, pp, pp_time;
};
class Event {
public:
    int exec_time, old_state, new_state, event_no;
    Process *process;
};
struct EventComparator {
    bool operator()(const Event event1, const Event event2) {
        if (event1.exec_time != event2.exec_time) {
            return event1.exec_time > event2.exec_time;
        }
        return event1.event_no > event2.event_no;
    }
};
priority_queue<Event, vector<Event>, EventComparator> eventQueue;

//Scehdulers
class baseScheduler
{
public:
    queue<Process*> RunQueue;
    virtual void add_process(Process *p){};
    virtual Process* get_next_process(){};
};

class FIFO : public baseScheduler{

     void add_process(Process *p){
        RunQueue.push(p);
    };
    Process* get_next_process(){
        if(RunQueue.size()==0) return nullptr;
        Process *next = RunQueue.front();
        RunQueue.pop();
        return next;
    };
private:
    queue<Process*> RunQueue;
};

class LCFS : public baseScheduler{
public :
    void add_process(Process *p){
        l_que.push_front(p);
    };

    Process* get_next_process(){
        if(l_que.size()==0) return nullptr;
        Process *next = l_que.front();
        l_que.pop_front();
        return next;
    };

private:
    deque<Process*> l_que;
};

class SRTF : public baseScheduler{
public :
    void add_process(Process *p){
        list<Process*>::iterator it;
        it = s_que.begin();
        while(it!=s_que.end() && (*it)->rem_cpu_time <= p->rem_cpu_time)
            {it++;}
        s_que.insert(it,p);
    };

    Process* get_next_process(){
        if(s_que.empty()) return nullptr;
        Process *next = s_que.front();
        s_que.pop_front();
        return next;
    };

private:
    list<Process*> s_que;
};

class RR : public baseScheduler{
    void add_process(Process *p){
        RunQueue.push(p);
    };
    Process* get_next_process(){
        if(RunQueue.size()==0) return nullptr;
        Process *next = RunQueue.front();
        RunQueue.pop();
        return next;
    };
private:
    queue<Process*> RunQueue;
};

class PRIO : public baseScheduler{

    void add_process(Process *p){
        int dp = p->dynamic_prio;
        if(p->dynamic_prio>=0){
            if(active_rq.size()<MAX_PRIO){
                for(int i=0;i<MAX_PRIO;i++){active_rq.push_back(queue<Process*>());}
            }
            active_rq[dp].push(p);
        }
        else{
            if(expired_rq.size()<MAX_PRIO){
                for(int i=0;i<MAX_PRIO;i++){expired_rq.push_back(queue<Process*>());}
            }
            p->dynamic_prio = p->static_priority-1;
            expired_rq[p->dynamic_prio].push(p);
        }
    };

    Process* get_next_process(){
        int i=MAX_PRIO-1;
        while(i>=0 && active_rq[i].empty()){i--;}
        if(i<0){
            vector<queue<Process*>> temp_rq = expired_rq;
            expired_rq = active_rq;
            active_rq = temp_rq;
        }

        i=MAX_PRIO-1;
        while(i>=0 && active_rq[i].empty()){i--;}
        if(i<0) return nullptr;

        Process *next = active_rq[i].front();
        active_rq[i].pop();
        return next;
    };

private:
    vector<queue<Process*>> active_rq;
    vector<queue<Process*>> expired_rq;
};

class PREPRIO : public baseScheduler{

    void add_process(Process *p){
        int dp = p->dynamic_prio;
        if(p->dynamic_prio>=0){
            if(active_rq.size()<MAX_PRIO){
                for(int i=0;i<MAX_PRIO;i++){active_rq.push_back(queue<Process*>());}
            }
            active_rq[dp].push(p);
        }
        else{
            if(expired_rq.size()<MAX_PRIO){
                for(int i=0;i<MAX_PRIO;i++){expired_rq.push_back(queue<Process*>());}
            }
            p->dynamic_prio = p->static_priority-1;
            expired_rq[p->dynamic_prio].push(p);
        }
    };

    Process* get_next_process(){
        int i=MAX_PRIO-1;
        while(i>=0 && active_rq[i].empty()){i--;}
        if(i<0){
            vector<queue<Process*>> temp_rq = expired_rq;
            expired_rq = active_rq;
            active_rq = temp_rq;
        }

        i=MAX_PRIO-1;
        while(i>=0 && active_rq[i].empty()){i--;}
        if(i<0) return nullptr;

        Process *next = active_rq[i].front();
        active_rq[i].pop();
        return next;
    };

private:
    vector<queue<Process*>> active_rq;
    vector<queue<Process*>> expired_rq;
};

// Simulator
enum STATE {CREATED, READY, RUNNING, BLOCKED, PREEMPT, FINISH};
char* states [] = {"CREATED", "READY", "RUNNG", "BLOCK", "PREEMPT","FINISH"};

vector<int> randvals;
int ofs=0;
int randSize;
map<int, map<string,int>> process_stats;
int vflag = 0;
int tflag = 0;
int eflag = 0;
int pflag = 0;
char *sflag = NULL;
vector<pair<int,int>> io_intervals;
int evt_cntr = 0;
baseScheduler *curr_sche;
int quantum = 4;
string s_name;

void createRanVals(string file){
    string line_;
    ifstream file_(file);
    int count = 0;
    int temp;
    char *p;

    if(file_.is_open()) {
        while (getline(file_, line_)) {
            if(count==0){
                randSize = strtol(line_.c_str(), &p, 10);
            }
            else{
                temp = strtol(line_.c_str(), &p, 10);
                randvals.push_back(temp);
            }
            count++;
        }
    }
    file_.close();
//    printf("Size of file: %u, Size of randvals: %lu",size,randvals.size());
}

//Event


int getBurst(int burst){
    int p = ofs;
    ofs++;
    if(ofs==randSize) ofs=0;
    return 1 + (randvals[p] % burst);
}

//Process
Process* create_process(string str){
    vector<int> process_list;
    int num;
    char* p;

    istringstream ss(str);
    string word;

    while (ss >> word)
    {
        num = strtol(word.c_str(), &p, 10);
        process_list.push_back(num);
    }

    Process *pro = new(Process);
    pro->arrival_time = (process_list)[0];
    pro->total_cpu_time = (process_list)[1];
    pro->max_cpu_burst = (process_list)[2];
    pro->max_io_burst = (process_list)[3];
    pro->static_priority = getBurst(MAX_PRIO);
    pro->dynamic_prio = pro->static_priority-1;

    pro->rem_cpu_time = (process_list)[1];
    pro->io_block_time = 0;
    pro->cpu_wait_time=0;
    pro->prev_cb=-1;
    pro->pp=-1;

    return pro;
}

////Priority Queue printer
//void showpq(priority_queue<Event , vector<struct Event >, EventComparator> gq)
//{
//    priority_queue<Event , vector<struct Event >, EventComparator> g = gq;
//    while (!g.empty()) {
//        printf("process : %u\n", g.top().process->arrival_time);
//        g.pop();
//    }
//    cout << '\n';
//}

void initialise(string file) {
    int line_no=0;
    string line_;
    string token;
    Event eve;
    Process *pro;

    // For checking eventQueue
    // priority_queue<Event, vector<struct Event >, EventComparator> *gq = &eventQueue;

    ifstream file_(file);
    if(file_.is_open()){
        while(getline(file_,line_)){

            pro = create_process(line_);
            pro->id = line_no;

            eve.exec_time = pro->arrival_time;
            eve.process = pro;
            eve.old_state = CREATED;
            eve.new_state = READY;
            eve.process->next_event_ts = pro->arrival_time;
            eve.event_no = evt_cntr; evt_cntr++;

            eventQueue.push(eve);

            line_no++;
        }
    }

    //initialise scheduler
    curr_sche = new PREPRIO();
    s_name = "PREPRIO";

//    if(sflag=="L"){ curr_sche = new LCFS();}
//    else if(sflag=="S"){ curr_sche = new SRTF();}
//    else if(sflag=="R"){ curr_sche = new RR();}
//    else if(sflag=="P"){ curr_sche = new PRIO();}
//    else if(sflag=="E"){ curr_sche = new PREPRIO();}
};

Event* get_event(){
    if(eventQueue.size()==0) return nullptr;
    Event e = eventQueue.top();
    eventQueue.pop();
    return &e;
}

void computeStats(Process *p){
    process_stats[p->id]["at"] = p->arrival_time;
    process_stats[p->id]["tc"] = p->total_cpu_time;
    process_stats[p->id]["cb"] = p->max_cpu_burst;
    process_stats[p->id]["io"] = p->max_io_burst;
    process_stats[p->id]["sp"] = p->static_priority;
    process_stats[p->id]["finish_time"] = p->finish_time;
    process_stats[p->id]["turn_around_time"] = p->finish_time - p->arrival_time;
    process_stats[p->id]["io_time"] = p->io_block_time;
    process_stats[p->id]["cpu_wait_time"] = p->cpu_wait_time;
}

//void createAndPushEventP(int prev_time, Process* p, STATE _old){
//    Event *eve = new Event();
//    eve->process = p;
//    eve->old_state = _old;
//    switch (_old) {
//        case READY:{
//            eve->new_state = RUNNING;
//            int temp = getBurst(p->max_cpu_burst);
//            eve->process->cpu_burst = getBurst(p->max_cpu_burst);
//            if(eve->process->cpu_burst >= eve->process->total_cpu_time){
//                eve->process->cpu_burst = eve->process->total_cpu_time;
//            }
//            eve->curr_time = prev_time;
//            break;
//        }
//        case RUNNING:{
//            //TODO:PREEMPT
//            if(eve->process->cpu_burst >= eve->process->total_cpu_time){
//                eve->curr_time += eve->process->cpu_burst;
//                computeStats(eve->curr_time,eve->process);
//                return;
//            }else{
//                eve->curr_time += eve->process->cpu_burst;
//                eve->process->io_burst = getBurst(p->max_io_burst);
//                eve->new_state = BLOCKED;
//            }
//            break;
//        }
//        case BLOCKED:{
//            eve->new_state = READY;
//            eve->curr_time += eve->process->io_burst;
//            break;
//        }
//        case PREEMPT:{
//            break;
//        }
//    }
//
//    eventQueue.push(*eve);
//}

//bool test_preempt(int p_dyn_prio, int p_next_event_ts, int curtime){
//    Process *temp = CURRENT_RUNNING_PROCESS;
//    if(CURRENT_RUNNING_PROCESS== nullptr) return false;
//    if(CURRENT_RUNNING_PROCESS->dynamic_prio < p_dyn_prio){
//        if(p_next_event_ts > curtime) return true;
//    }
//    return false;
//};

void Simulation() {
    Event *evt;
    int curr_time;
    bool CALL_SCHEDULER = false;
    Process *CURRENT_RUNNING_PROCESS = nullptr;

    Process *proc;

    //Testing
    priority_queue<Event, vector<struct Event >, EventComparator> *temp_eq = &eventQueue;

    while (!eventQueue.empty()) {
        evt = get_event();
        proc = evt->process;
        curr_time = evt->exec_time;
        //PRINT

        int pid = proc->id;

        switch(evt->new_state) {
            case READY:
            {
                //TODO: Consider premption case
                //CREATED->READY
                if(evt->old_state==CREATED){
                    printf("%u %u %u: %s -> %s\n",curr_time,pid,0,states[CREATED],states[READY]);
                }else{
                    printf("%u %u %u: %s -> %s\n",curr_time,pid,evt->process->prev_io,states[BLOCKED],states[READY]);
                }

                bool should_preempt=false;
                if(s_name=="PREPRIO"){
                    if(CURRENT_RUNNING_PROCESS==nullptr){should_preempt=false;}
                    else{
                        if(CURRENT_RUNNING_PROCESS->dynamic_prio < proc->dynamic_prio &&
                                CURRENT_RUNNING_PROCESS->next_event_ts > curr_time){
                        should_preempt=true;
                        }
                    }
                }

                if(s_name=="PREPRIO" && should_preempt){
                    printf("PRIO PREEMPTION \n");
                    priority_queue<Event, vector<Event>, EventComparator> new_eq;
                    Process* temp_p = new Process();
                    temp_p->id = CURRENT_RUNNING_PROCESS->id;

                    while(!eventQueue.empty()){
                        Event e = eventQueue.top();
                        eventQueue.pop();

                        if(e.process->id != temp_p->id){
                            new_eq.push(e);
                        }else{
                            int run_time = curr_time - CURRENT_RUNNING_PROCESS->prev_run_time;

                            Event eve;
                            eve.old_state = RUNNING;
                            eve.new_state = PREEMPT;
                            eve.process = CURRENT_RUNNING_PROCESS;
                            eve.exec_time = curr_time;
                            if(e.new_state==BLOCKED){
                                eve.process->prev_cb = eve.process->prev_cb -run_time;
                            }else{
                                eve.process->prev_cb = eve.process->prev_cb + CURRENT_RUNNING_PROCESS->supposed_to_sub-run_time;
                            }

                            eve.process->rem_cpu_time = eve.process->rem_cpu_time + CURRENT_RUNNING_PROCESS->supposed_to_sub - run_time;
                            eve.event_no = evt_cntr; evt_cntr++;
                            eve.process->next_event_ts=curr_time;
                            eve.process->pp=1;
                            eve.process->pp_time=run_time;
                            //eve.process->dynamic_prio = eve.process->dynamic_prio+1;
                            new_eq.push(eve);
                        }
                    }
                    eventQueue = new_eq;

                }

                proc->run_queue_in_time = curr_time;
                curr_sche->add_process(proc);
                CALL_SCHEDULER = true;
                break;
            }
            case RUNNING:
            {
                int cb_time;
                if(evt->process->prev_cb==-1){
                    cb_time = getBurst(evt->process->max_cpu_burst);
                    if(cb_time > evt->process->rem_cpu_time){
                        cb_time = evt->process->rem_cpu_time;
                    }
                }else{
                    cb_time = evt->process->prev_cb;
                }

                int temp = curr_time - evt->process->run_queue_in_time;

//                Process temp_p = *proc;
//                int temp_prio = evt->process->static_priority-1;
//                if(s_name=="PRIO"){
//                    printf("%u %u %u: %s -> %s cb=%u rem=%u prio=%u\n",
//                           curr_time,temp_p.id,curr_time-temp_p.run_queue_in_time,states[READY],states[RUNNING],
//                           cb_time,temp_p.rem_cpu_time,temp_p.dynamic_prio);
//                }else{
//                    printf("%u %u %u: %s -> %s cb=%u rem=%u prio=%u\n",
//                           curr_time,pid,temp,states[READY],states[RUNNING],
//                           cb_time,proc->rem_cpu_time,proc->static_priority-1);
//                }

//                printf("%u %u %u: %s -> %s cb=%u rem=%u prio=%u\n",
//                       curr_time,pid,temp,states[READY],states[RUNNING],
//                       cb_time,evt->process->rem_cpu_time,evt->process->static_priority-1);

                printf("%u %u %u: %s -> %s cb=%u rem=%u prio=%u\n",
                       curr_time,pid,temp,states[READY],states[RUNNING],
                       cb_time,evt->process->rem_cpu_time,evt->process->dynamic_prio);

                evt->process->cpu_wait_time = evt->process->cpu_wait_time + temp;

                if(cb_time>quantum){
                    evt->process->rem_cpu_time = evt->process->rem_cpu_time - quantum;
                    Event eve;
                    eve.process = evt->process;
                    eve.old_state = RUNNING;
                    eve.new_state = PREEMPT;
                    eve.exec_time = curr_time + quantum;
                    eve.process->prev_cb = cb_time-quantum;
                    eve.event_no = evt_cntr; evt_cntr++;
                    eve.process->next_event_ts = curr_time+quantum;
                    eve.process->supposed_to_sub = quantum;
                    eventQueue.push(eve);
                }
                else{
                    if (cb_time==evt->process->rem_cpu_time){
                        evt->process->rem_cpu_time = evt->process->rem_cpu_time - cb_time;
                        Event eve;
                        eve.process = evt->process;
                        eve.old_state = RUNNING;
                        eve.new_state = FINISH;
                        eve.exec_time = curr_time+cb_time;
                        eve.process->prev_cb = cb_time;
                        eve.process->next_event_ts = curr_time+quantum;
                        eve.process->supposed_to_sub = cb_time;
                        eventQueue.push(eve);
                    }else{
                        evt->process->rem_cpu_time = evt->process->rem_cpu_time - cb_time;
                        Event eve;
                        eve.process = evt->process;
                        eve.old_state = RUNNING;
                        eve.new_state = BLOCKED;
                        eve.exec_time = curr_time+cb_time;
                        eve.event_no = evt_cntr; evt_cntr++;
                        eve.process->prev_cb = cb_time;
                        eve.process->next_event_ts = curr_time+quantum;
                        eve.process->supposed_to_sub = cb_time;
                        eventQueue.push(eve);

                    }
                }

                break;
            }
            case BLOCKED:
            {
                //RUNNING->BLOCKED
                CURRENT_RUNNING_PROCESS = nullptr;
                int io_time = getBurst(evt->process->max_io_burst);
                int temp_cb = evt->process->prev_cb;

                Event eve;
                eve.old_state = BLOCKED;
                eve.new_state = READY;
                eve.exec_time = curr_time + io_time;
                eve.process = evt->process;
                eve.process->prev_io = io_time;
                eve.process->io_block_time = eve.process->io_block_time + io_time;
                eve.event_no = evt_cntr; evt_cntr++;
                eve.process->dynamic_prio = eve.process->static_priority-1;
                eve.process->prev_cb = -1;
                eventQueue.push(eve);

                io_intervals.push_back({curr_time, curr_time + io_time});

                CALL_SCHEDULER = true;

                printf("%u %u %u: %s -> %s ib=%u rem=%u\n",
                       curr_time,pid,temp_cb,states[RUNNING],states[BLOCKED],
                       eve.process->prev_io,eve.process->rem_cpu_time);

                break;
            }
            case PREEMPT:
            {
                //int temp = curr_time -
                if(evt->process->pp==1){
                    printf("%u %u %u: %s -> %s  cb=%u rem=%u prio=%u\n",
                           curr_time,pid,evt->process->pp_time,states[RUNNING],states[READY],
                           evt->process->prev_cb,evt->process->rem_cpu_time,evt->process->dynamic_prio);
                    evt->process->pp=-1;
                }else{
                    printf("%u %u %u: %s -> %s  cb=%u rem=%u prio=%u\n",
                           curr_time,pid,quantum,states[RUNNING],states[READY],
                           evt->process->prev_cb,evt->process->rem_cpu_time,evt->process->dynamic_prio);
                }


                evt->process->run_queue_in_time = curr_time;
                evt->process->dynamic_prio = evt->process->dynamic_prio-1;
                curr_sche->add_process(evt->process);
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;
                break;
            }

            case FINISH:
            {
                CURRENT_RUNNING_PROCESS = nullptr;
                CALL_SCHEDULER = true;
                printf("%u %u %u: Done\n",curr_time,evt->process->id,evt->process->prev_cb);
                evt->process->finish_time = curr_time;
                computeStats(evt->process);
                break;
            }
        }

        if(CALL_SCHEDULER) {
            if (eventQueue.size()!=0 && eventQueue.top().exec_time == curr_time) {
                continue;
            }
            CALL_SCHEDULER = false;
            if (CURRENT_RUNNING_PROCESS == nullptr) {
                CURRENT_RUNNING_PROCESS = curr_sche->get_next_process();
                if (CURRENT_RUNNING_PROCESS != nullptr){

                    Event eve;
                    eve.old_state = READY;
                    eve.new_state = RUNNING;
                    eve.process = CURRENT_RUNNING_PROCESS;
                    eve.exec_time = curr_time;
                    eve.event_no = evt_cntr; evt_cntr++;
                    eve.process->prev_run_time = curr_time;
                    eventQueue.push(eve);

                }
            }
        }
    }
}

//TODO: remove the printf in readArg to avoid error in cout
int readArg(int argc, char *argv[]){
    int index;
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "vteps:")) != -1)
        switch (c)
        {
            case 'v':
                vflag = 1;
                break;
            case 't':
                tflag = 1;
                break;
            case 'e':
                eflag = 1;
                break;
            case 'p':
                pflag = 1;
                break;
            case 's':
                sflag = optarg;
                break;
            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                return 1;
            default:
                abort ();
        }

//    for (auto itr = scheduler_map.find(sflag); itr != scheduler_map.end(); itr++) {
//        sche_flag = itr->second;
//    }

    printf ("vflag = %d, tflag = %d, evalue = %d, pvalue = %d, schedul = %s\n",
            vflag, tflag, eflag, pflag, sflag);

    for (index = optind; index < argc; index++)
        printf ("Non-option argument %s\n", argv[index]);
    return -1;
}

int get_io_util(){
    vector<pair<int,int>> ans;
    vector<pair<int,int>> *temp_ii = &io_intervals;
    sort(io_intervals.begin(), io_intervals.end());
    ans.push_back(io_intervals[0]);
    for (int i = 1; i < io_intervals.size(); i++) {
        if (ans.back().second < io_intervals[i].first) ans.push_back(io_intervals[i]);
        else
            ans.back().second = max(ans.back().second, io_intervals[i].second);
    }
    int sum = 0;
    for (int i = 0; i < ans.size(); i++) {sum+= ans[i].second-ans[i].first;}
    return sum;
}

void print_stats(){
    int last_event_ft = 0;
    int cpu_util = 0;
    int io_util_time = get_io_util();
    int num_proc = 0;
    int sum_ta_time=0;
    int sum_cw_time=0;
    for (auto i : process_stats)
    {
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",i.first,
                i.second["at"],i.second["tc"],i.second["cb"],i.second["io"],i.second["sp"],
                i.second["finish_time"],i.second["turn_around_time"],i.second["io_time"],i.second["cpu_wait_time"]);
        last_event_ft = max(last_event_ft,i.second["finish_time"]);
        cpu_util += i.second["tc"];
        num_proc++;
        sum_ta_time+=i.second["turn_around_time"];
        sum_cw_time+=i.second["cpu_wait_time"];
    }
    //compute summary stats

    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
           last_event_ft,((double)cpu_util*100)/last_event_ft,((double)io_util_time*100)/last_event_ft,
           (double)sum_ta_time/num_proc,(double)sum_cw_time/num_proc,(double)num_proc*100/last_event_ft);

}

void run(){
    string rfile = "/Users/sakshamsingh/Desktop/lab2_assign/mycode/rfile";
    createRanVals(rfile);
    string file = "/Users/sakshamsingh/Desktop/lab2_assign/mycode/input3";
    initialise(file);
    Simulation();
    print_stats();
}

int main(int argc, char *argv[]){
    readArg(argc,argv);
    run();
    return 0;
}
