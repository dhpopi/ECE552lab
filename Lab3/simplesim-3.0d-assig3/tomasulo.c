
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         16

#define RESERV_INT_SIZE    5
#define RESERV_FP_SIZE     3
#define FU_INT_SIZE        3
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     5
#define FU_FP_LATENCY      7

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */


/* ECE552 Assignment 3 - BEGIN CODE */
#define INT_EXECUTION_DONE(cur_cyc, start_cyc) (cur_cyc >= (start_cyc + FU_INT_LATENCY))
#define FP_EXECUTION_DONE(cur_cyc, start_cyc) (cur_cyc >= (start_cyc + FU_FP_LATENCY))

#define TAG_CLEAR(inst) (inst != NULL && inst->Q[0] == NULL && inst->Q[1] == NULL && inst->Q[2] == NULL)

#define IS_BRANCH(inst) (IS_UNCOND_CTRL(inst->op) || IS_COND_CTRL(inst->op))

void rm_from_RS(instruction_t* inst){

  if (USES_INT_FU(inst->op)){ // if instruction is in INT RS
    for (int idx = 0; idx < RESERV_INT_SIZE; idx++){
      if (reservINT[idx] == inst){
        reservINT[idx] = NULL;
        break;
      }
    }

  } else if (USES_FP_FU(inst->op)){ // if instruction is in FP RS
    for (int idx = 0; idx < RESERV_FP_SIZE; idx++){
      if (reservFP[idx] == inst){
        reservFP[idx] = NULL;
        break;
      }
    }
  }
}

void broadcast_tags(instruction_t* inst){
  // if tag match in map table then clear the tag
  for (int idx = 0; idx < MD_TOTAL_REGS; idx++){
    if (map_table[idx] == inst){
      map_table[idx] = NULL;
    }
  }

  // if tag match in INT RS then clear the tag
  for (int idx = 0; idx < RESERV_INT_SIZE; idx++){
    for (int tag_idx = 0; tag_idx < 3; tag_idx++){
      if (reservINT[idx] != NULL && reservINT[idx]->Q[tag_idx] == inst){
        reservINT[idx]->Q[tag_idx] = NULL;
      }
    }
  }

  // if tag match in FP RS then clear the tag
  for (int idx = 0; idx < RESERV_FP_SIZE; idx++){
    for (int tag_idx = 0; tag_idx < 3; tag_idx++){
      if (reservFP[idx] != NULL && reservFP[idx]->Q[tag_idx] == inst){
        reservFP[idx]->Q[tag_idx] = NULL;
      }
    }
  }
}

void set_tags(instruction_t* inst){
  // set the RS table tag entries base on map table
  for (int idx = 0; idx < 3; idx++) {
    if (inst->r_in[idx] != DNA) {
      inst->Q[idx] = map_table[inst->r_in[idx]];
    }
  }

  // set the map table entries based on the instruction output registers number
  for (int idx = 0; idx < 2; idx++) {
    if (inst->r_out[idx] != DNA) {
      map_table[inst->r_out[idx]] = inst;
    }
  }
}

void IFQ_pop_front(){
  if (instr_queue_size > 0){
    // shift to the left
    for (int idx = 0; idx < INSTR_QUEUE_SIZE-1; idx++) {
      instr_queue[idx] = instr_queue[idx+1];
    }
    // set the last element to NULL
    instr_queue[INSTR_QUEUE_SIZE-1] = NULL;
    instr_queue_size--;
  }
}

/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */
  // D S X W 
  //check if all instuction is fetched

  if(fetch_index <= sim_insn){
    return false;
  }

  //check if all instr is depatched is finished
  if(instr_queue_size != 0){
    return false;
  }
  
  //check is all reservation is cleaned
  for(int i = 0; i < RESERV_FP_SIZE; i++){
    if(reservFP[i] != NULL){
      return false;
    }
  }

  for(int i = 0; i < RESERV_INT_SIZE; i++){
    if(reservINT[i] != NULL){
      return false;
    }
  }

  //check if the function unit is cleaned
  for(int i = 0; i < FU_INT_SIZE; i++){
    if(fuINT[i] != NULL){
      return false;
    }
  }
  
  for(int i = 0; i < FU_FP_SIZE; i++){
    if(fuFP[i] != NULL){
      return false;
    }
  }

  //check if CDB broadcast is finished
  if(commonDataBus != NULL){
    return false;
  }

  // check if all register value are written to the regfile
  for(int i = 0; i < MD_TOTAL_REGS; i++){
    if(map_table[i] != NULL){
      return false;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
  return true; //ECE552: you can change this as needed; we've added this so the code provided to you compiles
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */
  
  // if CDB is in use and the current cycle is after the cdb broadcast cycle then clear the map table and cdb
  if(commonDataBus != NULL && commonDataBus->tom_cdb_cycle < current_cycle){
    if(commonDataBus->r_out[0] != DNA){
      map_table[commonDataBus->r_out[0]] == NULL;
    }
    if(commonDataBus->r_out[1] != DNA){
      map_table[commonDataBus->r_out[1]] == NULL;
    }
    commonDataBus = NULL;
  }
  
  /* ECE552 Assignment 3 - END CODE */
}


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */
  instruction_t* oldest_complete_inst = NULL;
  instruction_t** oldest_complete_inst_fu_entry = NULL;

  // if there is not structual hazard with CDB
  if (commonDataBus == NULL){
    // check if any instruction in INT FU finished
    for (int idx = 0; idx < FU_INT_SIZE; idx++){
      if (fuINT[idx] != NULL && INT_EXECUTION_DONE(current_cycle, fuINT[idx]->tom_execute_cycle)){
        // check if the instruction need to use CDB
        if (WRITES_CDB(fuINT[idx]->op)) {
          // check if there is contention on the CDB and only let the oldest one use the CDB
          if (oldest_complete_inst == NULL){
            oldest_complete_inst = fuINT[idx];
            oldest_complete_inst_fu_entry = &(fuINT[idx]);
          } else {
            if (oldest_complete_inst->index > fuINT[idx]->index){
              oldest_complete_inst = fuINT[idx];
              oldest_complete_inst_fu_entry = &(fuINT[idx]);
            }
          }
        } else { // if the instruction does not use CDB then clear RS and FU entry
          rm_from_RS(fuINT[idx]);
          fuINT[idx] = NULL;
        }
        
      }
    }

    // check if any instruction in FP FU finished
    for (int idx = 0; idx < FU_FP_SIZE; idx++){
      if (fuFP[idx] != NULL && FP_EXECUTION_DONE(current_cycle, fuFP[idx]->tom_execute_cycle)){
        // check if the instruction need to use CDB
        if (WRITES_CDB(fuFP[idx]->op)) {
          // check if there is contention on the CDB and only let the oldest one use the CDB
          if (oldest_complete_inst == NULL){
            oldest_complete_inst = fuFP[idx];
            oldest_complete_inst_fu_entry = &(fuFP[idx]);
          } else {
            if (oldest_complete_inst->index > fuFP[idx]->index){
              oldest_complete_inst = fuFP[idx];
              oldest_complete_inst_fu_entry = &(fuFP[idx]);
            }
          }
        } else { // if the instruction does not use CDB then clear RS and FU entry
          rm_from_RS(fuFP[idx]);
          fuFP[idx] = NULL;
        }
      }
    }

    // clear RS and FU entry for the instruction that goes into the CDB
    if (oldest_complete_inst != NULL) {
      oldest_complete_inst->tom_cdb_cycle = current_cycle;
      commonDataBus = oldest_complete_inst;
      rm_from_RS(oldest_complete_inst);
      *oldest_complete_inst_fu_entry = NULL;
    }
  }
  /* ECE552 Assignment 3 - END CODE */

}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */

  instruction_t* oldest_issueable_inst = NULL;
  // check if INT FU is available
  for (int FU_idx = 0; FU_idx < FU_INT_SIZE; FU_idx++){
    if (fuINT[FU_idx] == NULL){
      oldest_issueable_inst = NULL;
      // Check if instructions in INT RS is issueable and issue the oldest one
      for (int RS_idx = 0; RS_idx < RESERV_INT_SIZE; RS_idx++){
        if (TAG_CLEAR(reservINT[RS_idx]) && reservINT[RS_idx]->tom_execute_cycle == 0 && current_cycle > reservINT[RS_idx]->tom_issue_cycle){
          if (oldest_issueable_inst == NULL){
            oldest_issueable_inst = reservINT[RS_idx];
          } else if (oldest_issueable_inst->index > reservINT[RS_idx]->index) {
            oldest_issueable_inst = reservINT[RS_idx];
          }
        }
      }
      
      if (oldest_issueable_inst == NULL){ // if there is no instruction issueable
        break;
      } else {  // issue the oldest one found
        fuINT[FU_idx] = oldest_issueable_inst;
        fuINT[FU_idx]->tom_execute_cycle = current_cycle;
      }
    }
  }
  
  // check if FP FU is available
  for (int FU_idx = 0; FU_idx < FU_FP_SIZE; FU_idx++){
    if (fuFP[FU_idx] == NULL){
      oldest_issueable_inst = NULL;
      // Check if instructions in FP RS is issueable and issue the oldest one
      for (int RS_idx = 0; RS_idx < RESERV_FP_SIZE; RS_idx++){
        if (TAG_CLEAR(reservFP[RS_idx]) && reservFP[RS_idx]->tom_execute_cycle == 0 && current_cycle > reservFP[RS_idx]->tom_issue_cycle){
          if (oldest_issueable_inst == NULL){
            oldest_issueable_inst = reservFP[RS_idx];
          } else if (oldest_issueable_inst->index > reservFP[RS_idx]->index) {
            oldest_issueable_inst = reservFP[RS_idx];
          }
        }
      }
      
      if (oldest_issueable_inst == NULL){ // if there is no instruction issueable
        break;
      } else {  // issue the oldest one found
        fuFP[FU_idx] = oldest_issueable_inst;
        fuFP[FU_idx]->tom_execute_cycle = current_cycle;
      }
    }
  }
  if (commonDataBus != NULL){
    // broadcast the tag and update the RS and map table
    broadcast_tags(commonDataBus);
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */

  // check if any instruction dispatched is not issued yet
  // get the first instruction in the IFQ
  instruction_t* new_inst = instr_queue[0]; 
  if (new_inst != NULL){
    // if the instruction are branch instruction then only set the dispatch cycle
    if (IS_BRANCH(new_inst)){
      IFQ_pop_front();
    } else if (USES_INT_FU(new_inst->op)){  // if uses INT FU then need to dispatch to INT RS
      for (int idx = 0; idx < RESERV_INT_SIZE; idx++){
        if (reservINT[idx] == NULL){
          reservINT[idx] = new_inst;
          reservINT[idx]->tom_issue_cycle = current_cycle;
          set_tags(reservINT[idx]);
          IFQ_pop_front();
          break;
        }
      }
    } else if (USES_FP_FU(new_inst->op)){
      for (int idx = 0; idx < RESERV_FP_SIZE; idx++){
        if (reservFP[idx] == NULL){
          reservFP[idx] = new_inst;
          reservFP[idx]->tom_issue_cycle = current_cycle;
          set_tags(reservFP[idx]);
          IFQ_pop_front();
          break;
        }
      }
    } else {
      IFQ_pop_front();
    }
  }

  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {

  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */
  
  instruction_t* new_inst = NULL;
  // if IFQ is not full and there is more instruction to be fetched then fetch instruction into the queue
  if (instr_queue_size < INSTR_QUEUE_SIZE && fetch_index <= sim_num_insn){
    // discard trap instructions
    do {
      fetch_index++;
      new_inst = get_instr(trace, fetch_index);
    } while(IS_TRAP(new_inst->op));
    
    // add the new instruction to IFQ
    if (new_inst != NULL){
      instr_queue[instr_queue_size] = new_inst;
      instr_queue_size++;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {

  fetch(trace);
  
  /* ECE552: YOUR CODE GOES HERE */
  /* ECE552 Assignment 3 - BEGIN CODE */
  if (instr_queue_size != 0){
    if(instr_queue[instr_queue_size-1]->tom_dispatch_cycle == 0){
      instr_queue[instr_queue_size-1]->tom_dispatch_cycle = current_cycle;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}


/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }
  
  int cycle = 1;
  while (true) {

    /* ECE552: YOUR CODE GOES HERE */
    /* ECE552 Assignment 3 - BEGIN CODE */
    CDB_To_retire(cycle);  
    execute_To_CDB(cycle);
    issue_To_execute(cycle);
    dispatch_To_issue(cycle);
    fetch_To_dispatch(trace, cycle); 

    if (is_simulation_done(sim_num_insn)) break;

    cycle++;
    }
  /* ECE552 Assignment 3 - END CODE */
  return cycle;
}
