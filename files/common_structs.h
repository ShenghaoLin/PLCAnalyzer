#ifndef _COMMON_STRUCTS_H_
#define _COMMON_STRUCTS_H_

#include <stdbool.h>

/**
 * Tag data type -> C data type
 * DINT -> int
 * BOOL -> bool
 */

typedef struct {
  int PRE;
  int ACC;
  bool EN;
  bool TT;
  bool DN;
} TIMER;

#endif /* _COMMON_STRUCTS_H_ */