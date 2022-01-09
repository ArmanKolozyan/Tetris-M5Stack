#ifndef constants
#define constants

#define RANDOM_MAX 20
#define RANDOM_LIMIT_TIMED_BLOCK 5
#define RANDOM_LIMIT_BOMB 3
#define TIMED_BLOCK_LIMIT 7
#define TETROMINOS_AMOUNT 7

// Constants regarding the size of the field (both drawing and game-logic)
#define DISPLAY_HEIGHT 160
#define DISPLAY_WIDTH 80
#define FIELD_HEIGHT 20
#define FIELD_WIDTH 10
#define field_SIZE FIELD_HEIGHT *FIELD_WIDTH

// Constants that indicate how far the player should tilt the M5Stack to send
// the tetromino that is currently moving downward to the left/right or to
// let it move faster/slower.
#define ACC_MIN_LEFT 0.15
#define ACC_MIN_RIGHT -0.15
#define ACC_MIN_FAST 0.95
#define ACC_MIN_SLOW -0.50

#define SPEED_INCREASE 10







#define LINE_CLEAR_SCORE_INCREASE 50







#define MEM_SIZE 512


#define OUTSIDE_WIDTH 2
#define OUTSIDE_HEIGHT 3
#define COLLISION 4
#define CAN_MOVE 5


#endif