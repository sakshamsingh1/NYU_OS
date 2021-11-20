#include <iostream>
#include <queue>
#include <fstream>
#include <map>
#include <string>

using namespace std;

//constants and variables
const int MAX_FRAMES=16; //provided as input
const int MAX_VPAGES=64;
int num_proc = 0;
enum INST {R,W,C,E};
char* inst_print[] = {"r","w","c","e"};

//print helper
string SEGV = "SEGV";
string MAP = "MAP";
string ZERO = "ZERO";
string FIN = "FIN";
string IN = "IN";
string UNMAP = "UNMAP";
string OUT = "OUT";
string FOUT = "FOUT";
string SEGPROT = "SEGPROT";

//structs
struct pte_t {
    unsigned int present:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int page_frame_no:7;
    //--- Using other bits
    unsigned int valid:1;
    unsigned int file_mapped:1;
    unsigned int initialized:1;
    unsigned int other:17;
};

//Global stats

unsigned long ctx_switches = 0;
unsigned long process_exits = 0;
unsigned long long cost = 0;
unsigned long long inst_count = 0;

void print_h(string s){cout<<" "<<s<<endl;}

struct vma {int start_vp, end_vp, write_p, file_map;};

class Inst{
public:
    int vpage;
    INST operation;
};

struct Process {
    int pid;
    pte_t page_table[MAX_VPAGES];
    vector<vma> virt_mem;
    map<string,unsigned long> p_stats;
};

struct frame_t {
    int pid, vpage;
};

vector<Process*> proc_pool;
deque<int> free_frames;
frame_t frame_table[MAX_FRAMES];
queue<Inst> Q_ins;

class Pager{
public:
    virtual int select_victim_frame() = 0;
};

class FIFO : public Pager{
public:
    int head = 0;
    int select_victim_frame() override{
        int temp = head;
        head = (head+1)%MAX_FRAMES;
        return temp;
    }
};

class CLOCK : public Pager{
public:
    int head = 0;
    int select_victim_frame() override{
        
        while(proc_pool[frame_table[head].pid]->page_table[frame_table[head].vpage].referenced){
            proc_pool[frame_table[head].pid]->page_table[frame_table[head].vpage].referenced=0;
            head = (head+1)%MAX_FRAMES;
        }
        int temp = head;
        head = (head+1)%MAX_FRAMES;
        return temp;
    }
};



Pager *curr_pager;

void initialise(){
    for(int i=0;i<MAX_FRAMES;i++){
        free_frames.push_back(i);

        frame_t temp_ft;
        temp_ft.vpage = -1; temp_ft.pid = -1;
        frame_table[i] = temp_ft;
    }
    curr_pager = new CLOCK();
    inst_count = Q_ins.size();
}

int allocate_frame_from_free_list(){
    if(free_frames.empty()) return -1;
    int temp_front = free_frames.front(); free_frames.pop_front();
    return temp_front;
}

int get_frame_no(){
    int frame_no = allocate_frame_from_free_list();
    if (frame_no == -1) frame_no = curr_pager->select_victim_frame();
    return frame_no;
}

//TODO: zero processes

void readArgTest(int argc, char *argv[]){

    //readInpFile();
};

void readArg(int argc, char *argv[]){}

void read_in_file(string in_file){
//    queue<Inst> *temp_Q_in = &Q_ins;
    string line_;
    char *p;

    ifstream file_(in_file);
    if(file_.is_open()){
        while(getline(file_,line_)){
            if(line_[0]=='#') continue;

            num_proc = strtol(line_.c_str(), &p, 10);
            int temp_num_proc = 0;

            while(temp_num_proc<num_proc){
                getline(file_,line_);
                if(line_[0]=='#') continue;

                Process *curr_proc = new Process();
                curr_proc->pid = temp_num_proc;
                int num_vma = strtol(line_.c_str(), &p, 10);
                while(num_vma>0){
                    getline(file_,line_);
                    if(line_[0]=='#') continue;

                    int s_vp, e_vp, wp, fm;
                    sscanf(line_.c_str(),"%d %d %d %d",&s_vp, &e_vp, &wp, &fm);

                    vma temp_v;
                    temp_v.start_vp=s_vp;
                    temp_v.end_vp=e_vp;
                    temp_v.write_p = wp;
                    temp_v.file_map = fm;
                    curr_proc->virt_mem.push_back(temp_v);

                    num_vma--;
                }
                temp_num_proc++;

                curr_proc->p_stats["unmaps"] = 0;
                curr_proc->p_stats["maps"] = 0;
                curr_proc->p_stats["ins"] = 0;
                curr_proc->p_stats["outs"] = 0;
                curr_proc->p_stats["fins"] = 0;
                curr_proc->p_stats["fouts"] = 0;
                curr_proc->p_stats["zeros"] = 0;
                curr_proc->p_stats["segv"] = 0;
                curr_proc->p_stats["segprot"] = 0;

                proc_pool.push_back(curr_proc);
            }

            while(getline(file_,line_)){
                if(line_[0]=='#') continue;

                int temp; char c; Inst in;
                sscanf(line_.c_str(),"%c %d",&c,&temp);

                if(c=='r'){in.operation = R;}
                else if(c=='w'){in.operation = W;}
                else if(c=='c'){in.operation = C;}
                else{in.operation = E;}
                in.vpage = temp;
                Q_ins.push(in);
            }
            break;
        }
    }
}

void tester(){
    for(Process *it : proc_pool)
    {
        cout << "pid = " << it->pid << endl;
        for(vma it_v : it->virt_mem){
            printf("Start_page : %u, end_page : %u, write_map : %u, file_map : %u\n",
                   it_v.start_vp, it_v.end_vp, it_v.write_p, it_v.file_map);
        }
    }
    cout<<" "<<endl;
}

void scheduler(){
    frame_t (*temp_ft)[MAX_FRAMES] = &frame_table;
    Process *curr_proc = nullptr;
    int inst_no = -1;
    while (!Q_ins.empty()) {
        inst_no++;
        Inst curr_inst = Q_ins.front(); Q_ins.pop();

        //intialise if not yet
        if((curr_inst.operation==R || curr_inst.operation==W)
            && (curr_proc!= nullptr) &&  curr_proc->page_table[curr_inst.vpage].initialized==0){

            for(vma it_vma : curr_proc->virt_mem)
            {
                if (it_vma.start_vp<=curr_inst.vpage && curr_inst.vpage<=it_vma.end_vp){
                    curr_proc->page_table[curr_inst.vpage].valid=1;
                    if (it_vma.file_map==1){
                        curr_proc->page_table[curr_inst.vpage].file_mapped=1;
                    }
                    if (it_vma.write_p==1) {
                        curr_proc->page_table[curr_inst.vpage].write_protect = 1;
                    }
                    break;
                }
            }

            curr_proc->page_table[curr_inst.vpage].initialized=1;
        }

        printf("%d: ==> %s %d\n",inst_no,inst_print[curr_inst.operation],curr_inst.vpage);
        switch (curr_inst.operation) {
            case C:{
                curr_proc = proc_pool[curr_inst.vpage];
                ctx_switches++;
                break;
            }
            case E:{
                printf("EXIT current process %d\n",curr_proc->pid);
                int indx = 0;
                for(pte_t pt_it : curr_proc->page_table){
                    if(pt_it.present){
                        printf(" %s %d:%d\n",UNMAP.c_str(),curr_proc->pid,indx);
                        curr_proc->p_stats["unmaps"]++;
                        curr_proc->page_table[indx].present=0;
                        if (pt_it.modified && pt_it.file_mapped){
                            print_h(FOUT);
                            curr_proc->p_stats["fouts"]++;
                        }
                        free_frames.push_back(pt_it.page_frame_no);
                        frame_table[pt_it.page_frame_no].pid=-1;
                        frame_table[pt_it.page_frame_no].vpage=-1;
                    }
                    curr_proc->page_table[indx].paged_out=0;
                    indx++;
                }

                process_exits++;
                break;
            }
            case R:{
                pte_t *curr_pte = &curr_proc->page_table[curr_inst.vpage];
                if(!curr_pte->present){
                 //check for page fault
                 if(curr_pte->valid==0){
                     print_h(SEGV);
                     curr_proc->p_stats["segv"]++;
                     break;
                 }

                 //get free frame
                 int curr_frame_no = get_frame_no();
                 frame_t *curr_frame = &frame_table[curr_frame_no];
                 if(curr_frame->pid>=0){
                     printf(" %s %u:%u\n",UNMAP.c_str(),curr_frame->pid,curr_frame->vpage);

                     Process *out_proc = proc_pool[curr_frame->pid];
                     out_proc->p_stats["unmaps"]++;
                     if(out_proc->page_table[curr_frame->vpage].modified>0){
                         if(out_proc->page_table[curr_frame->vpage].file_mapped>0){
                             print_h(FOUT);
                             out_proc->p_stats["fouts"]++;
                         }else{
                             print_h(OUT);
                             out_proc->p_stats["outs"]++;
                         }
                         out_proc->page_table[curr_frame->vpage].paged_out=1;
                         out_proc->page_table[curr_frame->vpage].modified=0;
                     }

                     out_proc->page_table[curr_frame->vpage].present=0;
                 }

                 if(curr_pte->paged_out==1){
                     if(curr_pte->file_mapped==1){
                         print_h(FIN);
                         curr_proc->p_stats["fins"]++;
                     }
                     else{
                         print_h(IN);
                         curr_proc->p_stats["ins"]++;
                     }
                     //curr_pte->paged_out=0;
                 }
                else if(curr_pte->file_mapped==1){
                    print_h(FIN);
                     curr_proc->p_stats["fins"]++;
                }
                else {
                    print_h(ZERO);
                    curr_proc->p_stats["zeros"]++;
                }

                printf(" %s %u\n",MAP.c_str(),curr_frame_no);
                curr_proc->p_stats["maps"]++;

                curr_frame->pid = curr_proc->pid;
                curr_frame->vpage = curr_inst.vpage;

                curr_pte->present=1;
                curr_pte->page_frame_no = curr_frame_no;
                }

                curr_pte->referenced=1;
                break;
            }
            case W:{
                pte_t *curr_pte = &curr_proc->page_table[curr_inst.vpage];
                if(!curr_pte->present){
                    //check for page fault
                    if(curr_pte->valid==0){
                        print_h(SEGV);
                        curr_proc->p_stats["segv"]++;
                        break;
                    }

                    //get free frame
                    int curr_frame_no = get_frame_no();
                    frame_t *curr_frame = &frame_table[curr_frame_no];
                    if(curr_frame->pid>=0){
                        printf(" %s %u:%u\n",UNMAP.c_str(),curr_frame->pid,curr_frame->vpage);

                        Process *out_proc = proc_pool[curr_frame->pid];
                        out_proc->p_stats["unmaps"]++;
                        if(out_proc->page_table[curr_frame->vpage].modified>0){
                            if(out_proc->page_table[curr_frame->vpage].file_mapped>0){
                                print_h(FOUT);
                                out_proc->p_stats["fouts"]++;
                            }else{
                                print_h(OUT);
                                out_proc->p_stats["outs"]++;
                            }
                            out_proc->page_table[curr_frame->vpage].modified=0;
                            out_proc->page_table[curr_frame->vpage].paged_out=1;
                        }

                        out_proc->page_table[curr_frame->vpage].present=0;
                    }

                    if(curr_pte->paged_out==1){
                        if(curr_pte->file_mapped==1){
                            print_h(FIN);
                            curr_proc->p_stats["fins"]++;
                        }
                        else{
                            print_h(IN);
                            curr_proc->p_stats["ins"]++;
                        }
                        //curr_pte->paged_out=0;
                    }
                    else if(curr_pte->file_mapped==1){
                        print_h(FIN);
                        curr_proc->p_stats["fins"]++;
                    }
                    else {
                        print_h(ZERO);
                        curr_proc->p_stats["zeros"]++;
                    }

                    printf(" %s %u\n",MAP.c_str(),curr_frame_no);
                    curr_proc->p_stats["maps"]++;

                    curr_frame->pid = curr_proc->pid;
                    curr_frame->vpage = curr_inst.vpage;

                    curr_pte->present=1;
                    curr_pte->page_frame_no = curr_frame_no;
                }

                curr_pte->referenced=1;

                if(curr_pte->write_protect==1){
                    print_h(SEGPROT);
                    curr_proc->p_stats["segprot"]++;
                }else{
                    curr_pte->modified=1;
                }

                break;
            }
        }
    }

}

void print_results(){
    //Proc Table
    vector<Process*> temp_pp = proc_pool;

    for(Process* proc_it : proc_pool){
        string pt_str = "PT["+ to_string(proc_it->pid)+"]:";
        int idx = 0;
        for(pte_t pte_it : proc_it->page_table){
            if(pte_it.present==1){
                pt_str += " "+to_string(idx)+":";

                if(pte_it.referenced==1){
                    pt_str += "R";
                }else{pt_str += "-";}

                if(pte_it.modified==1){
                    pt_str += "M";
                }else{pt_str += "-";}

                if(pte_it.paged_out==1){
                    pt_str += "S";
                }else{pt_str += "-";}

            }else if(pte_it.paged_out==1) {
                pt_str += " #";
            } else{
                pt_str += " *";
            }
            idx++;
        }
        cout<<pt_str<<endl;
    }

    //Frame Table
    string ft_str = "FT:";
    for(frame_t ft_it : frame_table){
        if (ft_it.pid<0){
            ft_str += " *";
        }
        else{
            ft_str += " "+to_string(ft_it.pid)+":"+to_string(ft_it.vpage);
        }
    }

    cout<<ft_str<<endl;

    //Process stats
    for(Process *proc_it : proc_pool){
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
               proc_it->pid,
               proc_it->p_stats["unmaps"], proc_it->p_stats["maps"], proc_it->p_stats["ins"],
               proc_it->p_stats["outs"],
               proc_it->p_stats["fins"], proc_it->p_stats["fouts"], proc_it->p_stats["zeros"],
               proc_it->p_stats["segv"], proc_it->p_stats["segprot"]);

        cost += proc_it->p_stats["maps"]*300;
        cost += proc_it->p_stats["unmaps"]*400;
        cost += proc_it->p_stats["ins"]*3100;
        cost += proc_it->p_stats["outs"]*2700;
        cost += proc_it->p_stats["fins"]*2800;
        cost += proc_it->p_stats["fouts"]*2400;
        cost += proc_it->p_stats["zeros"]*140;
        cost += proc_it->p_stats["segv"]*340;
        cost += proc_it->p_stats["segprot"]*420;
    }

    cost += (inst_count-ctx_switches-process_exits);
    cost += ctx_switches*130;
    cost += process_exits*1250;

    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
           inst_count, ctx_switches, process_exits, cost, sizeof(pte_t));

}

int main(int argc, char *argv[]) {
    //readArg(argc,argv);
    readArgTest(argc,argv);
    string in_file = "/Users/sakshamsingh/Desktop/lab3/mmu/in11";
    read_in_file(in_file);
    initialise();
    scheduler();
    print_results();

    return 0;
}
