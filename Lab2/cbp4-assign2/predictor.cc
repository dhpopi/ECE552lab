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
  UINT32 MIS;
} TAG_DATA;

//total 128Kib
static UINT32 bimodel_PHT[4096];
static UINT32 selector[4096];
static UINT32 supersel = 0;

#define TAGE_BHR_MASK 0xFFFFFFFF
#define TAGE_PHT1_MASK 0b00000000000011111111
#define TAGE_PHT2_MASK 0b00000000111111111111
#define TAGE_PHT3_MASK 0b11111111111111111111

#define MAX_U_VALUE 4
#define TAGE_PHT_SIZE 256


static UINT32 TAGE_BHR;
static TAG_DATA TAGE_PHT1[TAGE_PHT_SIZE];
static TAG_DATA TAGE_PHT2[TAGE_PHT_SIZE];
static TAG_DATA TAGE_PHT3[TAGE_PHT_SIZE];

UINT32 bimodel_index(UINT32 PC){
  UINT32 index = PC & 0x00000fff;
  return index;
}

void init_bimodel(){
  for(int i = 0 ; i < 4096; i++){
    bimodel_PHT[i] = 0;
  }

}

bool Getperdiction_bimodel(UINT32 PC){
  UINT32 index = bimodel_index (PC);
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



void init_sel(){
  for(int i = 0; i < 4096; i++){
    selector[i] = W_NT;
  }
}

UINT32 selector_index(UINT32 PC){
  UINT32 index = (PC >> 2) & 0x00000fff;
  return index;
}

bool Get_sel(UINT32 PC){
  UINT32 index = selector_index (PC);
  switch(selector[index]){
    
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

UINT32 hash_func1(UINT32 index, UINT32 PC){
  UINT32 val = (PC >> 2) ^ index;
  UINT32 y1 = val & 0x00000001;
  UINT32 yn = val & 0x00002000;
  UINT32 changed = (val >> 1) |  yn;
  return (y1 ^ changed) % TAGE_PHT_SIZE;
}

UINT32 hash_func2(UINT32 index, UINT32 PC){
  UINT32 val = (PC>>2) ^ index;
  UINT32 yn = val & 0x00002000;
  UINT32 yn_1 = val & 0x00001000;
  UINT32 changed = ((val << 1) & 0x00003fff) | (yn_1 >> 12);
  return (changed ^ yn)% TAGE_PHT_SIZE;
}

UINT32 hash_func3(UINT32 index, UINT32 PC){
  UINT32 val = (PC>>2) ^ index;
  UINT32 yn = val & 0x00002000;
  UINT32 yn_1 = val & 0x00001000;
  UINT32 changed = ((val << 1) & 0x00003fff) | (yn_1 >> 12);
  UINT32 temp = (changed ^ yn);

  UINT32 y1_1 = val & 0x00000001;
  UINT32 yn_1_1 = val & 0x00002000;
  UINT32 changed_1 = (val >> 1) |  yn_1_1;
  UINT32 temp2 = (y1_1 ^ changed_1);


  return (temp^temp2) % TAGE_PHT_SIZE;
}




void init_TAGE(){
  TAGE_BHR = 0;
  for (int i = 0; i < TAGE_PHT_SIZE; i++){
    
    TAGE_PHT1[i].TAG = 0;
    TAGE_PHT1[i].DATA = W_NT;
    TAGE_PHT1[i].MIS = 0;

    TAGE_PHT2[i].TAG = 0;
    TAGE_PHT2[i].DATA = W_NT;
    TAGE_PHT2[i].MIS = 0;
    
    TAGE_PHT3[i].TAG = 0;
    TAGE_PHT3[i].DATA = W_NT;
    TAGE_PHT3[i].MIS = 0;
   
  }
}
bool get_result(UINT32 val){
  switch(val){
    
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

bool Getprediction_TAGE(UINT32 PC){
  UINT32 hashed_tag1 = hash_func1(TAGE_BHR , PC);
  UINT32 hashed_tag2 = hash_func2(TAGE_BHR , PC);
  UINT32 hashed_tag3 = hash_func3(TAGE_BHR , PC);
  UINT32 result = Getperdiction_bimodel(PC);
  bool result1 = 0;
  bool result2 = 0;
  bool result3 = 0;

    result1 = get_result(TAGE_PHT1[hashed_tag1].DATA);
    result2 = get_result(TAGE_PHT2[hashed_tag1].DATA);
    result3 = get_result(TAGE_PHT3[hashed_tag3].DATA);

  if ((result1 == TAKEN && result2 == TAKEN) || (result1 == TAKEN && result3 == TAKEN) || (result2 == TAKEN && result3 == TAKEN)) 
        result = TAKEN;
    else
        result = NOT_TAKEN;

    return result;

  

}

void UpdatePredictor_TAGE(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 hashed_tag1 = hash_func1(TAGE_BHR , PC);
  UINT32 hashed_tag2 = hash_func2(TAGE_BHR , PC);
  UINT32 hashed_tag3 = hash_func3(TAGE_BHR , PC);
  TAGE_BHR = (TAGE_BHR << 1) | resolveDir;

  //TAGE_PHT1
  if (resolveDir == TAKEN && TAGE_PHT1[hashed_tag1].DATA != S_T){
    TAGE_PHT1[hashed_tag1].DATA++;
  }
  if(resolveDir == NOT_TAKEN && TAGE_PHT1[hashed_tag1].DATA != S_NT){
    TAGE_PHT1[hashed_tag1].DATA++;
  }
  //devide 2 to convert 2bits to T/N
  if(resolveDir != (TAGE_PHT1[hashed_tag1].DATA /2) && TAGE_PHT1[hashed_tag1].MIS < MAX_U_VALUE ){
    TAGE_PHT1[hashed_tag1].MIS ++;
  }
  if(resolveDir == (TAGE_PHT1[hashed_tag1].DATA /2) && TAGE_PHT1[hashed_tag1].MIS > 0 ){

    TAGE_PHT1[hashed_tag1].MIS --;
  }

  //TAGE_PHT2
  if (resolveDir == TAKEN && TAGE_PHT2[hashed_tag2].DATA != S_T){
    TAGE_PHT2[hashed_tag2].DATA++;
  }
  if(resolveDir == NOT_TAKEN && TAGE_PHT2[hashed_tag2].DATA != S_NT){
    TAGE_PHT2[hashed_tag2].DATA++;
  }
  //devide 2 to convert 2bits to T/N
  if(resolveDir != (TAGE_PHT2[hashed_tag2].DATA /2) && TAGE_PHT2[hashed_tag2].MIS < MAX_U_VALUE ){
    TAGE_PHT2[hashed_tag2].MIS ++;
  }
  if(resolveDir == (TAGE_PHT2[hashed_tag2].DATA /2) && TAGE_PHT2[hashed_tag2].MIS > 0 ){
    TAGE_PHT2[hashed_tag2].MIS --;
  }

  //TAGE_PHT3
  if (resolveDir == TAKEN && TAGE_PHT3[hashed_tag3].DATA != S_T){
    TAGE_PHT3[hashed_tag3].DATA++;
  }
  if(resolveDir == NOT_TAKEN && TAGE_PHT1[hashed_tag3].DATA != S_NT){
    TAGE_PHT1[hashed_tag3].DATA++;
  }
  //devide 2 to convert 2bits to T/N
  if(resolveDir != (TAGE_PHT3[hashed_tag3].DATA /2) && TAGE_PHT3[hashed_tag3].MIS < MAX_U_VALUE ){
    TAGE_PHT3[hashed_tag3].MIS ++;
  }
  if(resolveDir == (TAGE_PHT3[hashed_tag3].DATA /2) && TAGE_PHT3[hashed_tag3].MIS > 0 ){
    TAGE_PHT3[hashed_tag3].MIS --;
  }
}





void InitPredictor_openend() {
  init_bimodel();
  init_TAGE();
}

bool GetPrediction_openend(UINT32 PC) {
  bool result_tage = Getprediction_TAGE(PC);
  bool result_bimodel = Getperdiction_bimodel(PC);
  bool res = Getprediction_TAGE(PC);
  return res;
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


