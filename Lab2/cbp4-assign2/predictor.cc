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
     Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]  = (( Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] << 1) & 0x3f) | resolveDir;
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

void InitPredictor_openend() {

}

bool GetPrediction_openend(UINT32 PC) {

  return TAKEN;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

}

