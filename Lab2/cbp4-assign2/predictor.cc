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

#define HASH_MATRIX_ROW_1 0b00010111001010000001010011011000
#define HASH_MATRIX_ROW_2 0b00111011010001000010001010100100
#define HASH_MATRIX_ROW_3 0b01111101100000100100000101000010
#define HASH_MATRIX_ROW_4 0b00000001000000011000000010000001

typedef struct TAG_DATA{
  UINT32 TAG;
  UINT32 DATA;
  UINT32 HITS;
} TAG_DATA;

//total 128Kib
static UINT32 bimodel_PHT[4096];
static UINT32 selector[4096];

#define TAGE_BHR_MASK 0xFFFFFFFF
#define TAGE_PHT1_MASK 0b0000000000001111
#define TAGE_PHT2_MASK 0b0000000011111111
#define TAGE_PHT3_MASK 0b1111111111111111

#define TAGE_PHT1_SIZE 4
#define TAGE_PHT2_SIZE 16
#define TAGE_PHT3_SIZE 64

static UINT32 TAGE_BHR;
static TAG_DATA TAGE_PHT1[32][TAGE_PHT1_SIZE];
static TAG_DATA TAGE_PHT2[32][TAGE_PHT2_SIZE];
static TAG_DATA TAGE_PHT3[32][TAGE_PHT3_SIZE];





void init_sel(){
  for(int i = 0; i < 4096; i++){
    selector[i] = W_NT;
  }
}

UINT32 selector_index(UINT32 PC){
  UINT32 index = (PC >> 2) & 0x00000fff;
  return index;
}

bool Get_sel(UINT32 pc){
  UINT32 index = selector_index (pc);
  switch(bimodel_PHT[index]){
    
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
}

int unary_xor(UINT32 num){
    int count = 0;
    while(num > 0){
        if(num & 1){
        count ++;
        }
        num = num >> 1;
        
    }
    return count%2;
}

UINT32 hash_func1(UINT32 pc){
  UINT32 index = 0;
  index |= pc & 0x4;
  index |= unary_xor(pc & HASH_MATRIX_ROW_1) << 1;
  index |= unary_xor(pc & HASH_MATRIX_ROW_2) << 2;
  index |= unary_xor(pc & HASH_MATRIX_ROW_3) << 3;
  index |= unary_xor(pc & HASH_MATRIX_ROW_4) << 4;
  return index;
}

UINT32 hash_func2(UINT32 index){
  UINT32 y1 = index & 0x00000001;
  UINT32 yn = index & 0x00002000;
  UINT32 changed = (index >> 1) |  yn;
  return y1 ^ changed;
}

void init_TAGE(){
  TAGE_BHR = 0;
  for (int i = 0; i < 16; i++){
    for (int j = 0; j < TAGE_PHT1_SIZE; j++){
      TAGE_PHT1[i][j].TAG = 0;
      TAGE_PHT1[i][j].DATA = W_NT;
      TAGE_PHT1[i][j].HITS = 0;
    }
    for (int j = 0; j < TAGE_PHT2_SIZE; j++){
      TAGE_PHT2[i][j].TAG = 0;
      TAGE_PHT2[i][j].DATA = W_NT;
      TAGE_PHT2[i][j].HITS = 0;
    }
    for (int j = 0; j < TAGE_PHT3_SIZE; j++){
      TAGE_PHT3[i][j].TAG = 0;
      TAGE_PHT3[i][j].DATA = W_NT;
      TAGE_PHT3[i][j].HITS = 0;
    }
  }
}

bool Getprediction_TAGE(UINT32 pc){
  UINT32 hashed_idx = hash_func2(pc) & 0x1F;
  UINT32 hashed_tag1 = hash_func1(TAGE_BHR & TAGE_PHT1_MASK);
  UINT32 hashed_tag2 = hash_func1(TAGE_BHR & TAGE_PHT2_MASK);
  UINT32 hashed_tag3 = hash_func1(TAGE_BHR & TAGE_PHT3_MASK);
  UINT32 result = TAKEN;

  for (int j = 0; j < TAGE_PHT1_SIZE; j++){
    if (TAGE_PHT1[hashed_idx][j].TAG == hashed_tag1) {
      result = TAGE_PHT1[hashed_idx][j].DATA;
      TAGE_PHT1[hashed_idx][j].HITS++;
    }
  }
  for (int j = 0; j < TAGE_PHT2_SIZE; j++){
    if (TAGE_PHT2[hashed_idx][j].TAG == hashed_tag2) {
      result = TAGE_PHT2[hashed_idx][j].DATA;
      TAGE_PHT2[hashed_idx][j].HITS++;
    }
  }
  for (int j = 0; j < TAGE_PHT3_SIZE; j++){
    if (TAGE_PHT3[hashed_idx][j].TAG == hashed_tag3) {
      result = TAGE_PHT3[hashed_idx][j].DATA;
      TAGE_PHT3[hashed_idx][j].HITS++;
    }
  }

  return result;
}

void UpdatePredictor_TAGE(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 hashed_idx = hash_func2(PC) & 0x1F;
  UINT32 hashed_tag1 = hash_func1(TAGE_BHR & TAGE_PHT1_MASK);
  UINT32 hashed_tag2 = hash_func1(TAGE_BHR & TAGE_PHT2_MASK);
  UINT32 hashed_tag3 = hash_func1(TAGE_BHR & TAGE_PHT3_MASK);
  
  UINT32 lowest_hits_idx = 0;
  UINT32 lowest_hits_value = TAGE_PHT1[hashed_idx][0].HITS;
  for (int j = 0; j < TAGE_PHT1_SIZE; j++){
    if (TAGE_PHT1[hashed_idx][j].HITS < lowest_hits_value){
      lowest_hits_idx = j;
    }

    if(resolveDir == TAKEN && TAGE_PHT1[hashed_idx][j].TAG == hashed_tag1 && TAGE_PHT1[hashed_idx][j].DATA != S_T){
      TAGE_PHT1[hashed_idx][j].DATA++;
      break;
    }
    if(resolveDir == NOT_TAKEN && TAGE_PHT1[hashed_idx][j].TAG == hashed_tag1 && TAGE_PHT1[hashed_idx][j].DATA != S_NT){
      TAGE_PHT1[hashed_idx][j].DATA--;
      break;
    }

    if (j == TAGE_PHT1_SIZE - 1) {
      TAGE_PHT1[hashed_idx][lowest_hits_idx].TAG = hashed_tag1;
      TAGE_PHT1[hashed_idx][lowest_hits_idx].DATA = resolveDir? W_T : W_NT;
      TAGE_PHT1[hashed_idx][lowest_hits_idx].HITS = 0;
    }
  }

  lowest_hits_idx = 0;
  lowest_hits_value = TAGE_PHT2[hashed_idx][0].HITS;
  for (int j = 0; j < TAGE_PHT2_SIZE; j++){
    if (TAGE_PHT2[hashed_idx][j].HITS < lowest_hits_value){
      lowest_hits_idx = j;
    }

    if(resolveDir == TAKEN && TAGE_PHT2[hashed_idx][j].TAG == hashed_tag2 && TAGE_PHT2[hashed_idx][j].DATA != S_T){
      TAGE_PHT2[hashed_idx][j].DATA++;
      break;
    }
    if(resolveDir == NOT_TAKEN && TAGE_PHT2[hashed_idx][j].TAG == hashed_tag2 && TAGE_PHT2[hashed_idx][j].DATA != S_NT){
      TAGE_PHT2[hashed_idx][j].DATA--;
      break;
    }

    if (j == TAGE_PHT2_SIZE - 1) {
      TAGE_PHT2[hashed_idx][lowest_hits_idx].TAG = hashed_tag2;
      TAGE_PHT2[hashed_idx][lowest_hits_idx].DATA = resolveDir? W_T : W_NT;
      TAGE_PHT2[hashed_idx][lowest_hits_idx].HITS = 0;
    }
  }

  lowest_hits_idx = 0;
  lowest_hits_value = TAGE_PHT3[hashed_idx][0].HITS;
  for (int j = 0; j < TAGE_PHT3_SIZE; j++){
    if (TAGE_PHT3[hashed_idx][j].HITS < lowest_hits_value){
      lowest_hits_idx = j;
    }

    if(resolveDir == TAKEN && TAGE_PHT3[hashed_idx][j].TAG == hashed_tag3 && TAGE_PHT3[hashed_idx][j].DATA != S_T){
      TAGE_PHT3[hashed_idx][j].DATA++;
      break;
    }
    if(resolveDir == NOT_TAKEN && TAGE_PHT3[hashed_idx][j].TAG == hashed_tag3 && TAGE_PHT3[hashed_idx][j].DATA != S_NT){
      TAGE_PHT3[hashed_idx][j].DATA--;
      break;
    }

    if (j == TAGE_PHT3_SIZE - 1) {
      TAGE_PHT3[hashed_idx][lowest_hits_idx].TAG = hashed_tag3;
      TAGE_PHT3[hashed_idx][lowest_hits_idx].DATA = resolveDir? W_T : W_NT;
      TAGE_PHT3[hashed_idx][lowest_hits_idx].HITS = 0;
    }
  }

  TAGE_BHR = ((TAGE_BHR << 1) | resolveDir) & TAGE_BHR_MASK;

}

UINT32 bimodel_index(UINT32 pc){
  UINT32 index = pc & 0x00000fff;
  return index;
}

void init_bimodel(){
  for(int i = 0 ; i < 4096; i++){
    bimodel_PHT[i] = 0;
  }

}

bool Getperdiction_bimodel(UINT32 pc){
  UINT32 index = bimodel_index (pc);
  switch(bimodel_PHT[index]){
    
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
}

void UpdatePredictor_bimodel(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 index = bimodel_index (PC);
  

  if(resolveDir == TAKEN && bimodel_PHT[index] != S_T){
    bimodel_PHT[index]++;
  }
  if(resolveDir == NOT_TAKEN && bimodel_PHT[index] != S_NT){
    bimodel_PHT[index]--;
  }
}



void InitPredictor_openend() {
  init_bimodel();
  init_TAGE();
}

bool GetPrediction_openend(UINT32 PC) {
  bool result_tage = Getprediction_TAGE(PC);
  bool result_bimodel = Getperdiction_bimodel(PC);
  return result_tage;
  if(selector[selector_index(PC)] > W_NT){
    return result_bimodel;
  }else{
    return result_tage;
  }
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UpdatePredictor_bimodel(PC, resolveDir, predDir, branchTarget);
  UpdatePredictor_TAGE(PC, resolveDir, predDir, branchTarget);
  bool result_tage = Getprediction_TAGE(PC);
  bool result_bimodel = Getperdiction_bimodel(PC);

  if(result_bimodel == resolveDir && result_tage != resolveDir && selector[selector_index(PC)] != S_T){
    selector[selector_index(PC)] ++;
  }
  if(result_bimodel != resolveDir && result_tage == resolveDir && selector[selector_index(PC)] != S_NT){
    selector[selector_index(PC)] --;
  }

}


