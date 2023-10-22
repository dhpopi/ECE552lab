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

<<<<<<< HEAD
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
  UINT32 index = (PC >> 0) & 0x00000fff;
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


=======
// helper macro functions
#define MASK_OF_SIZE(size) ((1 << size) - 1)
#define WEAK_TAKEN(size) (1 << (size - 1))
#define WEAK_NOT_TAKEN(size) (WEAK_TAKEN(size) - 1)
#define LOW_CONFIDENCE(cnt, size) (cnt == WEAK_TAKEN(size) || cnt == WEAK_NOT_TAKEN(size))
#define MAPPING_RESULT(prediction, size) (((prediction >> (size - 1)) == TAKEN) ? TAKEN : NOT_TAKEN)
#define INC_SAT_CNT(cnt, size) ((cnt == MASK_OF_SIZE(size)) ? cnt : cnt + 1)  // count up saturating counter
#define DEC_SAT_CNT(cnt, size) ((cnt == 0) ? cnt : cnt - 1)                // count down saturating counter

// predictor hyperparameters
#define BHT_IDX_SIZE    1       // size of the history table index
#define BHT_SIZE        (1 << BHT_IDX_SIZE)           // size of the history table index
#define BHT_IDX_MASK    MASK_OF_SIZE(BHT_IDX_SIZE)    // size of the history table index
#define BHT_IDX_OFFSET  1       // offset applied when calculating index from PC

#define BASE_IDX_SIZE   9       // size of the basic prediction table index
#define BASE_PHT_SIZE   (1 << BASE_IDX_SIZE)          // number of entries in basic prediction table
#define BASE_IDX_MASK   MASK_OF_SIZE(BASE_IDX_SIZE)   // mask for obtaining basic prediction table index from PC
#define BASE_CNT_SIZE   4       // number of bits used in saturation counter in basic prediction table

#define NUM_TAGE_PHT    7       // number of TAGE prediction table used
#define TAGE_IDX_SIZE   10      // size of the TAGE prediction table index
#define TAGE_PHT_SIZE   (1 << TAGE_IDX_SIZE)          // number of entries in TAGE prediction table
#define TAGE_IDX_MASK   MASK_OF_SIZE(TAGE_IDX_SIZE)   // mask for obtaining TAGE prediction table index from PC
#define TAGE_CNT_SIZE   3       // number of bits used in saturation counters in TAGE prediction table
#define TAGE_TAG_SIZE   11       // number of bits used in tags in TAGE prediction table
#define TAGE_U_SIZE     2       // number of bits used in usefulness counter in TAGE prediction table
#define CYC_RST_U       ((BASE_PHT_SIZE + TAGE_PHT_SIZE * NUM_TAGE_PHT)<<4)   // number of prediction made before all usefulness counter being "weakened"

#define BIAS_SIZE       6       // the size of bias counter that determine whether longer history with less confidence is better than short history


typedef struct base_entry{
  UINT32 cnt;     // saturation counter
}base_entry;

typedef struct tage_entry{
  UINT32 cnt;     // saturation counter
  UINT32 tag;     // tag
  UINT32 useful;  // age counter
}tage_entry;

typedef __uint128_t UINT128;

UINT128 BHT[BHT_SIZE];  // Globle history register
base_entry BASE_PHT[BASE_PHT_SIZE];  // basic prediction table
tage_entry TAGE_PHT[NUM_TAGE_PHT][TAGE_PHT_SIZE];  // TAGE prediction tables

// give an alternative if the bias counter indicates that prediction made by longest history with less confidence is less favourable
UINT32 use_alt;

UINT32 lh_pred;       // the prediction made by tag-matching TAGE_PHT that uses the longest history
int lh_PHT_num;       // the table number tag-matching TAGE_PHT that uses the longest history
UINT32 lh_PHT_idx;    // the index of the entry in the tag-matching TAGE_PHT that uses the longest history

UINT32 alt_pred;      // the prediction made by tag-matching TAGE_PHT that uses the second longest history
int alt_PHT_num;      // the table number tag-matching TAGE_PHT that uses the second longest history
UINT32 alt_PHT_idx;   // the index of the entry in the tag-matching TAGE_PHT that uses the second longest history

UINT128 num_prediction_made;   // counter for keeping track of when to soft reset all usefulness counter

// folding xor based on Michaud PPM-like paper
UINT32 folded_xor(UINT128 value, UINT32 size, UINT32 targ_size){
  UINT32 mask = MASK_OF_SIZE(targ_size);
  UINT32 temp = value & mask;

  if (size < 128) value = value & MASK_OF_SIZE(size);

  for (UINT32 i = 0; i < ((size - 1) / targ_size); i++){
    value = value >> targ_size;
    temp = temp ^ value;
  }

  return temp & mask;
}

// calculating tag based on Michaud PPM-like paper
UINT32 get_tag(UINT32 PC, UINT128 history, UINT32 PHT_number){
  UINT32 folded_PC = folded_xor(PC, 32, TAGE_TAG_SIZE);
  UINT32 folded_HIS1 = folded_xor(history, (2<<PHT_number), TAGE_TAG_SIZE);
  UINT32 folded_HIS2 = folded_xor(history, (2<<PHT_number), TAGE_TAG_SIZE-1);
  UINT32 combined = folded_PC ^ folded_HIS1 ^ (folded_HIS2 << 1);
  UINT32 hash = combined & MASK_OF_SIZE(TAGE_TAG_SIZE);
  return hash;
}

// calculating index based on Michaud PPM-like paper
UINT32 get_idx(UINT32 PC, UINT128 history, UINT32 PHT_number){
  UINT32 folded_PC1 = folded_xor(PC, 32, TAGE_IDX_SIZE);
  UINT32 folded_PC2 = folded_xor(PC, 32, TAGE_IDX_SIZE-1);
  UINT32 folded_HIS = folded_xor(history, (2<<PHT_number), TAGE_IDX_SIZE);
  UINT32 combined = folded_PC1 ^ (folded_PC2 << 1) ^ folded_HIS;
  UINT32 hash = combined & TAGE_IDX_MASK;
  return hash;
}

>>>>>>> TAGE-60bit

void InitPredictor_openend() {
  // initialize history register
  for(int i = 0; i < BHT_SIZE; i++){
    BHT[i] = NOT_TAKEN;
  }

<<<<<<< HEAD
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

static UINT32 u_counter = 0;
void UpdatePredictor_TAGE(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){
  u_counter++;
  
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
=======
  // initialize basic prediction table
  for(int i = 0; i < BASE_PHT_SIZE; i++){
    //set base prediction to weak_NT
    BASE_PHT[i].cnt = WEAK_TAKEN(BASE_CNT_SIZE);
  }

  // initialize TAGE prediction table
  for(int i = 0; i < NUM_TAGE_PHT; i++){
    for (int j = 0; j < TAGE_PHT_SIZE; j++){
      //set TAGE prediction to weak_T
      TAGE_PHT[i][j].cnt = WEAK_TAKEN(TAGE_CNT_SIZE);
      TAGE_PHT[i][j].tag = 0;
      TAGE_PHT[i][j].useful = 0;
    }
  }
  
  // initialize the bias counter to slightly favour the prediction made with longer history bits even it is newly allocated
  use_alt = WEAK_NOT_TAKEN(BIAS_SIZE);
  num_prediction_made = 0;
  
}

bool GetPrediction_openend(UINT32 PC) {
  //get predition and alternate prediction
  UINT32 predDir = TAKEN;
  bool lh_pred_made = false;
  bool alt_pred_made = false;
  UINT32 TAGE_PHT_idx;
  UINT32 TAGE_PHT_tag;

  for (int i = NUM_TAGE_PHT-1; i >= 0; i--){
    TAGE_PHT_idx = get_idx(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
    TAGE_PHT_tag = get_tag(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
    if (TAGE_PHT[i][TAGE_PHT_idx].tag == TAGE_PHT_tag){   // tag match
      if (!lh_pred_made){   // first time finding a match -> longest histroy
        lh_pred = MAPPING_RESULT(TAGE_PHT[i][TAGE_PHT_idx].cnt, TAGE_CNT_SIZE);
        lh_PHT_num = i;
        lh_PHT_idx = TAGE_PHT_idx;
        lh_pred_made = true;
      } else {  // second time finding a match
        alt_pred = MAPPING_RESULT(TAGE_PHT[i][TAGE_PHT_idx].cnt, TAGE_CNT_SIZE);     
        alt_PHT_num = i;
        alt_PHT_idx = TAGE_PHT_idx;
        alt_pred_made = true;
        break;
      }
    }
  }

  if (lh_pred_made){
    if (!alt_pred_made){ // no alternative found
      alt_pred = MAPPING_RESULT(BASE_PHT[PC & BASE_IDX_MASK].cnt, BASE_CNT_SIZE);
      alt_PHT_num = -1;
      alt_PHT_idx = PC & BASE_IDX_MASK;
    }

    // deciding if current bias favour longer history with low confidence
    if(MAPPING_RESULT(use_alt, BIAS_SIZE) && TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful == 0 && LOW_CONFIDENCE(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt,TAGE_CNT_SIZE)) {
      predDir = alt_pred;
    } else {
      predDir = lh_pred;
    }
  } else {  // no prediction made in TAGE PHT
    lh_pred = MAPPING_RESULT(BASE_PHT[PC & BASE_IDX_MASK].cnt, BASE_CNT_SIZE);
    lh_PHT_num = -1;
    lh_PHT_idx = PC & BASE_IDX_MASK;
    alt_pred = lh_pred;
    alt_PHT_num = -1;
    alt_PHT_idx = PC & BASE_IDX_MASK;
    predDir = lh_pred;
  }
  return predDir;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // increment counter
  num_prediction_made++;

  // update basic prediction table
  if (resolveDir == TAKEN){
    BASE_PHT[PC & BASE_IDX_MASK].cnt = INC_SAT_CNT(BASE_PHT[PC & BASE_IDX_MASK].cnt,BASE_CNT_SIZE);
  } else {
    BASE_PHT[PC & BASE_IDX_MASK].cnt = DEC_SAT_CNT(BASE_PHT[PC & BASE_IDX_MASK].cnt,BASE_CNT_SIZE);
  } 

  // update the bias counter
  if (lh_PHT_num != -1 && lh_pred != alt_pred && TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful == 0 && LOW_CONFIDENCE(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt,TAGE_CNT_SIZE)){
    if (alt_pred == resolveDir) use_alt = INC_SAT_CNT(use_alt,BIAS_SIZE);
    else                        use_alt = DEC_SAT_CNT(use_alt,BIAS_SIZE);
>>>>>>> TAGE-60bit
  }

<<<<<<< HEAD
  //update 2bit in each bank
  if(TAGE_PHT1[index1].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT1[index1].ctr --;
  }
  if(TAGE_PHT1[index1].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT1[index1].ctr++;
  }
  if(TAGE_PHT2[index2].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT2[index2].ctr--;
  }
  if(TAGE_PHT2[index2].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT2[index2].ctr++;
  } 
  if(TAGE_PHT3[index3].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT3[index3].ctr--;
  }
  if(TAGE_PHT3[index3].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT3[index3].ctr++;
  } 
  if(TAGE_PHT4[index4].ctr > 0 && resolveDir == NOT_TAKEN){
    TAGE_PHT4[index4].ctr--;
  }
  if(TAGE_PHT4[index4].ctr < ((1<<N_BIT_SAT)-1) && resolveDir == TAKEN){
    TAGE_PHT4[index4].ctr++;
  } 

  if (u_counter % 200 == 0) {
    TAGE_PHT1[index1].u = 0;
    TAGE_PHT2[index2].u = 0;
    TAGE_PHT3[index3].u = 0;
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
=======
  // update the usefulness counter if the prediction is made using TAGE PHT
  if (lh_PHT_num != -1 && lh_pred != alt_pred){
    if (lh_pred == resolveDir) TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful = INC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful, TAGE_U_SIZE);
    else                       TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful = DEC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].useful, TAGE_U_SIZE);
  }

  // check if it is time to "weaken" the usefulness of all entries
  if (num_prediction_made == CYC_RST_U){
    for(int i = 0; i < NUM_TAGE_PHT; i++){
      for (int j = 0; j < TAGE_PHT_SIZE; j++){
        // "weaken" usefulness counter
        TAGE_PHT[i][j].useful &= MASK_OF_SIZE(TAGE_U_SIZE) - 1;
>>>>>>> TAGE-60bit
      }
    }
    num_prediction_made = 0;
  }

  // update TAGE PHT
  if (lh_PHT_num != -1){
    if (resolveDir == TAKEN) TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt = INC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt, TAGE_CNT_SIZE);
    else                     TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt = DEC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt, TAGE_CNT_SIZE);
  }
<<<<<<< HEAD
  
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
=======

  // if prediction is wrong then allocate a new entry in TAGE PHT that looks at longer history
  if (lh_pred != resolveDir){
    UINT32 idx;
    bool space_found = false;
    for(int i = lh_PHT_num + 1 + use_alt%NUM_TAGE_PHT; i < NUM_TAGE_PHT; i++){
      idx = get_idx(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
      if (TAGE_PHT[i][idx].useful == 0){
        TAGE_PHT[i][idx].cnt = WEAK_TAKEN(TAGE_CNT_SIZE);
        TAGE_PHT[i][idx].tag = get_tag(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
        space_found = true;
        break;
      }
>>>>>>> TAGE-60bit
    }
    
    if (!space_found){  // no space available then decrease all the usefulness counter
      for(int i = lh_PHT_num + 1; i < NUM_TAGE_PHT; i++){
        idx = get_idx(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
        TAGE_PHT[i][idx].useful = DEC_SAT_CNT(TAGE_PHT[i][idx].useful, TAGE_U_SIZE);
      }
    }
  }

  // update history register
  BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK] = (BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK] << 1) | resolveDir;
}


