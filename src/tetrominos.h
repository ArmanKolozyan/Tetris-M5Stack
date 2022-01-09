#ifndef tetrominos
#define tetrominos


/**
 * Unique information on each of the 7 possible tetrominos.
 */
struct tetromino {
    uint8_t *blocks;
    uint8_t length;
    uint8_t blocks_amount;
};

/**
 * The tetromino that is currently falling down.
 */
struct tetromino_state {
    short row_in_field;
    short column_in_field;
    uint8_t rotation;
    struct tetromino *tetromino;
};

/**
 * When a new block is generated to drop, this block is sometimes turned into a timed block.
 * Once this block is dropped, the player is given a certain amount of "time" to completely remove this block.
 * By "time" in this implementation is meant 10 tetrominos that have fallen after the generation of the timed_block have fallen.
 * Thus, at the eleventh block, the game is over if the timed_block is not cleared by then.
 * About such a block we save the following informations: whether it is active, what the limit is, in this case 10,
 * and the index of the tetromino (in TETROMINOS) that was converted into a timed block.
 */
struct timed_block {
    uint8_t active = 0;
    uint8_t limit = 0;
    uint8_t index = 0;
};

/**
 * A bomb is a special 1×1 block that blows up itself and its eight neighboring blocks
 * when it collides with another block or when it reaches the bottom of the field.
 * Of these bombs, we track whether it is active or not.
 */
struct bomb {
    uint8_t active;
};


// The seven known tetromino forms and the bomb.
// The arrays contain unique numbers to distinguish color.
uint8_t TETROMINO_1[] = {
    0, 0, 0, 0,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0};

uint8_t TETROMINO_2[] = {
    2, 2,
    2, 2};

uint8_t TETROMINO_3[] = {
    0, 0, 0,
    3, 3, 3,
    0, 3, 0};

uint8_t TETROMINO_4[] = {
    4, 0, 0,
    4, 4, 4,
    0, 0, 0};

uint8_t TETROMINO_5[] = {
    0, 0, 5,
    5, 5, 5,
    0, 0, 0};

uint8_t TETROMINO_6[] = {
    0, 6, 6,
    6, 6, 0,
    0, 0, 0};

uint8_t TETROMINO_7[] = {
    7, 7, 0,
    0, 7, 7,
    0, 0, 0};

uint8_t TETROMINO_8[] = {
    8};

struct tetromino TETROMINOS[] = {
    {TETROMINO_1, 4, 4},
    {TETROMINO_2, 2, 4},
    {TETROMINO_3, 3, 4},
    {TETROMINO_4, 3, 4},
    {TETROMINO_5, 3, 4},
    {TETROMINO_6, 3, 4},
    {TETROMINO_7, 3, 4},
    {TETROMINO_8, 1, 1}};

#endif