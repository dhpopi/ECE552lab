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
#define BASE_CNT_SIZE   8       // number of bits used in saturation counter in basic prediction table

#define NUM_TAGE_PHT    6       // number of TAGE prediction table used
#define TAGE_IDX_SIZE   10      // size of the TAGE prediction table index
#define TAGE_PHT_SIZE   (1 << TAGE_IDX_SIZE)          // number of entries in TAGE prediction table
#define TAGE_IDX_MASK   MASK_OF_SIZE(TAGE_IDX_SIZE)   // mask for obtaining TAGE prediction table index from PC
#define TAGE_CNT_SIZE   3       // number of bits used in saturation counters in TAGE prediction table
#define TAGE_TAG_SIZE   11      // number of bits used in tags in TAGE prediction table
#define TAGE_U_SIZE     2       // number of bits used in usefulness counter in TAGE prediction table
#define CYC_RST_U       ((BASE_PHT_SIZE + TAGE_PHT_SIZE * NUM_TAGE_PHT)<<4)   // number of prediction made before all usefulness counter being "weakened"

#define LOOP_IDX_SIZE   4       // size of the Loop prediction table index
#define LOOP_PHT_SIZE   (1 << LOOP_IDX_SIZE)          // size of the Loop prediction table index
#define LOOP_IDX_MASK   MASK_OF_SIZE(LOOP_IDX_SIZE)   // size of the Loop prediction table index
#define LOOP_IDX_OFFSET 4       // offset applied when calculating index from PC
#define LOOP_TAG_SIZE   32      // size of the Loop prediction tag
#define LOOP_SI_SIZE    10      // size of the Loop prediction speculative iteration count
#define LOOP_NSI_SIZE   10      // size of the Loop prediction non-speculative iteration count
#define LOOP_TRIP_SIZE  10      // size of the Loop prediction trip counter
#define LOOP_CONF_SIZE  1       // size of the Loop prediction confidence bit

#define BIAS_SIZE       6       // the size of bias counter that determine whether longer history with less confidence is better than short history


typedef struct base_entry{
  UINT32 cnt;     // saturation counter
}base_entry;

typedef struct tage_entry{
  UINT32 cnt;     // saturation counter
  UINT32 tag;     // tag
  UINT32 useful;  // age counter
}tage_entry;

typedef struct loop_entry{
  UINT32 tag;             // tag
  UINT32 spec_iter;       // speculative iteration count
  UINT32 non_spec_iter;   // non-speculative iteration count
  UINT32 trip;            // loop trip count is the number of times in a row the loop branch is taken
  UINT32 conf;            // confidence bit indicating that the same loop trip count has been seen at least twice in a row
}loop_entry;

typedef __uint128_t UINT128;

UINT128 BHT[BHT_SIZE];  // Globle history register
base_entry BASE_PHT[BASE_PHT_SIZE];  // basic prediction table
tage_entry TAGE_PHT[NUM_TAGE_PHT][TAGE_PHT_SIZE];  // TAGE prediction tables
loop_entry LOOP_PHT[LOOP_PHT_SIZE]; // loop prediction table

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


void InitPredictor_openend() {
  // initialize history register
  for(int i = 0; i < BHT_SIZE; i++){
    BHT[i] = NOT_TAKEN;
  }

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

  // initialize loop prediction table
  for(int i = 0; i < LOOP_PHT_SIZE; i++){
    LOOP_PHT[i].tag = 0;
    LOOP_PHT[i].spec_iter = 0;
    LOOP_PHT[i].non_spec_iter = 0;
    LOOP_PHT[i].trip = 0;
    LOOP_PHT[i].conf = 0;
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

  if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].tag == PC){
    if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter == LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip){
      if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].conf) predDir = NOT_TAKEN;
    } else {
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter = INC_SAT_CNT(LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter, LOOP_SI_SIZE);
    }
  }
  
  return predDir;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // increment counter
  num_prediction_made++;
  bool loop_back = PC > branchTarget;

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
  }

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
      }
    }
    num_prediction_made = 0;
  }

  // update TAGE PHT
  if (lh_PHT_num != -1){
    if (resolveDir == TAKEN) TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt = INC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt, TAGE_CNT_SIZE);
    else                     TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt = DEC_SAT_CNT(TAGE_PHT[lh_PHT_num][lh_PHT_idx].cnt, TAGE_CNT_SIZE);
  }

  // if prediction is wrong then allocate a new entry in TAGE PHT that looks at longer history
  if (lh_pred != resolveDir){
    UINT32 idx;
    bool space_found = false;
    for(int i = lh_PHT_num + 1; i < NUM_TAGE_PHT; i++){
      idx = get_idx(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
      if (TAGE_PHT[i][idx].useful == 0){
        TAGE_PHT[i][idx].cnt = WEAK_TAKEN(TAGE_CNT_SIZE);
        TAGE_PHT[i][idx].tag = get_tag(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
        space_found = true;
        break;
      }
    }
    
    if (!space_found){  // no space available then decrease all the usefulness counter
      for(int i = lh_PHT_num + 1; i < NUM_TAGE_PHT; i++){
        idx = get_idx(PC, BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK], i);
        TAGE_PHT[i][idx].useful = DEC_SAT_CNT(TAGE_PHT[i][idx].useful, TAGE_U_SIZE);
      }
    }
  }

  // update loop prediction table
  if (loop_back && resolveDir == TAKEN){
    if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].tag != PC && predDir != resolveDir){
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].tag = PC;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter = 0;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter = 0;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip = 0;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].conf = 0;
    } else if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].tag == PC) {
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter = INC_SAT_CNT(LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter, LOOP_NSI_SIZE);
    }
  } else if (loop_back && resolveDir == NOT_TAKEN){
    if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].tag == PC){
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip = INC_SAT_CNT(LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip, LOOP_TRIP_SIZE);
      if (LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter == LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip){
        LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].conf = 1;
      } else {
        LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].conf = 0;
      }
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter = INC_SAT_CNT(LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter, LOOP_NSI_SIZE);
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].trip = LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter = LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].spec_iter - LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter;
      LOOP_PHT[(PC >> LOOP_IDX_OFFSET) & LOOP_IDX_MASK].non_spec_iter = 0;
    }
  }

  // update history register
  BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK] = (BHT[(PC>>BHT_IDX_OFFSET) & BHT_IDX_MASK] << 1) | resolveDir;
}


