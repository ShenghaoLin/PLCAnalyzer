#include <stdbool.h>
#include <stdio.h>
#include "common_structs.h"

//
// global vars, e.g., global tags
//
bool Camera2_Inspect = 0;
bool PPX2 = 0;
bool RFIDTP2 = 0;
bool Mode_Start = 0;
bool Mode_Semi = 0;

void Cell_1() {
  // function body omitted for simplicity
}

void Cell_2() {
  //
  // a local var, e.g., a local tag
  //
  TIMER Cam2;
  Cam2.DN = 0;
  Cam2.EN = 0;
  Cam2.TT = 0;

  int i = 0;

  while (i++ < 5) {
    printf("Loop #%d\n", i);

    if (Mode_Start || Mode_Semi) {
      if ((PPX2 && RFIDTP2) || Cam2.TT) {
        if (!Cam2.DN) {
          Camera2_Inspect = 1;
          // enable timer
          Cam2.EN = 1;
          Cam2.TT = 1;
          Cam2.PRE = 3000;
          Cam2.ACC = 0;
        }  
      }
    }
    if (Cam2.EN) {
      Cam2.ACC += 2000;
      Cam2.DN = Cam2.ACC >= Cam2.PRE;
    }

    if (Mode_Start || Mode_Semi) {
      if (Cam2.DN) {
        // when will it be 1?
        Camera2_Inspect = 1;
      }
    }
  }
}

int main() {
  Cell_1();
  Cell_2();
}
