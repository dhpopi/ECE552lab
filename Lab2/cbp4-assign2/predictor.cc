#include "predictor.h"


#define S_NT 0  // Strongly Not Taken
#define W_NT 1  // Weakly Not Taken
#define W_T 2   // Weakly Taken
#define S_T 3   // Strongly Taken

#define TWO_BIT_SAT_PHT_IDX_MASK 0x00000FFF // mask for obtaining the index of PHT from PC for 2-bit saturating counter
#define TWO_LEVEL_PHT_IDX_MASK 0x00000007 // mask for obtaining the index of PHT from PC for 2-level predictor
#define TWO_LEVEL_BHT_IDX_MASK 0x00000FF8 // mask for obtaining the index of BHT from PC for 2-level predictor
#define TWO_LEVEL_BHT_BIT_MASK 0x0000003F // mask for updating the 6-bit entry of BHT for 2-level predictor

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

static UINT32 Two_bit_sat_PHT[4096];

void InitPredictor_2bitsat() {
  int idx;
  for (idx = 0; idx < 4096; idx++) {
    // Initialize all entries to weakly not taken
    Two_bit_sat_PHT[idx] = W_T;
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  // mask out the upper bits in PC to get the index of PHT
  switch(Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK]){

    case S_NT:
      return NOT_TAKEN;

    case W_NT:
      return NOT_TAKEN;
    
    case W_T:
      return TAKEN;
    
    case S_T:
      return TAKEN;

    default:
      return TAKEN;
  }

  return TAKEN;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  switch (resolveDir) {
    case TAKEN:
      // update the value in the PHT, shift the predict towards strongly Taken
      if (Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK] != S_T) Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK]++;
      break;
    case NOT_TAKEN:
      // update the value in the PHT, shift the predict towards strongly Not Taken
      if (Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK] != S_NT) Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK]--;
      break;
    default:
      break;
  }
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

static UINT32 Two_level_BHT[512];
static UINT32 Two_level_PHT[8][64];

void InitPredictor_2level() {
  int i, j;
  // Initialize all BHT entries to not taken
  for (i = 0; i < 512; i++) {
    Two_level_BHT[i] = NOT_TAKEN;
  }

  // Initialize all PHT entries to weakly not taken
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 64; j++) {
      Two_level_PHT[i][j] = W_NT;
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  // using PC and BHT value to index the PHT to find the corresponding prediction value
  switch(Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]]){
    
    case S_NT:
      return NOT_TAKEN;

    case W_NT:
      return NOT_TAKEN;
    
    case W_T:
      return TAKEN;
    
    case S_T:
      return TAKEN;

    default:
      return TAKEN;
  }

  return TAKEN;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  switch (resolveDir) {
    case TAKEN:
      if (Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]] != S_T) {
        // update the value in the PHT, shift the predict towards strongly Taken
        Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]]++;
        }
        // update the histroy bits in the BHT
      Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] = ((Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] << 1) | 0x1) & TWO_LEVEL_BHT_BIT_MASK;
      
      break;
    case NOT_TAKEN:
      if (Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]] != S_NT) {
        // update the value in the PHT, shift the predict towards strongly Not Taken
        Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]]--;
        }
        // update the histroy bits in the BHT
      Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] = (Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] << 1) & (TWO_LEVEL_BHT_BIT_MASK - 0b1);
      
      break;
    default:
      break;
  }
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////
UINT64 GHR;
//hyper table
#define T0_size 1000
//2bit sat base T0
static UINT32 bimodel[T0_size];
//hyper table
#define NUM_BANK 4
#define BANK_SIZE 1024
#define CYC_RES_U 100
//banks of TAGE
int cycle_count = 0;
int alt_pred = 0;

typedef struct tage_entry{
  UINT32 ctr; //counter
  UINT32 tag; // tag
  UINT32 u; //age counter
}tage_entry;

static tage_entry bank[NUM_BANK][BANK_SIZE];

UINT32 TAG_8bit(UINT32 PC, int history_bits){
  UINT32 his_temp1 =0;
  UINT32 his_temp2 =0;
  UINT32 pc_temp = 0;
  //pc csr
  for(int i = 0; i < 32/8; i++){
    pc_temp ^= PC >> (8*i);
  }
  //csr1
  for(int i = 0; i < history_bits / 8; i++){
    his_temp1 ^= GHR >> (8*i);
  }
  //csr2
  for(int i = 0; i < history_bits / 7; i++){
    his_temp2 ^= GHR >> (8*i);
  }

  return (pc_temp ^ his_temp1 ^ (his_temp2 << 1)) && 0xff;
}

UINT32 Index_10bit(UINT32 PC, int history_bits){
  UINT32 pc_temp = 0;
  UINT32 his_temp1 =0;
  //csr pc
  for(int i = 0; i < 4; i++){
    pc_temp ^= PC >> (8*i);
  }
  //csr his
  for(int i = 0; i < history_bits / 10; i++){
    his_temp1 ^= GHR >> (8*i);
  }

  return (pc_temp ^ his_temp1) & 0x3ff;
}


void InitPredictor_openend() {
  GHR = 0;
  for(int i = 0; i < NUM_BANK; i++){
    for (int j = 0; j < BANK_SIZE; j++){
      bank[i][j].ctr = 0;
      bank[i][j].u = 0;
      bank[i][j].tag = 0;
    }
  }
  for(int i = 0; i < T0_size; i++){
    //set bimodel to weak_NT
    bimodel[i] = 1;
  }
}

bool GetPrediction_openend(UINT32 PC) {
  //find index and tags
  UINT32 tag[4];
  UINT32 index[4];
  UINT32 his_bit[4] = {10, 20, 40, 60};
  //get predition and altern prediction
  int pred = 0;
  int pred_make = 0;
  
  int alt_pred_make =0
  for(int i = 0; i < 4; i++){
    tag[i] = TAG_8bit(PC, his_bit[i]);
    index[i] = Index_10bit(PC, his_bit[i]);
  }
  //from higter order bank to lower bank
  for(int i = NUM_BANK - 1; i > 0; i--){
    //bank entry hit
    if(bank[i][index[i]].tag == tag[i]){
      if(pred_make == 0){
        pred_make = 1;
        pred = bank[i][index[i]].ctr >> 1;
      }else if(alt_pred_make == 0;){
        alt_pred_make = 0;
        alt_pred = bank[i][index[i]].ctr >> 1;
      }
    }
  }
  if(perd_make == 0){
    //use bimodel
    pred = bimodel[(PC >> 2) % T0_size] > 1;
  }
  if(alt_pred_make == 0){
    alt_pred = bimodel[(PC >> 2) % T0_size] > 1;
  }
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  

  GHR = (GHR << 1) | resolveDir;
}


