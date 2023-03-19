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
// Local Predictor Parameters
//
uint8_t* bhtLocal;
uint64_t* lHistory;

//
// Global Predictor Parameters
//
uint8_t* bhtGlobal;
uint64_t gHistory;
uint8_t* bhtChoose;

//
// Local Perceptron Parameters
//
int** pWeights;
int* pHistory;
uint8_t percepRes;

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
    case TOURNAMENT: {
      // Initialize Local Predictor
      int num = 1 << lhistoryBits, pcNum = 1 << pcIndexBits;
      lHistory = (uint64_t*) malloc(pcNum * sizeof(uint64_t));
      bhtLocal = (uint8_t*) malloc(num * sizeof(uint8_t));
      memset(lHistory, 0, pcNum * sizeof(uint64_t));
      memset(bhtLocal, WN, num * sizeof(uint8_t));
      // Initialize Global Predictor
      num = 1 << ghistoryBits;
      gHistory = 0;
      bhtGlobal = (uint8_t*) malloc(num * sizeof(uint8_t));
      memset(bhtGlobal, WN, num * sizeof(uint8_t));
      // Initialize Choose Predictor
      bhtChoose = (uint8_t*) malloc(num * sizeof(uint8_t));
      memset(bhtChoose, WGLOBAL, num * sizeof(uint8_t));
      break;
    }
    case CUSTOM: {
      // Initialize Perceptron
      int lNum = 1 << lhistoryBits, gNum = 1 << ghistoryBits, pcNum = 1 << pcIndexBits;
      percepRes = 0;
      pHistory = (int*) malloc(gNum * sizeof(int));
      pWeights = (int**) malloc(pcNum * sizeof(int*));
      memset(pHistory, -1, gNum * sizeof(int));
      for (int i = 0; i < gNum; ++i) {
        pWeights[i] = (int*) malloc(ghistoryBits * sizeof(int));
        memset(pWeights[i], 0, ghistoryBits * sizeof(int));
      }
      // Initialize Local Predictor
      lHistory = (uint64_t*) malloc(pcNum * sizeof(uint64_t));
      bhtLocal = (uint8_t*) malloc(lNum * sizeof(uint8_t));
      memset(lHistory, 0, pcNum * sizeof(uint64_t));
      memset(bhtLocal, WN, lNum * sizeof(uint8_t));
      // Initialize Global Predictor
      gHistory = 0;
      bhtGlobal = (uint8_t*) malloc(gNum * sizeof(uint8_t));
      memset(bhtGlobal, WN, gNum * sizeof(uint8_t));
    }
    case STATIC:
    default:
      break;
  }
  
}

// Custom Prediction & Training
uint8_t
make_prediction_custom(uint32_t pc) {
  int globalNum = 1 << ghistoryBits;
  int pcNum = 1 << pcIndexBits, lNum = 1 << lhistoryBits;
  int localIndex = (lHistory[(pc & (pcNum - 1))] & (lNum - 1));
  int gshareIndex = ((pc & (globalNum - 1)) ^ (gHistory & (globalNum - 1)));
  // Perceptron
  int pForward = 1; // bias term
  for (int i = 0; i < ghistoryBits; ++i) pForward += pWeights[localIndex][i] * pHistory[i];
  percepRes = pForward < 0 ? NOTTAKEN : TAKEN;
  // Global GShare
  uint8_t globalRes;
  switch (bhtGlobal[gshareIndex]) {
    case ST:
    case WT:
      globalRes = TAKEN;
      break;
    case WN:
    case SN:
      globalRes = NOTTAKEN;
      break;
    default:
      printf("Unknown state %d in Global history table!\n", bhtGlobal[gshareIndex]);
      globalRes = NOTTAKEN;
  }
  // Local 2-bit Counter
  uint8_t localRes;
  switch (bhtLocal[localIndex]) {
    case ST:
    case WT:
      localRes = TAKEN;
      break;
    case SN:
    case WN:
      localRes = NOTTAKEN;
      break;
    default:
      printf("Unknown state %d in Local history table!\n", bhtGlobal[localIndex]);
      localRes = NOTTAKEN;
  }
  // Majority Vote
  if (localRes == globalRes) return localRes;
  if (localRes == percepRes) return localRes;
  return globalRes;
}

void
train_custom(uint32_t pc, uint8_t outcome) {
  int pcNum = 1 << pcIndexBits, lNum = 1 << lhistoryBits;
  int localIndex = (lHistory[(pc & (pcNum - 1))] & (lNum - 1));
  int globalNum = 1 << ghistoryBits;
  int globalIndex = ((pc & (globalNum - 1)) ^ (gHistory & (globalNum - 1)));
  // Perceptron
  if (percepRes != outcome) {
    for (int i = 0; i < ghistoryBits; ++i) {
      pWeights[localIndex][i] += pHistory[i] * (outcome == TAKEN ? 1 : -1);
      pWeights[localIndex][i] = MIN(pWeights[localIndex][i], MIN_WEIGHT);
      pWeights[localIndex][i] = MAX(pWeights[localIndex][i], MAX_WEIGHT);
    }
    for (int i = ghistoryBits - 1; i > 0; --i) pHistory[i] = pHistory[i - 1];
    pHistory[0] = (outcome == TAKEN ? 1 : -1);
  }
  // Global 2-bit Counter
  switch (bhtGlobal[globalIndex]) {
    case ST:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? ST : WT;
      break;
    case WT:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? ST : WN;
      break;
    case WN:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? WN : SN;
      break;
    default:
      printf("Unknown state %d in Global history table!\n", bhtGlobal[globalIndex]);
  }

  // Local 2-bit Counter
  switch (bhtLocal[localIndex]) {
    case ST:
      bhtLocal[localIndex] = (outcome == TAKEN) ? ST : WT;
      break;
    case WT:
      bhtLocal[localIndex] = (outcome == TAKEN) ? ST : WN;
      break;
    case WN:
      bhtLocal[localIndex] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      bhtLocal[localIndex] = (outcome == TAKEN) ? WN : SN;
      break;
    default:
      printf("Unknown state %d in Local history table!\n", bhtLocal[localIndex]);
  }

  // Update History
  gHistory = ((gHistory << 1) | outcome);
  lHistory[(pc & (pcNum - 1))] = ((lHistory[(pc & (pcNum - 1))] << 1) | outcome);
}

// Tournament Prediction & Training
uint8_t
make_prediction_tournament(uint32_t pc) {
  // Global 2-bit Counter
  int globalNum = 1 << ghistoryBits;
  int globalIndex = (gHistory & (globalNum - 1));
  uint8_t globalRes;
  switch (bhtGlobal[globalIndex]) {
    case ST:
    case WT:
      globalRes = TAKEN;
      break;
    case WN:
    case SN:
      globalRes = NOTTAKEN;
      break;
    default:
      printf("Unknown state %d in Global history table!\n", bhtGlobal[globalIndex]);
      globalRes = NOTTAKEN;
  }

  // Local 2-bit Counter
  int pcNum = 1 << pcIndexBits, lNum = 1 << lhistoryBits;
  int localIndex = (lHistory[(pc & (pcNum - 1))] & (lNum - 1));
  uint8_t localRes;
  switch (bhtLocal[localIndex]) {
    case ST:
    case WT:
      localRes = TAKEN;
      break;
    case SN:
    case WN:
      localRes = NOTTAKEN;
      break;
    default:
      printf("Unknown state %d in Local history table!\n", bhtGlobal[localIndex]);
      localRes = NOTTAKEN;
  }

  // Choice Predictor
  uint8_t choice = bhtChoose[globalIndex];
  switch (choice) {
    case WGLOBAL:
    case SGLOBAL:
      return globalRes;
    case WLOCAL:
    case SLOCAL:
      return localRes;
    default:
      printf("Unknown state %d in Local history table!\n", bhtChoose[globalIndex]);
      return globalRes;
  }
}

void
train_tournament(uint32_t pc, uint8_t outcome) {
  // Global 2-bit Counter
  int globalNum = 1 << ghistoryBits;
  int globalIndex = (gHistory & (globalNum - 1));
  switch (bhtGlobal[globalIndex]) {
    case ST:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? ST : WT;
      break;
    case WT:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? ST : WN;
      break;
    case WN:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      bhtGlobal[globalIndex] = (outcome == TAKEN) ? WN : SN;
      break;
    default:
      printf("Unknown state %d in Global history table!\n", bhtGlobal[globalIndex]);
  }

  // Local 2-bit Counter
  int pcNum = 1 << pcIndexBits, lNum = 1 << lhistoryBits;
  int localIndex = (lHistory[(pc & (pcNum - 1))] & (lNum - 1));
  switch (bhtLocal[localIndex]) {
    case ST:
      bhtLocal[localIndex] = (outcome == TAKEN) ? ST : WT;
      break;
    case WT:
      bhtLocal[localIndex] = (outcome == TAKEN) ? ST : WN;
      break;
    case WN:
      bhtLocal[localIndex] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      bhtLocal[localIndex] = (outcome == TAKEN) ? WN : SN;
      break;
    default:
      printf("Unknown state %d in Local history table!\n", bhtLocal[localIndex]);
  }

  // Update Choose Predictor
  if (bhtLocal[localIndex] != bhtGlobal[globalIndex]) {
    switch (bhtChoose[globalIndex]) {
      case SGLOBAL:
        bhtChoose[globalIndex] = (outcome == bhtGlobal[globalIndex]) ? SGLOBAL : WGLOBAL;
        break;
      case WGLOBAL:
        bhtChoose[globalIndex] = (outcome == bhtGlobal[globalIndex]) ? SGLOBAL : WLOCAL;
        break;
      case WLOCAL:
        bhtChoose[globalIndex] = (outcome == bhtGlobal[globalIndex]) ? WGLOBAL : SLOCAL;
        break;
      case SLOCAL:
        bhtChoose[globalIndex] = (outcome == bhtGlobal[globalIndex]) ? WLOCAL : SLOCAL;
        break;
      default:
        printf("Unknown state %d in Choose History table!\n", bhtChoose[globalIndex]);
    }
  }

  // Update History
  gHistory = ((gHistory << 1) | outcome);
  lHistory[(pc & (pcNum - 1))] = ((lHistory[(pc & (pcNum - 1))] << 1) | outcome);
}

// Gshare Prediction & Training
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
      printf("Unknown state %d in Gshare history table!\n", bhtGlobal[index]);
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
      return make_prediction_tournament(pc);
    case CUSTOM:
      return make_prediction_custom(pc);
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
      break;
    case TOURNAMENT:
      train_tournament(pc, outcome);
      break;
    case CUSTOM:
      train_custom(pc, outcome);
      break;
    case STATIC:
    default:
      break;
  }
}
