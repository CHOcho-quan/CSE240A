//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <string.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Quan Luo";
const char *studentID   = "A59012030";
const char *email       = "quluo@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// Global Predictor Parameters
//
uint8_t* bhtGlobal;
uint64_t gHistory;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  switch (bpType) {
    case GSHARE: {
      // Initialization for GShare
      int num = 1 << ghistoryBits;
      gHistory = 0;
      bhtGlobal = (uint8_t*) malloc(num * sizeof(uint8_t));
      memset(bhtGlobal, WN, num * sizeof(uint8_t));
      break;
    }
    case TOURNAMENT:
    case CUSTOM:
    case STATIC:
    default:
      break;
  }
  
}

// Gshare Prediction & training
uint8_t
make_prediction_gshare(uint32_t pc) {
  int num = 1 << ghistoryBits;
  int index = (pc & (num - 1)) ^ (gHistory & (num - 1));
  switch (bhtGlobal[index]) {
    case ST:
    case WT:
      return TAKEN;
    case WN:
    case SN:
      return NOTTAKEN;
    default:
      printf("Unknown state %d in Gshare history table!\n", bhtGlobal[index]);
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  int num = 1 << ghistoryBits;
  int index = (pc & (num - 1)) ^ (gHistory & (num - 1));
  switch (bhtGlobal[index]) {
    case ST:
      bhtGlobal[index] = (outcome == TAKEN) ? ST : WT;
      break;
    case WT:
      bhtGlobal[index] = (outcome == TAKEN) ? ST : WN;
      break;
    case WN:
      bhtGlobal[index] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      bhtGlobal[index] = (outcome == TAKEN) ? WN : SN;
      break;
    default:
      printf("Unknown state %d in Gshare history table\n", bhtGlobal[index]);
  }

  // Update History
  gHistory = ((gHistory << 1) | outcome);
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return make_prediction_gshare(pc);
    case TOURNAMENT:
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  switch (bpType) {
    case GSHARE:
      train_gshare(pc, outcome);
    case TOURNAMENT:
    case CUSTOM:
    case STATIC:
    default:
      break;
  }
}
