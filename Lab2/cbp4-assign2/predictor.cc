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
  if (resolveDir == TAKEN){
    // update the value in the PHT, shift the predict towards strongly Taken
    if (Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK] != S_T) Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK]++;
  } else {
    // update the value in the PHT, shift the predict towards strongly Not Taken
    if (Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK] != S_NT) Two_bit_sat_PHT[PC & TWO_BIT_SAT_PHT_IDX_MASK]--;
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
  if (resolveDir == TAKEN) {
    if (Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]] != S_T) {
      // update the value in the PHT, shift the predict towards strongly Taken
      Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]]++;
      }
      // update the histroy bits in the BHT
    Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] = ((Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] << 1) | 0x1) & TWO_LEVEL_BHT_BIT_MASK;
  } else {
    if (Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]] != S_NT) {
      // update the value in the PHT, shift the predict towards strongly Not Taken
      Two_level_PHT[PC & TWO_LEVEL_PHT_IDX_MASK][Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3]]--;
      }
      // update the histroy bits in the BHT
    Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] = (Two_level_BHT[(PC & TWO_LEVEL_BHT_IDX_MASK) >> 3] << 1) & (TWO_LEVEL_BHT_BIT_MASK - 0b1);
  }
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

#define HASH_MATRIX_ROW_1 0b00010111001010000001010011011000
#define HASH_MATRIX_ROW_2 0b00111011010001000010001010100100
#define HASH_MATRIX_ROW_3 0b01111101100000100100000101000010
#define HASH_MATRIX_ROW_4 0b00000001000000011000000010000001



//total 128Kib
static UINT32 bimodel_PHT[4096];
static UINT32 selector[4096];


#define TAGE_BHR_MASK 0xFFFFFFFF
#define TAGE_PHT1_MASK 0b00000000000011111111
#define TAGE_PHT2_MASK 0b00000000111111111111
#define TAGE_PHT3_MASK 0b11111111111111111111

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

UINT32 bimodel_index(UINT32 PC){
  UINT32 index = (PC >> 2) & 0x00000fff;
  return index;
}

void init_bimodel(){
  for(int i = 0 ; i < 4096; i++){
    bimodel_PHT[i] = 0;
  }

}

bool Getperdiction_bimodel(UINT32 PC){
  UINT32 index = bimodel_index (PC);
  return (bimodel_PHT[index] >> 2);
}

void UpdatePredictor_bimodel(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 index = bimodel_index (PC);
  if(resolveDir == TAKEN && bimodel_PHT[index] < 8){
    bimodel_PHT[index]++;
  }
  if(resolveDir == NOT_TAKEN && bimodel_PHT[index] > 0){
    bimodel_PHT[index]--;
  }
}

typedef struct TAG_DATA{
  UINT32 pred; //8 bit tag
  UINT32 ctr; // saturation bits
  UINT32 u; //useful counter
} TAG_DATA;

#define MAX_U_VALUE 4
#define TAGE_PHT_SIZE 1024
#define N_BIT_SAT 3


static UINT64 TAGE_BHR;
static TAG_DATA TAGE_PHT1[TAGE_PHT_SIZE];
static TAG_DATA TAGE_PHT2[TAGE_PHT_SIZE];
static TAG_DATA TAGE_PHT3[TAGE_PHT_SIZE];
static TAG_DATA TAGE_PHT4[TAGE_PHT_SIZE];


UINT32 folded_xor(UINT64 value, UINT32 num_bit, UINT32 targ_num_bit){
  if (num_bit < 64) value = value & ((1 << num_bit) - 1);
  UINT32 mask = (1 << targ_num_bit) - 1;
  UINT32 temp = value & mask;

  for (UINT32 i = 0; i < ((num_bit -1) / targ_num_bit); i++){
    value = value >> targ_num_bit;
    temp = temp ^ value;
  }

  return temp & mask;
}


UINT32 Hash1(UINT32 PC){
  UINT32 folded_HIS1 = folded_xor(TAGE_BHR, 8, 8);
  UINT32 folded_HIS2 = folded_xor(TAGE_BHR, 8, 7);
  UINT32 combined = PC ^ folded_HIS1 ^ (folded_HIS2 << 1);
  UINT32 hash = combined & 0xff;
  return hash;
}

UINT32 Hash2(UINT32 PC){
  UINT32 folded_HIS1 = folded_xor(TAGE_BHR, 16, 8);
  UINT32 folded_HIS2 = folded_xor(TAGE_BHR, 16, 7);
  UINT32 combined = PC ^ folded_HIS1 ^ (folded_HIS2 << 1);
  UINT32 hash = combined & 0xff;
  return hash;
}

UINT32 Hash3(UINT32 PC){
  UINT32 folded_HIS1 = folded_xor(TAGE_BHR, 32, 8);
  UINT32 folded_HIS2 = folded_xor(TAGE_BHR, 32, 7);
  UINT32 combined = PC ^ folded_HIS1 ^ (folded_HIS2 << 1);
  UINT32 hash = combined & 0xff;
  return hash;
}

UINT32 Hash4(UINT32 PC){
  UINT32 folded_HIS1 = folded_xor(TAGE_BHR, 64, 8);
  UINT32 folded_HIS2 = folded_xor(TAGE_BHR, 64, 7);
  UINT32 combined = PC ^ folded_HIS1 ^ (folded_HIS2 << 1);
  UINT32 hash = combined & 0xff;
  return hash;
}




UINT32 PHT_index1(UINT32 PC, UINT64 HIS){
  UINT32 folded_HIS = folded_xor(HIS, 10, 8);
  UINT32 folded_PC = folded_xor(PC, 10, 10);
  UINT32 idx = folded_PC ^ (folded_HIS << 0);
  return idx % TAGE_PHT_SIZE;
}

UINT32 PHT_index2(UINT32 PC, UINT64 HIS){
  UINT32 folded_HIS = folded_xor(HIS, 10, 8);
  UINT32 folded_PC = folded_xor(PC, 10, 10);
  UINT32 idx = folded_PC ^ (folded_HIS << 2);
  return idx % TAGE_PHT_SIZE;
}

UINT32 PHT_index3(UINT32 PC, UINT64 HIS){
  UINT32 folded_HIS = folded_xor(HIS, 15, 8);
  UINT32 folded_PC = folded_xor(PC, 15, 10);
  UINT32 idx = folded_PC ^ (folded_HIS << 2);
  return idx % TAGE_PHT_SIZE;
}

UINT32 PHT_index4(UINT32 PC, UINT64 HIS){
  UINT32 folded_HIS = folded_xor(HIS, 20, 8);
  UINT32 folded_PC = folded_xor(PC, 15, 10);
  UINT32 idx = folded_PC ^ (folded_HIS << 6);
  return idx % TAGE_PHT_SIZE;
}

/*
UINT32 PHT_index1(UINT32 PC, UINT32 HIS){
  UINT64 history = HIS;
  UINT32 history_10bit = history & 0x3ff;
  UINT32 PCXOR = (PC & 0x3ff) ^ ((PC >> 10) & 0x3ff);
  UINT32 val = PCXOR ^ history_10bit;
  return val % TAGE_PHT_SIZE;
}
UINT32 PHT_index2(UINT32 PC, UINT32 HIS){
  UINT64 history = HIS;
  UINT32 history_10bit_1 = history & 0x3ff;
  history = history >> 10;
  UINT32 history_10bit_2 = history_10bit_1 ^ (history & 0x3ff);
  UINT32 val = (PC >> 2) ^ history_10bit_2;
  return val % TAGE_PHT_SIZE;
}
UINT32 PHT_index3(UINT32 PC, UINT32 HIS){
  UINT64 history = HIS;
  UINT32 history_10bit_1 = history & 0x3ff;
  history = history >> 10;
  UINT32 history_10bit_2 = history_10bit_1 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_3 = history_10bit_2 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_4 = history_10bit_3 ^ (history & 0x3ff);
  UINT32 val = (PC >> 2) ^ history_10bit_4;
  return val % TAGE_PHT_SIZE;
}
UINT32 PHT_index4(UINT32 PC, UINT32 HIS){
  UINT64 history = HIS;
  UINT32 history_10bit_1 = history & 0x3ff;
  history = history >> 10;
  UINT32 history_10bit_2 = history_10bit_1 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_3 = history_10bit_2 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_4 = history_10bit_3 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_5 = history_10bit_4 ^ (history & 0x3ff);
  history = history >> 10;
  UINT32 history_10bit_6 = history_10bit_5 ^ (history & 0x3ff);
  UINT32 val = (PC >> 2) ^ history_10bit_6;
  return val % TAGE_PHT_SIZE;
}
*/
void Init_TAGE(){
  for(int i = 0; i < TAGE_PHT_SIZE; i++){
    TAGE_PHT1[i].ctr = (1 << (N_BIT_SAT-1));
    TAGE_PHT1[i].pred = 0;
    TAGE_PHT1[i].u = 0;
    TAGE_PHT2[i].ctr = (1 << (N_BIT_SAT-1));
    TAGE_PHT2[i].pred = 0;
    TAGE_PHT2[i].u = 0;
    TAGE_PHT3[i].ctr = (1 << (N_BIT_SAT-1));
    TAGE_PHT3[i].pred = 0;
    TAGE_PHT3[i].u = 0;
    TAGE_PHT4[i].ctr = (1 << (N_BIT_SAT-1));
    TAGE_PHT4[i].pred = 0;
    TAGE_PHT4[i].u = 0;
  }
  TAGE_BHR = 0;
  return;
}
bool GetPrediction_TAGE(UINT32 PC){  
  //get pred from 3 entry:
  UINT32 index1 = PHT_index1(PC, TAGE_BHR);
  UINT32 index2 = PHT_index2(PC, TAGE_BHR);
  UINT32 index3 = PHT_index3(PC, TAGE_BHR);
  UINT32 index4 = PHT_index4(PC, TAGE_BHR);
  UINT32 hash1 = Hash1(PC);
  UINT32 hash2 = Hash2(PC);
  UINT32 hash3 = Hash3(PC);
  UINT32 hash4 = Hash4(PC);
  UINT32 result1 = TAGE_PHT1[index1].ctr >> (N_BIT_SAT-1);
  UINT32 result2 = TAGE_PHT2[index2].ctr >> (N_BIT_SAT-1);
  UINT32 result3 = TAGE_PHT3[index3].ctr >> (N_BIT_SAT-1);
  UINT32 result4 = TAGE_PHT4[index4].ctr >> (N_BIT_SAT-1);
  //all tag geted
  bool result = Getperdiction_bimodel(PC);
  
    if (TAGE_PHT1[index1].pred == hash1){
      result = result1;
    }
  
  
    if (TAGE_PHT2[index2].pred == hash2){
      result = result2;
    }
  
  
    if (TAGE_PHT3[index3].pred == hash3){
      result = result3;
    }

    if (TAGE_PHT4[index4].pred == hash4){
      result = result4;
    }

  return result;
}

void UpdatePredictor_TAGE(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

  
  UINT32 index1 = PHT_index1(PC, TAGE_BHR);
  UINT32 index2 = PHT_index2(PC, TAGE_BHR);
  UINT32 index3 = PHT_index3(PC, TAGE_BHR);
  UINT32 index4 = PHT_index4(PC, TAGE_BHR);
  UINT32 hash1 = Hash1(PC);
  UINT32 hash2 = Hash2(PC);
  UINT32 hash3 = Hash3(PC);
  UINT32 hash4 = Hash4(PC);

  UINT32 result_bank = 0;
  if (TAGE_PHT1[index1].pred == hash1){
    result_bank = 1;
  }
  if (TAGE_PHT2[index2].pred == hash2){
    result_bank = 2;
  }
  if (TAGE_PHT3[index3].pred == hash3){
    result_bank = 3;
  }
  if (TAGE_PHT4[index4].pred == hash4){
    result_bank = 4;
  }
  //find result from which bank

  //update 2bit in each bank
  if(TAGE_PHT1[index1].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT1[index1].ctr --;
    TAGE_PHT1[index1].u = 0;
  }
  if(TAGE_PHT1[index1].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT1[index1].ctr++;
    TAGE_PHT1[index1].u = 0;
  }
  if(TAGE_PHT2[index2].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT2[index2].ctr--;
    TAGE_PHT2[index2].u = 0;
  }
  if(TAGE_PHT2[index2].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT2[index2].ctr++;
    TAGE_PHT2[index2].u = 0;
  } 
  if(TAGE_PHT3[index3].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT3[index3].ctr--;
    TAGE_PHT3[index3].u = 0;
  }
  if(TAGE_PHT3[index3].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT3[index3].ctr++;
    TAGE_PHT3[index3].u = 0;
  } 
  if(TAGE_PHT4[index4].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT4[index4].ctr--;
    TAGE_PHT4[index4].u = 0;
  }
  if(TAGE_PHT4[index4].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT4[index4].ctr++;
    TAGE_PHT4[index4].u = 0;
  } 

  //prediciton wrong
  if(resolveDir != predDir){
    if(result_bank < 4){
      if(result_bank == 0){
        if(!TAGE_PHT1[index1].u){
          TAGE_PHT1[index1].pred = hash1;
          TAGE_PHT1[index1].ctr = resolveDir << (N_BIT_SAT-1);
          TAGE_PHT1[index1].u = 0;
        }
      }
      if(result_bank == 1){
        if(!TAGE_PHT2[index2].u){
          TAGE_PHT2[index2].pred = hash2;
          TAGE_PHT2[index2].ctr = resolveDir << (N_BIT_SAT-1);
          TAGE_PHT2[index2].u = 0;
        }
      }
      if(result_bank == 2){
        if(!TAGE_PHT3[index3].u){
          TAGE_PHT3[index3].pred = hash3;
          TAGE_PHT3[index3].ctr = resolveDir << (N_BIT_SAT-1);
          TAGE_PHT3[index3].u = 0;
        }
      }
      if(result_bank == 3){
        if(!TAGE_PHT4[index4].u){
          TAGE_PHT4[index4].pred = hash4;
          TAGE_PHT4[index4].ctr = resolveDir << (N_BIT_SAT-1);
          TAGE_PHT4[index4].u = 0;
        }
      }
    }

  }
  
  //perdiction correct
  if(resolveDir == predDir){
    if(result_bank == 1){
      TAGE_PHT1[index1].u = 1;
    }
    if(result_bank == 2){
      TAGE_PHT2[index2].u = 1;
    }
    if(result_bank == 3){
      TAGE_PHT3[index3].u = 1;
    }
    if(result_bank == 4){
      TAGE_PHT4[index4].u = 1;
    }
    
  }
  if (Getperdiction_bimodel(PC) != resolveDir && Getperdiction_bimodel(PC) != predDir){
    UINT32 index = bimodel_index (PC);
    bimodel_PHT[index] = (resolveDir << 2);
  }
  TAGE_BHR = (TAGE_BHR << 1) | resolveDir;
  
}








void InitPredictor_openend() {
  init_bimodel();
  Init_TAGE();
}

bool GetPrediction_openend(UINT32 PC) {
  bool result_tage = GetPrediction_TAGE(PC);
  bool result_bimodel = Getperdiction_bimodel(PC);
  bool res = result_tage;
  
  return res;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UpdatePredictor_bimodel(PC, resolveDir, predDir, branchTarget);
  UpdatePredictor_TAGE(PC, resolveDir, predDir, branchTarget);
  bool result_tage = GetPrediction_TAGE(PC);
  bool result_bimodel = Getperdiction_bimodel(PC);

  if(result_bimodel == resolveDir && result_tage != resolveDir && selector[selector_index(PC)] != S_T){
    selector[selector_index(PC)] ++;
  }
  if(result_bimodel != resolveDir && result_tage == resolveDir && selector[selector_index(PC)] != S_NT){
    selector[selector_index(PC)] --;
  }

}


