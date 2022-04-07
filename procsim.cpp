#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <string>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
using namespace std;

#include "procsim.hpp"
string alu = "alu";
string lsu = "lsu";
string mul = "mul";
string load = "LOAD";
string store = "STORE";
#define DEBUG 1

// The helper functions in this #ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)




//
// TODO: Define any useful data structures and functions here
//
/*typedef enum {
    ALU,
    LSU,
    MUL,
} fu_t;*/
inst_t dummy_inst;
typedef struct {
    string fu;
    int id;
    bool ready;

} fu_t;

typedef struct {
    const char *fu;
    int8_t dest;
    uint64_t dest_tag;
    bool src1_ready;
    uint64_t src1_tag;
    uint64_t src1_value;
    bool src2_ready;
    uint64_t src2_tag;
    uint64_t src2_value; 
    inst_t inst;
} rs_t;

typedef struct {
    bool ready;
    uint64_t tag;
    uint64_t value;
    int8_t reg_num;
   
} rf_t;

typedef struct {
    bool ready;
    inst_t inst;
} alu_fu_t;

typedef struct {
    bool ready;
    inst_t inst;
    bool stalled;
    uint64_t cycle;
    uint64_t index;
    uint64_t cache_tag;
    bool hit;
} lsu_fu_t;

typedef struct {
    bool ready;
    inst_t inst_stage1;
    inst_t inst_stage2;
    inst_t inst_stage3;
} mul_fu_t;

std::list<inst_t> disp_q;
std::vector<rs_t> sched_q;
std::vector<inst_t> rob;
rf_t *messy_reg;
alu_fu_t *alu_fu;
lsu_fu_t *lsu_fu;
mul_fu_t *mul_fu;
FILE *outfile;

size_t num_rob_entries;
size_t num_schedq_entries_per_fu;
size_t dcache_c;
size_t num_alu_fus;
size_t num_mul_fus;
size_t num_lsu_fus;
size_t fetch_width;
size_t retire_width;
size_t sched_q_size;
size_t sched_q_inuse;
size_t alu_insue;
size_t mul_inuse;
size_t lsu_inuse;
size_t rob_inuse;
uint64_t new_tag = 32;
uint64_t curr_cycle = 0;
uint64_t num_cache_entry;
procsim_stats_t my_stat;
inline const char* toString(opcode_t v)
{
    switch (v)
    {
        case OPCODE_ADD:   return "alu";
        case OPCODE_BRANCH:   return "alu";
        case OPCODE_LOAD: return "lsu";
        case OPCODE_STORE:   return "lsu";
        case OPCODE_MUL:   return "mul";
        default:      return "[Unknown OS_type]";
    }
}

inline const char* get_fu(opcode_t v)
{
    switch (v)
    {
        case OPCODE_ADD:   return "ADD";
        case OPCODE_BRANCH:   return "BRANCH";
        case OPCODE_LOAD: return "LOAD";
        case OPCODE_STORE:   return "STORE";
        case OPCODE_MUL:   return "MUL";
        default:      return "[Unknown OS_type]";
    }
}

#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        //fprintf(outfile,"(none)");
    } else {
        //fprintf(outfile,"R%" PRId8, rx);
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction( inst_t *inst) {
    //static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    //fprintf(outfile,"opcode=%s, dest=", opcode_names[inst->opcode]);
    print_operand(inst->dest);
    //fprintf(outfile,", src1=");
    print_operand(inst->src1);
    //fprintf(outfile,", src2=");
    print_operand(inst->src2);
}

static void print_messy_rf(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            //fprintf(outfile,"    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_reg[regno].tag, messy_reg[regno].ready); // TODO: fix me
        } else if (!(regno & 0x3)) {
            //fprintf(outfile,"\n    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_reg[regno].tag, messy_reg[regno].ready); // TODO: fix me
        } else {
            //fprintf(outfile,", R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_reg[regno].tag, messy_reg[regno].ready); // TODO: fix me
        }
    }
    //fprintf(outfile,"\n");
}

static void print_schedq(void) {
    size_t printed_idx = 0;
    for (size_t i = 0; i < sched_q.size(); i++) { // TODO: fix me
        if (printed_idx == 0) {
            //fprintf(outfile,"    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", sched_q[i].inst.fired, sched_q[i].inst.tag, sched_q[i].src1_tag, sched_q[i].src1_ready, sched_q[i].src2_tag, sched_q[i].src2_ready); // TODO: fix me
        } else if (!(printed_idx & 0x1)) {
            //fprintf(outfile,"\n    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}",sched_q[i].inst.fired, sched_q[i].inst.tag, sched_q[i].src1_tag, sched_q[i].src1_ready, sched_q[i].src2_tag, sched_q[i].src2_ready); // TODO: fix me
        } else {
            //fprintf(outfile,", {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", sched_q[i].inst.fired, sched_q[i].inst.tag, sched_q[i].src1_tag, sched_q[i].src1_ready, sched_q[i].src2_tag, sched_q[i].src2_ready); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        //fprintf(outfile,"    (scheduling queue empty)");
    }
    //fprintf(outfile,"\n");
}

static void print_rob(void) {
    size_t printed_idx = 0;
    ////fprintf(outfile,"    Next tag to retire (head of ROB): %" PRIu64 "\n", rob[0].tag); // TODO: fix me
    for (size_t i = 0; i < rob.size(); i++) {
        if (printed_idx == 0) {
            //fprintf(outfile,"    { tag: %" PRIu64 ", interrupt: %d }", rob[i].tag,rob[i].interrupt ); // TODO: fix me
        } else if (!(printed_idx & 0x3)) {
            //fprintf(outfile,"\n    { tag: %" PRIu64 ", interrupt: %d }", rob[i].tag,rob[i].interrupt); // TODO: fix me
        } else {
            //fprintf(outfile,", { tag: %" PRIu64 " interrupt: %d }",rob[i].tag,rob[i].interrupt); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        //fprintf(outfile,"    (ROB empty)");
    }
    //fprintf(outfile,"\n");
}
#endif

//Implement cache here
//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t cycle_to_update;
} cache_entry;
cache_entry *dcache;
cache_entry *aux_cache;
void cache_init(){
    num_cache_entry = pow(2, (dcache_c-6));
    dcache = (cache_entry*)malloc(sizeof(cache_entry) * (num_cache_entry));
    aux_cache = (cache_entry*)malloc(sizeof(cache_entry) * (num_cache_entry));
    for (uint64_t i = 0; i < num_cache_entry ; i++) {
        dcache[i].tag = 0;
        dcache[i].valid = 0;
    }
}

void get_tag(uint64_t addr, uint64_t *tag, uint64_t *index) {
    uint64_t pos = 6;
    uint64_t mask = 1;
    uint64_t num_bits = dcache_c - 6;
    *index = ((( mask << num_bits)- mask ) & (addr >> (pos )));
    pos = dcache_c;
    mask = 1;
    num_bits = 64 - dcache_c;
    *tag = ((( mask << num_bits)- mask ) & (addr >> (pos )));
}
bool access_dcache(uint64_t addr, procsim_stats_t *stats){
    
    uint64_t tag;
    uint64_t index;
    get_tag(addr, &tag, &index);
    bool hit = false;
    if (dcache[index].valid && dcache[index].tag == tag ) {
        hit = true;
        //cout<<"hit\n";
    }
    if (!hit) {
        //do cycle thing herrrrrrrrrrrrrrrrrrrrreeeeeeeeeeeeeee
        aux_cache[index].cycle_to_update = curr_cycle + 9;
        aux_cache[index].tag = tag;
        aux_cache[index].valid = 1;  
        
    }
    return hit;
}

void update_dcache(uint64_t index, uint64_t tag, procsim_stats_t *stats){

    dcache[index].tag = tag;
    dcache[index].valid = 1;
}
//-----------------------------------------------------------------------------------------------
size_t my_rob_size = 0;

// Optional helper function which resets all state on an interrupt, including
// the dispatch queue, scheduling queue, the ROB, etc. Note the dcache should
// NOT be flushed here! The messy register file needs to have all register set
// to ready (and in a real system, we would copy over the values from the
// architectural register file; however, this is a simulation with no values to
// copy!)
static void flush_pipeline(void) {
    // TODO: fill me in
    //delete everything from dispatch queue and set inuse value
    //flushing FUs----------------------------------------------------
    for (size_t i = 0; i < num_alu_fus; i++) {
        alu_fu[i].ready = true;
        alu_fu[i].inst = dummy_inst;
    }
    for (size_t i = 0; i < num_lsu_fus; i++) {
        lsu_fu[i].cycle = 0;
        lsu_fu[i].stalled = false;
        lsu_fu[i].ready = true;
        lsu_fu[i].inst = dummy_inst;
        lsu_fu[i].cache_tag = -1;
        lsu_fu[i].index = -1;
        lsu_fu[i].hit = false;
    }
    for (size_t i = 0; i < num_mul_fus; i++) {
        mul_fu[i].ready = true;
        mul_fu[i].inst_stage1 = dummy_inst;
        mul_fu[i].inst_stage2 = dummy_inst;
        mul_fu[i].inst_stage3 = dummy_inst;
    }
    //reseting messy register file-------------------------------
    for (int i = 0; i < 32; i++) {
        messy_reg[i].ready = true;
        messy_reg[i].tag = new_tag;
        new_tag++;
    }
    for (size_t i = 0; i < num_cache_entry; i++) {
        aux_cache[i].cycle_to_update = 0;  
        aux_cache[i].valid = 0;
    }

    //free dispatch queue entries shit
    disp_q.clear();
    sched_q.clear();
    rob.clear();
    my_rob_size =0;

}

// Optional helper function which pops instructions from the head of the ROB
// with a maximum of retire_width instructions retired. (In a real system, the
// destination register value from the ROB would be written to the
// architectural register file, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring an interrupt, this function will set
// *retired_interrupt_out = true and return immediately. Note that in this
// case, the interrupt must be counted as one of the retired instructions.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_interrupt_out) {
    // TODO: fill me in
    uint64_t return_val = 0;
    rob_inuse = rob.size();
    int iter = min(retire_width, rob_inuse);
    for (int i = 0; i < iter; i ++) {
        //std::cout<<"checking rob inst\n";
            //cout<<"rob at 0"<<rob.at(0).ready<<"\n";
        if (rob.size() == 0) {
            *retired_interrupt_out = false;
            return return_val;
        }
        if (rob.at(0).ready) {
            //std::cout<<"retiring inst\n";
            //fprintf(outfile, "Retiring instruction with tag %" PRIu64, rob[0].tag);
            //fprintf(outfile, "\n");
            return_val++;
            if( rob[0].dest != -1) stats->arf_writes++; 
            stats->instructions_retired++;
            if(rob.at(0).interrupt) {
                rob.erase(rob.begin());
                my_rob_size--;
                *retired_interrupt_out = true;
                return return_val;
            }
            rob.erase(rob.begin());
            my_rob_size--;
        }
    }
    //delete stuff from the rob
    *retired_interrupt_out = false;
    return return_val;
}
void delete_elem(inst_t inst) {
    size_t inx = 0;
    for (size_t i = 0; i < sched_q.size(); i++) {
        if (sched_q[i].inst.tag == inst.tag) {
            inx = i;
            break;
        }
    }
    sched_q.erase(sched_q.begin() + inx);
}
// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the messy register file and
// scheduling queue. This function should remove an instruction from the
// scheduling queue when it has completed.
int *index_list ;

int num_completed = 0;
static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me in
    // loops through all the entries of the functional unit


    for (size_t i = 0; i < num_alu_fus; i++) {
        //if the position is in use, we set its value in the schedulue queu
        // to be true       
        if (!alu_fu[i].ready) {
            //std::cout<<"alu ready not\n";
            num_completed++;
            alu_fu[i].ready = true;
            //fprintf(outfile, "Instruction with tag %" PRIu64 " and dest reg=R%" PRIu64 " completing",alu_fu[i].inst.tag, (uint64_t)alu_fu[i].inst.dest  );
            //fprintf(outfile, "\n");
            //fprintf(outfile, "Inserting instruction with tag %" PRIu64 " into the ROB",alu_fu[i].inst.tag  );
            //fprintf(outfile, "\n");
            my_rob_size++;
            delete_elem(alu_fu[i].inst);
            //find all occurrences of the tag in the scheduling que and mark them as ready
            for (size_t j = 0; j < sched_q.size() ; j++) {
                rs_t rs = sched_q.at(j);
                if (rs.src1_tag == alu_fu[i].inst.tag) {
                    sched_q.at(j).src1_ready = true;
                } 
                if (rs.src2_tag == alu_fu[i].inst.tag) {
                    sched_q.at(j).src2_ready = true;
                } 
                if (rs.inst.tag == alu_fu[i].inst.tag) {
                    sched_q.at(j).inst.ready = true;
                } 

            }
            //find the tags in the messy register file and set the ready bit to true
            for (int j = 0; j < 32; j ++) {
                rf_t rf = messy_reg[j];
                if (rf.tag == alu_fu[i].inst.tag) {
                    messy_reg[j].ready = true;
                }
            }
            //delete stuff from sched q here and add to rob
            //mark instruction as ready in the rob
            for (size_t j = 0 ; j < rob.size(); j++) {
                if (rob.at(j).tag == alu_fu[i].inst.tag ) {
                   //std::cout<<"setting rob\n";
                    rob.at(j).ready = true;
                }
            }
        }
    }
    for (size_t i = 0; i < num_lsu_fus; i++) {
        //if the position is in use, we set its value in the schedulue queu
        // to be true
        if (!lsu_fu[i].ready && !lsu_fu[i].stalled ) {
            if (lsu_fu[i].stalled ) {
                //fprintf(outfile, "Got response from L2, updating dcache for read from instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", lsu_fu[i].inst.tag, lsu_fu[i].index, lsu_fu[i].cache_tag);
            }
            if (lsu_fu[i].hit ) {
                stats->dcache_reads++;
                //fprintf(outfile,"dcache hit for instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", lsu_fu[i].inst.tag, lsu_fu[i].index, lsu_fu[i].cache_tag);
            }
            lsu_fu[i].hit = false;
            lsu_fu[i].cycle = 0;
            lsu_fu[i].ready = true;
            num_completed++;
            //fprintf(outfile, "Instruction with tag %" PRIu64 " and dest reg=R%" PRIu64 " completing",lsu_fu[i].inst.tag, (uint64_t)lsu_fu[i].inst.dest  );
            //fprintf(outfile, "\n");
            //fprintf(outfile, "Inserting instruction with tag %" PRIu64 " into the ROB",lsu_fu[i].inst.tag  );
            //fprintf(outfile, "\n");
            my_rob_size++;
            delete_elem(lsu_fu[i].inst);
            //find all occurrences of the tag in the scheduling que and mark them as ready
            for (size_t j = 0; j < sched_q.size() ; j++) {
                rs_t rs = sched_q.at(j);
                if (rs.src1_tag == lsu_fu[i].inst.tag) {
                    sched_q.at(j).src1_ready = true;
                } 
                if (rs.src2_tag == lsu_fu[i].inst.tag) {
                    sched_q.at(j).src2_ready = true;
                } 
                if (rs.inst.tag == lsu_fu[i].inst.tag) {
                    sched_q.at(j).inst.ready = true;
                }
                
            }
            //find the tags in the messy register file and set the ready bit to true
            for (int j = 0; j < 32; j ++) {
                rf_t rf = messy_reg[j];
                if (rf.tag == lsu_fu[i].inst.tag) {
                    messy_reg[j].ready = true;
                }
            }
            //delete stuff from sched q here and add to rob

            //mark instruction as ready in the rob
            for (size_t j = 0 ; j < rob.size(); j++) {
                if (rob.at(j).tag == lsu_fu[i].inst.tag ) {
                    rob.at(j).ready = true;
                }
            }
        } else if (!lsu_fu[i].ready && lsu_fu[i].stalled) {
            if (lsu_fu[i].cycle == 1) {
                stats->dcache_read_misses++;
                //fprintf(outfile, "dcache miss for instruction with tag %" PRIu64 "(dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ", stalling for 9 cycles\n", lsu_fu[i].inst.tag, lsu_fu[i].index, lsu_fu[i].cache_tag);
            }
            if (lsu_fu[i].cycle == 9) {
                lsu_fu[i].stalled = false;
            }
            lsu_fu[i].cycle++;
        }
    }
    for (size_t i = 0; i < num_mul_fus; i++) {
        //if the position is in use, we set its value in the schedulue queue
        // to be true
        if (mul_fu[i].inst_stage3.valid != false) {
            num_completed++;
            //fprintf(outfile, "Instruction with tag %" PRIu64 " and dest reg=R%" PRIu64 " completing",mul_fu[i].inst_stage3.tag, (uint64_t)mul_fu[i].inst_stage3.dest  );
            //fprintf(outfile, "\n");
            //fprintf(outfile, "Inserting instruction with tag %" PRIu64 " into the ROB",mul_fu[i].inst_stage3.tag  );
            //fprintf(outfile, "\n");
            my_rob_size++;
            delete_elem(mul_fu[i].inst_stage3);
        }
            mul_fu[i].ready = true;
            //find all occurrences of the tag in the scheduling que and mark them as ready
            if (mul_fu[i].inst_stage3.dest != false) {
                for (size_t j = 0; j < sched_q.size() ; j++) {
                    rs_t rs = sched_q.at(j);
                    if (rs.src1_tag == mul_fu[i].inst_stage3.tag) {
                        sched_q.at(j).src1_ready = true;
                    } 
                    if (rs.src2_tag == mul_fu[i].inst_stage3.tag) {
                        sched_q.at(j).src2_ready = true;
                    } 
                    if (rs.inst.tag == mul_fu[i].inst_stage3.tag) {
                        sched_q.at(j).inst.ready = true;
                    }
                    
                }
                //find the tags in the messy register file and set the ready bit to true
                for (int j = 0; j < 32; j ++) {
                    rf_t rf = messy_reg[j];
                    if (rf.tag == mul_fu[i].inst_stage3.tag) {
                        messy_reg[j].ready = true;
                    }
                }
                //mark instruction as ready in the rob
                for (size_t j = 0 ; j < rob.size(); j++) {
                    if (rob.at(j).tag == mul_fu[i].inst_stage3.tag ) {
                        rob.at(j).ready = true;
                    }
                } 
            }
            
            mul_fu[i].inst_stage3 = mul_fu[i].inst_stage2;
            mul_fu[i].inst_stage2 = mul_fu[i].inst_stage1;
            mul_fu[i].inst_stage1 = dummy_inst;
            //delete stuff from sched q here and add to rob 
    }

    /*int count = 0;
    for (size_t i = 0; i < sched_q.size(); i++) {
        for (int j = 0; j < num_completed; j++) {
            if (instr_completed[j].tag == sched_q.at(i).inst.tag && sched_q.at(i).inst.ready) {
                index_list[count] = i;
                count++;
                break;
            }
        }
    }*/
   // //fprintf(outfile, "num completed %d\n",num_completed);
    /*for (int i = 0; i < num_completed; i++) {
        //cout <<"deleting index "<<index_list[i]<<"\n";
        cout << "about to delete inst with tag "<<instr_completed[i].tag<<" in position "<<index_list[i]<<" of the sched q\n";
        
        //delete_elem(instr_completed[i]);
        //sched_q.erase(sched_q.begin() + index_list[i]);
    }
    //cout<<"after loop\n ";*/
}



uint64_t *this_cycle_store_index;
// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions. Note that when multiple
// instructions are ready to fire in a given cycle, they must be fired in
// program order. Also, load and store instructions must be fired according to
// the modified TSO ordering described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {
    bool fire = false;
    num_completed = 0;
    memset(index_list,0, sched_q_size);
    //memset(instr_completed,0, sched_q_size);
    // TODO: fill me in
    //list that will be used to check for repeated index when we are doing a store
    memset(this_cycle_store_index,-1,sched_q_size);
    //dummy tag that will not be used
    uint64_t dummy_tag;
    //loops through the scheduling queue (reservation station) and fires instructions
    for (size_t j = 0; j < sched_q.size(); j++) {
        uint64_t index;
        //This current entry of the reservation station
        rs_t rs = sched_q.at(j);
        // check that both source registers in the reservation stations are ready
        // before moving on 
        if (rs.src1_ready && rs.src2_ready && rs.inst.fired != 1) {
            if (rs.fu == alu) { // if the current rs entry is an alu, we cheack if it's ready
            //and sets the current position to the instruction of the current rs entry instr
            //std::cout<<"in alu rob\n";
                for (size_t i = 0; i < num_alu_fus; i++) {
                    if (alu_fu[i].ready) {
                        //fprintf(outfile, "Firing instruction with tag: %" PRIu64 "!", rs.inst.tag);
                        //fprintf(outfile, "\n");
                        if (rs.inst.tag == 31905) {
                            uint64_t existing_inx;
                            uint64_t existing_tag;
                            get_tag(rs.inst.load_store_addr, &existing_tag, &existing_inx );
                            print_instruction(&rs.inst);
                            //fprintf(outfile, " the ready is %d tag %" PRIu64 " \n",rs.inst.ready, rs.inst.tag);
                        }
                        fire = true;
                        sched_q[j].inst.fired = true;
                        
                        alu_fu[i].inst =rs.inst;
                        alu_fu[i].ready = false;
                        break;
                    }
                }
            } 
            if (rs.fu == lsu) { //do cache shitttttttttttttttttttttttt fuckkkkkkkkkkkkkkkkkkkkkkkkkkkkkk!!!!!!!!!!!!!!!!!!!!!!
            //std::cout<<"in lsu rob\n";
            // if the rs entry fu is a load or store
                if ((rs.inst.opcode) == 4) { // if we are doing a load
               // std::cout<<"in load rob\n";
                       /*if (rs.inst.tag == 42040) {
                            uint64_t existing_inx;
                            uint64_t existing_tag;
                            get_tag(rs.inst.load_store_addr, &existing_tag, &existing_inx );
                            print_instruction(&rs.inst);
                            //fprintf(outfile, " src1 _ready %d, src2_ready %d cycle %" PRIu64 " tag %" PRIx64 " \n",rs.src1_ready, rs.src2_ready, curr_cycle, existing_tag);
                        }
                        if (rs.inst.tag == 42042) {
                            uint64_t existing_inx;
                            uint64_t existing_tag;
                            get_tag(rs.inst.load_store_addr, &existing_tag, &existing_inx );
                            print_instruction(&rs.inst);
                            //fprintf(outfile, " src1 _ready %d, src2_ready %d cycle %" PRIu64 " tag %" PRIx64 " \n",rs.src1_ready, rs.src2_ready, curr_cycle, existing_tag);
                        } */
                    for (size_t i = 0; i < num_lsu_fus; i++) {
                        
                         // check if there is any available fu
                        if (lsu_fu[i].ready) {
                            get_tag(rs.inst.load_store_addr, &dummy_tag, &index );
                            lsu_fu[i].cache_tag = dummy_tag;
                            lsu_fu[i].index = index;
                            //finding the current index if it's being used it this cycle
                            int found = 0;
                            for (size_t i = 0; i < j; i++) {
                                uint64_t existing_inx;
                                uint64_t existing_tag;
                                get_tag(sched_q[i].inst.load_store_addr, &existing_tag, &existing_inx );
                                if (existing_inx == index && (sched_q[i].inst.opcode == 4 || sched_q[i].inst.opcode == 5) ) {
                                    found = 1;
                                    /*if (rs.inst.tag == 42040) {
                                        print_instruction(&rs.inst);
                                        //fprintf(outfile, " src1 _ready %d, src2_ready %d cycle %" PRIu64 " old inst %" PRIu64 " \n",rs.src1_ready, rs.src2_ready, curr_cycle, sched_q[i].inst.tag);
                                    }
                                    if (rs.inst.tag == 42042) {
                                        print_instruction(&rs.inst);
                                        //fprintf(outfile, " src1 _ready %d, src2_ready %d cycle %" PRIu64 "\n",rs.src1_ready, rs.src2_ready, curr_cycle);
                                    }*/
                                }
                            }
                            if (found == 0) {
                                //assign the current instruction to the fu and sets the current fu position to false
                                //fprintf(outfile, "Firing instruction with tag: %" PRIu64 "!", rs.inst.tag);
                            fire = true;
                                
                                sched_q[j].inst.fired = true;
                                //fprintf(outfile, "\n");
                                lsu_fu[i].inst = rs.inst;
                                lsu_fu[i].ready = false;
                                //if there's a hit in the cache we set the stalled value that
                                //will be used in the exec stage to false
                                //stats->dcache_reads++;
                                if (access_dcache(rs.inst.load_store_addr, stats)) { // hit in cache 
                                    lsu_fu[i].stalled = false; 
                                    lsu_fu[i].hit = true; 
                                    

                                } else { // miss in cache
                                    //stats->dcache_read_misses++;
                                    lsu_fu[i].stalled = true;
                                    lsu_fu[i].cycle++;
                                }
                            }
                            break;
                        }
                    }         
                } else if (get_fu(rs.inst.opcode) == store) { //if we are doing a store
                //std::cout<<"in store rob\n";
                    for (size_t i = 0; i < num_lsu_fus; i++) {
                        //if we find a ready fu
                        if (lsu_fu[i].ready ) { 
                            //std::cout<<"lsu is ready rob\n";
                            get_tag(rs.inst.load_store_addr, &dummy_tag, &index );
                            //std::cout<<"got tag rob\n";
                            //finding the current index if it's being used it this cycle
                            //std::cout << std::hex << index << std::endl;
                            int found = 0;
                            for (size_t i = 0; i < j; i++) {
                               // std::cout<<"in loop rob\n";
                                uint64_t existing_inx;
                                uint64_t existing_tag;
                                get_tag(sched_q[i].inst.load_store_addr, &existing_tag, &existing_inx );
                                if (existing_inx == index  && (sched_q[i].inst.opcode == 4 || sched_q[i].inst.opcode == 5) ) {
                                    found = 1;
                                }
                            }
                            //std::cout<<"after loop\n";
                            if (found == 0) {
                                //fprintf(outfile, "Firing instruction with tag: %" PRIu64 "!", rs.inst.tag);
                            fire = true;
                                
                                sched_q[j].inst.fired = true;
                                //fprintf(outfile, "\n");
                                this_cycle_store_index[j] = index;
                                lsu_fu[i].inst = rs.inst;
                                lsu_fu[i].ready = false;
                            }   
                            break;
                        }
                    }
                }
            }
            if (rs.fu == mul) {
                for (size_t i = 0; i < num_mul_fus; i++) {
                    if (mul_fu[i].ready) {
                        //fprintf(outfile, "Firing instruction with tag: %" PRIu64 "!", rs.inst.tag);
                        fire = true;
                        sched_q[j].inst.fired = true;
                        //fprintf(outfile, "\n");
                        mul_fu[i].inst_stage1 =rs.inst;
                        mul_fu[i].ready = false;
                        break;
                    }
                }
            }
        }
        
    }
    if (!fire) stats->no_fire_cycles++;


}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB; effectively, dispatch allocates
// reservation stations and ROB space for each instruction dispatched and
// stalls if there is either is unavailable. Note the scheduling queue and ROB
// have configurable sizes. The PDF has details.
size_t stalls;
static void stage_dispatch(procsim_stats_t *stats) {
    // TODO: fill me in
    //if the scheduling queue is not full and the rob is not full
    if (rob.size() >= num_rob_entries && sched_q.size() < sched_q_size) stats->rob_stall_cycles++;
    if (sched_q.size() < sched_q_size && rob.size() < num_rob_entries ) {
        //loops through the dispatch queue and adds instructions to the
        //scheduling queue
        int inst_dispatched = 0;
        for (auto const& data : disp_q) {
            //if the queue is full, we break and exit the loop
            

            if (sched_q.size() == sched_q_size || rob.size() == num_rob_entries ) {
                if (rob.size() == num_rob_entries && sched_q.size() < sched_q_size) stats->rob_stall_cycles++;
                stalls++;
                break;
            }
            
            inst_dispatched++;
            //create a reservation station entry
            inst_t inst = data;
            inst.valid = true;
            inst.tag = new_tag;
            rs_t rs;
            rs.fu = toString(inst.opcode);//fix meeeeeeeeeeeeeeeeeeeeeeeeeeeeee
            rs.dest = inst.dest;
            rs.inst = inst;
            //fprintf(outfile, "Dispatching instruction ");
            print_instruction(&inst);
            //fprintf(outfile, ". Assigning tag=%" PRIu64 " and setting ", inst.tag);
            if (messy_reg[inst.src1].ready) {
                if (inst.src1 != -1) {
                    //fprintf(outfile, "src1_tag=%" PRIu64 ", src1_ready=%d",messy_reg[inst.src1].tag, messy_reg[inst.src1].ready);
                    rs.src1_ready = messy_reg[inst.src1].ready;
                    rs.src1_tag = messy_reg[inst.src1].tag;
                } else {
                    //fprintf(outfile, ", src1_ready=%d", true);
                    rs.src1_ready = true;
                    rs.src1_tag = 0;
                }

                
            } else   {
                if (inst.src1 != -1) {
                    //fprintf(outfile, "src1_tag=%" PRIu64 ", src1_ready=%d",messy_reg[inst.src1].tag, messy_reg[inst.src1].ready); 
                    rs.src1_ready = false;
                    rs.src1_tag = messy_reg[inst.src1].tag;
                } else {
                    //fprintf(outfile, ", src1_ready=%d", true);
                    rs.src1_ready = true;
                    rs.src1_tag = 0;
                }
                
            }
            
            if (messy_reg[inst.src2].ready) {
                if (inst.src2 != -1) {
                    //fprintf(outfile, "src2_tag=%" PRIu64 ", and src2_ready=%d",messy_reg[inst.src2].tag, messy_reg[inst.src2].ready);
                    rs.src2_ready = messy_reg[inst.src2].ready;
                    rs.src2_tag = messy_reg[inst.src2].tag;
                } else {
                    //fprintf(outfile, ", and src2_ready=%d", true);
                    rs.src2_ready = true;
                    rs.src2_tag = 0;
                }
                
            } else   {
                if (inst.src2 != -1) {
                    //fprintf(outfile, "src2_tag=%" PRIu64 ", src2_ready=%d",messy_reg[inst.src2].tag, messy_reg[inst.src2].ready);
                    rs.src2_ready = false;
                    rs.src2_tag = messy_reg[inst.src2].tag;
                } else {
                    //fprintf(outfile, ", and src2_ready=%d", true);
                    rs.src2_ready = true;
                    rs.src2_tag = 0;
                }
                
            }
            
            if (inst.dest != -1){
                messy_reg[inst.dest].tag = new_tag;
                messy_reg[inst.dest].ready = false;
                //fprintf(outfile, "\n");
                //fprintf(outfile, "Marking ready=%d and assigning tag=%" PRIu64 " for R%" PRIu64" in messy RF",messy_reg[inst.dest].ready,messy_reg[inst.dest].tag = new_tag, (uint64_t)inst.dest  );
            }
            rs.dest_tag = new_tag;
            //fprintf(outfile, "\n");
            new_tag++;
            sched_q.push_back(rs);
            //adds the instruction to the tail of the rob
            rob.push_back(inst);
        }
        for (int i = 0; i < inst_dispatched; i++) {
            disp_q.erase(disp_q.begin());
        }
    } else {
        stalls++;
    }

    // TODO: delete all dispatched instructions here

}


// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. To simplify the
// project, the dispatch queue is infinite in size.

static void stage_fetch(procsim_stats_t *stats) {
    // TODO: fill me in
    inst_t *my_inst;
    for (size_t i = 0; i < fetch_width; i++) {
        my_inst = (inst_t*)procsim_driver_read_inst();
        
        if (my_inst == NULL) break;
        my_inst->ready = false;
        my_inst->fired = false;
        my_inst->completed = false;
        disp_q.push_back(*my_inst);
        stats->instructions_fetched++;
        //fprintf(outfile, "Fetching instruction ");
        print_instruction(my_inst);
        //fprintf(outfile, ". Adding to dispatch queue");
        //fprintf(outfile,"\n");
    }
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    // TODO: fill me in
    outfile = fopen("outfile.txt","w");
    num_rob_entries = sim_conf->num_rob_entries;
    num_schedq_entries_per_fu = sim_conf->num_schedq_entries_per_fu;
    dcache_c = sim_conf->dcache_c;
    num_alu_fus = sim_conf->num_alu_fus;
    num_mul_fus = sim_conf->num_mul_fus;
    num_lsu_fus = sim_conf->num_lsu_fus;
    fetch_width = sim_conf->fetch_width;
    retire_width = sim_conf->retire_width;
    sched_q_size = num_schedq_entries_per_fu * (num_alu_fus + num_lsu_fus + num_mul_fus);
    alu_fu = (alu_fu_t*)malloc((num_alu_fus)*sizeof(alu_fu_t));
    mul_fu = (mul_fu_t*)malloc((num_mul_fus)*sizeof(mul_fu_t));
    lsu_fu = (lsu_fu_t*)malloc((num_lsu_fus)*sizeof(lsu_fu_t));
    messy_reg = (rf_t*)malloc((32) * sizeof(rf_t));
    this_cycle_store_index = (uint64_t*)malloc(sched_q_size*(sizeof(uint64_t)));
    index_list = (int*)malloc(sched_q_size*(sizeof(int)));
   //instr_completed = (inst_t*)malloc(sched_q_size*(sizeof(inst_t)));
    for (int i = 0; i < 32; i++) {
        messy_reg[i].ready = 1;
        messy_reg[i].tag = i;
    }
    dummy_inst.dest = -2;
    dummy_inst.interrupt =0;
    dummy_inst.load_store_addr = 0;
    dummy_inst.tag = 0;
    dummy_inst.valid = false;
    dummy_inst.completed = false;
    dummy_inst.fired = false;
    dummy_inst.ready = false;
    for (size_t i = 0; i < num_alu_fus; i++) {
        alu_fu[i].inst = dummy_inst;
        alu_fu[i].ready = true;
    }
    for (size_t i = 0; i < num_lsu_fus; i++) {
        lsu_fu[i].cycle = 0;
        lsu_fu[i].stalled = false;
        lsu_fu[i].ready = true;
        lsu_fu[i].inst = dummy_inst;
        lsu_fu[i].cache_tag = -1;
        lsu_fu[i].index = -1;
        lsu_fu[i].hit = false;
    }
    for (size_t i = 0; i < num_mul_fus; i++) {
        mul_fu[i].inst_stage1 = dummy_inst;
        mul_fu[i].inst_stage2 = dummy_inst;
        mul_fu[i].inst_stage3 = dummy_inst;
        mul_fu[i].ready = true;
    }
    cache_init();
    #ifdef DEBUG
    //fprintf(outfile, "\nScheduling queue capacity: %lu instructions\n", sched_q_size); // TODO: fix me
    //fprintf(outfile,"Initial messy RF state:\n");
    print_messy_rf();
    //fprintf(outfile,"\n");
    #endif
}
size_t rob_avg =0 ;
size_t disp_avg =0;
size_t sched_avg =0;
// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if an interrupt was retired by assigning true or false to
// *retired_interrupt_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_interrupt_out) {
    #ifdef DEBUG
    //fprintf(outfile,"================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    // stage_state_update() should set *retired_interrupt_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_interrupt_out);

    if (*retired_interrupt_out) {
        #ifdef DEBUG
        //fprintf(outfile,"%" PRIu64 " instructions retired. Retired interrupt, so flushing pipeline!\n", retired_this_cycle);
        #endif

        // After we retire an interrupt, flush the pipeline immediately and
        // then pick up where we left off in the next cycle.
        for (uint64_t i = 0; i < num_cache_entry; i++) {
            if (aux_cache[i].cycle_to_update == stats->cycles-1 && aux_cache[i].valid ) {
            //update_dcache(i, aux_cache[i].tag, stats);
            dcache[i].valid = 0;
            //printf("Updating index %" PRIx64 " in cycle %" PRIu64 " and tag %" PRIx64 " \n", i, curr_cycle, aux_cache[i].tag);
        }
    }
        stats->interrupts++;
        flush_pipeline();

    } else {
        #ifdef DEBUG
        //fprintf(outfile,"%" PRIu64 " instructions retired. Did not retire interrupt, so proceeding with other pipeline stages.\n", retired_this_cycle);
        #endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

    #ifdef DEBUG
    //fprintf(outfile,"End-of-cycle dispatch queue usage: %lu\n", 0UL); // TODO: fix me
    //fprintf(outfile,"End-of-cycle messy RF state:\n");
    print_messy_rf();
    //fprintf(outfile,"End-of-cycle scheduling queue state:\n");
    print_schedq();
    //fprintf(outfile,"End-of-cycle ROB state:\n");
    print_rob();
    //fprintf(outfile,"================================ End cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    for (uint64_t i = 0; i < num_cache_entry; i++) {
        if (aux_cache[i].cycle_to_update == stats->cycles && aux_cache[i].valid ) {
            update_dcache(i, aux_cache[i].tag, stats);
            //printf("Updating index %" PRIx64 " in cycle %" PRIu64 " and tag %" PRIx64 " \n", i, curr_cycle, aux_cache[i].tag);
        }
    }
    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;
    curr_cycle = stats->cycles;
    disp_avg += disp_q.size();
    rob_avg += my_rob_size;
    sched_avg += sched_q.size();
    stats->rob_max_usage = max(stats->rob_max_usage, rob.size());
    stats->dispq_max_usage = max(stats->dispq_max_usage, disp_q.size());
    stats->schedq_max_usage = max(stats->schedq_max_usage, sched_q.size());

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {
    // TODO: fill me in
    stats->dcache_reads = stats->dcache_reads + stats->dcache_read_misses;
    stats->dcache_read_miss_ratio = (stats->dcache_read_misses * 1.0)  / (stats->dcache_reads * 1.0);
    stats->dcache_read_aat = 1.0 + (stats->dcache_read_miss_ratio * 9.0);
    stats->dispq_avg_usage = disp_avg * 1.0 / stats->cycles ;
    stats->rob_avg_usage = rob_avg * 1.0 / stats->cycles;
    stats->schedq_avg_usage = sched_avg * 1.0 / stats->cycles;
    stats->ipc = stats->instructions_retired * 1.0 / stats->cycles;
}
