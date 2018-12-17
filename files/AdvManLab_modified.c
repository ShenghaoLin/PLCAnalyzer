#include <stdbool.h>
#include <stdio.h>
#include "common_structs.h"

//
// global vars, e.g., global tags
//
bool Camera1_Inspect = 0;
bool Camera2_Inspect = 0;
bool PPX2 = 0;
bool RFIDTP2 = 0;
bool Mode_Start = 0;
bool Mode_Semi = 0;
int Cell_1_input;
int Cell_2_input;

void Cell_1(int input);
void Cell_2(int input);

void Cell_1(int input) {
  int var = input;
  int angle = 3;
  Cell_2(var);
  if (var || Mode_Semi || Mode_Start) {
    angle = angle * 100000;
    RFIDTP2 = 1;
    PPX2 = 1;
  }
  // function body omitted for simplicity
}

void Cell_2(int input) {
  //
  // a local var, e.g., a local tag
  //
  // TIMER Cam2;
  int Cam2_DN = 0;
  int Cam2_EN = 0;
  int Cam2_TT = 0;
  int Cam2_ACC = 0;
  int Cam2_PRE = 0;

  int i = 0;

  while (i < 5) {
    //printf("Loop #%d\n", i);
    i ++;
    if (Mode_Start || Mode_Semi) {
      if ((PPX2 && RFIDTP2) || Cam2_TT) {
        if (!Cam2_DN) {
          Camera2_Inspect = 1;
          // enable timer
          Cam2_EN = 1;
          Cam2_TT = 1;
          Cam2_PRE = 3000;
          Cam2_ACC = 0;
        }  
      }
    }
    if (Cam2_EN) {
      Cam2_ACC = Cam2_ACC + 2000;
      Cam2_DN = Cam2_ACC >= Cam2_PRE;
    }

    if (Mode_Start || Mode_Semi) {
      if (Cam2_DN) {
        // when will it be 1?
        Camera2_Inspect = 1 + input; //Quality control
      }
    }
  }
}

int main() {
  Cell_2_input = 100;
  PPX2 = 0;
  Cell_1(Cell_1_input);
  Cell_2(Cell_2_input);
}
