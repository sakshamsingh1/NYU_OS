#include <iostream>
#include <queue>
#include <fstream>

using namespace std;

//constants and variables
const int MAX_FRAMES=16; //provided as input
const int MAX_VPAGES=64;
int num_proc = 0;
enum INST {R,W,C,E};
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
    unsigned int other:18;
};

void print_h(string s){cout<<s<<endl;}

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

Pager *curr_pager;

void initialise(){
    for(int i=0;i<MAX_FRAMES;i++){
        free_frames.push_back(i);

        frame_t temp_ft;
        temp_ft.vpage = -1; temp_ft.pid = -1;
        frame_table[i] = temp_ft;
    }
    curr_pager = new FIFO();
}

int allocate_frame_from_free_list(){
    if(free_frames.empty()) return -1;
    return free_frames.front();
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
                    //updating valid bits for these
                    for(int i=s_vp;i<=e_vp;i++){
                        curr_proc->page_table[i].valid=1;
                        if(temp_v.file_map==1){
                            curr_proc->page_table[i].file_mapped=1;
                        }
                        if(temp_v.write_p==1){
                            curr_proc->page_table[i].write_protect=1;
                        }
                    }
                    curr_proc->virt_mem.push_back(temp_v);

                    num_vma--;
                }
                temp_num_proc++;
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
    Process *curr_proc = nullptr;
    int inst_no = -1;
    while (!Q_ins.empty()) {
        inst_no++;
        Inst curr_inst = Q_ins.front();
        printf("%d: ==> %c %d\n",inst_no,curr_inst.operation,curr_inst.vpage);
        switch (curr_inst.operation) {
            case C:{
                curr_proc = proc_pool[curr_inst.vpage];
                break;
            }
            case E:{

                break;
            }
            case R:{
                pte_t *curr_pte = &curr_proc->page_table[curr_inst.vpage];
                if(!curr_pte->present){
                 //check for page fault
                 if(curr_pte->valid==0){
                     cout<<SEGV<<endl;
                     break;
                 }

                 //get free frame
                 int curr_frame_no = get_frame_no();
                 frame_t *curr_frame = &frame_table[curr_frame_no];
                 if(curr_frame->pid>0){
                     printf("%s %u:%u",UNMAP.c_str(),curr_frame->pid,curr_frame->vpage);
                     Process *out_proc = proc_pool[curr_frame->pid];
                     if(out_proc->page_table[curr_frame->vpage].modified>0){
                         if(out_proc->page_table[curr_frame->vpage].file_mapped>0){
                             print_h(FOUT);
                         }else{
                             print_h(OUT);
                         }
                         out_proc->page_table[curr_frame->vpage].modified=0;
                         out_proc->page_table[curr_frame->vpage].paged_out=1;
                         out_proc->page_table[curr_frame->vpage].present=0;
                     }
                 }

                 if(curr_pte->paged_out==1){
                     if(curr_pte->file_mapped==1){print_h(FIN);}
                     else{print_h(IN);}
                 }
                else if(curr_pte->file_mapped==1){print_h(FIN);}
                else {print_h(ZERO);}

                printf("%s %u",MAP.c_str(),curr_frame_no);

                curr_frame->pid = curr_proc->pid;
                curr_frame->vpage = curr_inst.vpage;

                curr_pte->present=1;
                curr_pte->page_frame_no = curr_frame_no;
                curr_pte->referenced=1;
                }

                break;
            }
            case W:{
                pte_t *curr_pte = &curr_proc->page_table[curr_inst.vpage];
                if(!curr_pte->present){
                    //check for page fault
                    if(curr_pte->valid==0){
                        cout<<SEGV<<endl;
                        break;
                    }

                    //get free frame
                    int curr_frame_no = get_frame_no();
                    frame_t *curr_frame = &frame_table[curr_frame_no];
                    if(curr_frame->pid>0){
                        printf("%s %u:%u",UNMAP.c_str(),curr_frame->pid,curr_frame->vpage);
                        Process *out_proc = proc_pool[curr_frame->pid];
                        if(out_proc->page_table[curr_frame->vpage].modified>0){
                            if(out_proc->page_table[curr_frame->vpage].file_mapped>0){
                                print_h(FOUT);
                            }else{
                                print_h(OUT);
                            }
                            out_proc->page_table[curr_frame->vpage].modified=0;
                            out_proc->page_table[curr_frame->vpage].paged_out=1;
                            out_proc->page_table[curr_frame->vpage].present=0;

                        }
                    }

                    if(curr_pte->paged_out==1){
                        if(curr_pte->file_mapped==1){print_h(FIN);}
                        else{print_h(IN);}
                    }
                    else if(curr_pte->file_mapped==1){print_h(FIN);}
                    else {print_h(ZERO);}

                    printf("%s %u",MAP.c_str(),curr_frame_no);

                    curr_frame->pid = curr_proc->pid;
                    curr_frame->vpage = curr_inst.vpage;

                    curr_pte->present=1;
                    curr_pte->page_frame_no = curr_frame_no;
                    curr_pte->referenced=1;
                }

                break;
            }
        }
    }

}


int main(int argc, char *argv[]) {
    //readArg(argc,argv);
    readArgTest(argc,argv);
    string in_file = "/Users/sakshamsingh/Desktop/lab3/mmu/in7";
    read_in_file(in_file);
    initialise();
    scheduler();

    return 0;
}
