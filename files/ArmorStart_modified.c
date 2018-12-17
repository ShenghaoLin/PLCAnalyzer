#include <stdbool.h>

int scaled_speed;
bool Sts_OperatorModeEnabled;
bool Sts_ProgramModeEnabled;

typedef struct {
  //
  // omitted struct fields
  //
} Inp_AS_284E;

typedef struct {
  //
  // omitted struct fields 
  //
  int FreqCommand;
} Out_AS_284E;


// add-on instruction
void AS_284E_AOI(Inp_AS_284E *in, int Inp_Scaled_Speed_At_400Hz) {
  int Set_SpeedOper = 10;
  int Set_SpeedProg = 20;
  //
  // omitted operations
  //

  int out_speed = 0;

  if (Sts_OperatorModeEnabled) {
    out_speed = (Set_SpeedOper * 4000) / Inp_Scaled_Speed_At_400Hz;
  }
  if (Sts_ProgramModeEnabled) {
    out_speed = (Set_SpeedProg * 4000) / Inp_Scaled_Speed_At_400Hz;
  }

}

int main() {
  Inp_AS_284E in;
  Out_AS_284E out;
  Sts_OperatorModeEnabled = true;
  Sts_ProgramModeEnabled = false;

  scaled_speed = 200;

  // ...
  AS_284E_AOI(&in, scaled_speed);

  return 0;
}