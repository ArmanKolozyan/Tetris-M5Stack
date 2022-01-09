#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * All the different game_modes that the player, at the beginning of a game session, can choose.
 */
enum game_mode : byte {
    no_mines_no_timed_blocks,
    mines_only,
    timed_blocks_only,
    mines_and_timed_blocks
};

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

/**
 * The speed at which the tetrominos are moved downwards.
 */
struct speed {
    uint8_t limit;
    uint8_t counter;
};

struct accelerometer_data {
    float x;
    float y;
    float z;
};

/**
 * A tetromino that was installed in the playing field because it could no longer move.
 * The positions of the components of such a tetromino in the playing field are tracked to facilitate writing out and loading in the state of the game.
 * A free_index is provided to indicate the first free position in such a poisitions array, since positions can be both deleted and added.
 * Thus, such a tetromino is also flexible in its components (for possible further extensions).
 * Since the size of the array is not known in advance, it is allocated dynamically.
 */
struct installed_piece_data {
    uint id; // to distinguish between different installed tetrominos
    uint8_t color;
    uint8_t blocks_left; // to give the player bonus points when all blocks of a tetromino are cleared
    uint8_t free_index;
    uint8_t *positions;
};

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

/**
 * Structure that gathers the whole state of the game together.
 */
struct game_state {
    struct double_node *field[FIELD_WIDTH * FIELD_HEIGHT];

    enum game_mode game_mode;

    struct tetromino_state tetromino_state;

    struct timed_block timed_block;

    struct bomb bomb;

    struct speed speed;

    struct double_node *active_pieces;

    uint score;
    uint8_t game_over;
    uint8_t current_id;
};

// Since these two operations come in handy very often, I created the two abstractions below so as not to have to do the same computation all the time.
struct double_node *field_get(struct double_node **values, short row, short col) {
    short index = row * FIELD_WIDTH + col;
    return values[index];
}

void field_set(struct double_node **values, short row, short col, struct double_node *parent) {
    short index = row * FIELD_WIDTH + col;
    values[index] = parent;
}

/**
 * Each "cell" in the field must keep certain information about the tetromino it is part of, namely the installed_piece_data.
 * For this reason, each block in the playfield contains a pointer to a parent node, which contains the necessary information.
 * The data structure in which this information is maintained is a double-linked list, with installed_piece_data as its values.
 */
typedef struct double_node {
    struct installed_piece_data value;
    struct double_node *prev;
    struct double_node *next;
} Double_Node;

Double_Node *insert_before(installed_piece_data value, Double_Node *q) {
    Double_Node *p = (Double_Node *)malloc(sizeof(Double_Node));
    p->value = value;
    p->next = q;
    if (q != NULL) {
        Double_Node *prev_node = q->prev;
        q->prev = p;
        p->prev = prev_node;
        if (prev_node != NULL) {
            prev_node->next = p;
        }
    } else {
        p->prev = NULL;
    }
    return p;
}

Double_Node *insert_after(installed_piece_data value, Double_Node *q) {
    Double_Node *p = (Double_Node *)malloc(sizeof(Double_Node));
    p->value = value;
    p->prev = q;
    if (q != NULL) {
        Double_Node *next = q->next;
        q->next = p;
        p->next = next;
        if (next != NULL) {
            next->prev = p;
        }
    } else {
        p->next = NULL;
    }
    return p;
}

Double_Node *delete_node(uint id, Double_Node *start) {
    Double_Node *p;
    p = start;
    while (p != NULL && (p->value.id != id)) {
        p = p->next;
    }
    if (p != NULL) {
        Double_Node *next = p->next;
        Double_Node *prev = p->prev;
        if (prev != NULL) {
            prev->next = next;
            if (next != NULL) {
                next->prev = prev;
            }
        } else {
            start = p->next;
            if (next != NULL) {
                next->prev = NULL;
            }
        }
        free(p->value.positions);
        free(p);
    }
    return start;
}

uint8_t count_nodes(Double_Node *start);

uint8_t count_nodes(Double_Node *start) {
    Double_Node *p;
    p = start;
    uint8_t i = 0;

    while (p != NULL) {
        p = p->next;
        i++;
    }

    return i;
}

struct installed_piece_data create_installed_piece(uint id, uint8_t color, uint8_t blocks) {

    struct installed_piece_data s;

    // Dynamically allocating memory according to blocks_left
    s.positions = (uint8_t *)malloc(sizeof(uint8_t) * 2 * blocks); // LET OP: GEBRUIKMAKEN VAN POSITIE STRUCTS!!

    s.id = id;
    s.color = color;
    s.blocks_left = blocks;
    s.free_index = 0;

    return s;
}

/**
 * Replaces the last position with the position to be deleted and decrements the free_index.
 */
void delete_position(installed_piece_data *installed_piece_data, uint8_t x, uint8_t y) {

    uint8_t *positions = installed_piece_data->positions;

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        if ((positions[i] == x) && (positions[i + 1] == y)) {
            positions[i] = positions[installed_piece_data->free_index - 2];
            positions[i + 1] = positions[installed_piece_data->free_index - 1];
            installed_piece_data->free_index -= 2;
            break;
        }
    }
    installed_piece_data->blocks_left--;
}

/**
 * Replaces the first given position with the second given position.
 */
void change_position(installed_piece_data *installed_piece_data, uint8_t x, uint8_t y, uint8_t new_x, uint8_t new_y) {

    uint8_t *positions = installed_piece_data->positions;

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        Serial.printf("POSITION BEFORE: %i\n", positions[i]);
    }

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        if ((positions[i] == x) && (positions[i + 1] == y)) {
            positions[i] = new_x;
            positions[i + 1] = new_y;
            break;
        }
    }

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        Serial.printf("POSITION AFTER: %i\n", positions[i]);
    }
}

/**
 * Returns the state of a component of a tetromino taking into account its rotation.
 * In other words, an indexing in the blocks array of a given tetromino is used depending on its rotation.
 */
uint8_t get_component(struct tetromino *tetromino, short row, short column, uint8_t rotation) {
    uint8_t length = tetromino->length;
    uint8_t value;
    switch (rotation) {
    case 0:
        value = tetromino->blocks[row * length + column];
        break;
        // 0  1   2  3
        // 4  5   6  7
        // 8  9  10 11
        // 12 13  14 15
    case 1:
        value = tetromino->blocks[(length - column - 1) * length + row];
        break;
        // 12  8   4  0
        // 13  9   5  1
        // 14  10  6  2
        // 15  11  7  3
    case 2:
        value = tetromino->blocks[(length - row - 1) * length + (length - column - 1)];
        break;
        //  15  14  13 12
        //  11  10  9  8
        //  7   6   5  4
        //  3   2   1  0
    case 3:
        value = tetromino->blocks[column * length + (length - row - 1)];
        break;
        // 3  7  11  15
        // 2  6  10  14
        // 1  5   9  13
        // 0  4   8  12
    default: // to catch wrong input
        value = tetromino->blocks[row * length + column];
        break;
    }
    return value;
}

/**
 * Extracts the color from the array of blocks.
 */
uint8_t determine_color(uint8_t *blocks, uint8_t length) {
    for (uint8_t i = 0; i < length * length; i++) {
        if (blocks[i] != 0) {
            Serial.printf("%i", blocks[i]);
            return blocks[i];
        }
    }
    return 0;
}

/**
 * Changes the color in the array of blocks.
 */
void change_color(uint8_t *blocks, uint8_t length, uint8_t new_color) {
    for (uint8_t i = 0; i < length * length; i++) {
        if (blocks[i] != 0) {
            blocks[i] = new_color;
        }
    }
}

/**
 * Installs the current tetromino in the field (when it can no longer move).
 * Installation is done by pointing each of its components in the field
 * to a newly added installed_piece_data node.
 */
void install_tetromino_in_field(struct game_state *game) {
    struct installed_piece_data value = create_installed_piece(game->current_id, determine_color(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length), game->tetromino_state.tetromino->blocks_amount);
    game->active_pieces = insert_before(value, game->active_pieces);
    struct tetromino *tetromino = game->tetromino_state.tetromino;
    for (uint8_t row = 0;
         row < tetromino->length;
         ++row) {
        for (uint8_t column = 0;
             column < tetromino->length;
             ++column) {
            uint8_t value = get_component(tetromino, row, column, game->tetromino_state.rotation);
            if (value) {
                short field_row = game->tetromino_state.row_in_field + row;
                short field_column = game->tetromino_state.column_in_field + column;
                struct double_node *new_node = game->active_pieces;
                uint8_t free_index = game->active_pieces->value.free_index; // in een aparte functie want komt 2 keer voor
                game->active_pieces->value.positions[free_index] = (uint8_t)field_column;
                game->active_pieces->value.positions[free_index + 1] = (uint8_t)field_row;
                game->active_pieces->value.free_index += 2;
                field_set(game->field, field_row, field_column, new_node);
            }
        }
    }
    if (determine_color(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length) == 9) {
        free(game->tetromino_state.tetromino->blocks);
        free(game->tetromino_state.tetromino);
    }
}

#define OUTSIDE_WIDTH 2
#define OUTSIDE_HEIGHT 3
#define COLLISION 4
#define CAN_MOVE 5
/**
 * Checks whether the tetromino that is currently falling down can move further.
 * It returns a number that indicates corresponding information.
 * 2 => outside the vertical bounds (width)
 * 3 => outside the horizontal bounds (height)
 * 4 => in the position of a component, there is already another component
 * 5 => the tetromino can move safely
 */
uint8_t piece_fits(struct tetromino_state tetromino_state, struct game_state *game_state, uint8_t width, uint8_t height) {

    struct tetromino *tetromino = game_state->tetromino_state.tetromino;

    for (short row = 0;
         row < tetromino->length;
         ++row) {
        for (short column = 0;
             column < tetromino->length;
             ++column) {
            uint8_t component_value = get_component(tetromino, row, column, tetromino_state.rotation);
            if (component_value > 0) {
                short field_row = tetromino_state.row_in_field + row;
                short field_column = tetromino_state.column_in_field + column;
                if (field_column >= 0 && field_column < FIELD_WIDTH) {
                    if (field_row >= 0 && field_row < FIELD_HEIGHT) {
                        if (field_get(game_state->field, field_row, field_column)) {
                            return COLLISION;
                        }
                    } else {
                        return OUTSIDE_HEIGHT;
                    }
                } else {
                    return OUTSIDE_WIDTH;
                }
            }
        }
    }
    return CAN_MOVE;
}

uint8_t possible_to_lower(struct game_state *game_state) {
    tetromino_state tetromino = game_state->tetromino_state;
    tetromino.row_in_field += 1;
    return (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT) == CAN_MOVE);
}

#define SPEED_INCREASE 10

/**
 * Does the necessary actions depending on the button(s) pressed and the tilt of the M5Stack.
 */
void handle_input(struct game_state *game_state, accelerometer_data accelerometer_data) {

    if (M5.BtnA.wasPressed() && M5.BtnB.wasPressed()) {
        write_data(game_state);

    } else if (M5.BtnB.wasPressed()) {
        load_data(game_state);

    } else {
        tetromino_state tetromino = game_state->tetromino_state;

        if (accelerometer_data.x > ACC_MIN_LEFT) {
            tetromino.column_in_field--;
            delay(100);

        } else if (accelerometer_data.x < ACC_MIN_RIGHT) {
            tetromino.column_in_field++;
            delay(100);
        }

        if (accelerometer_data.y > ACC_MIN_FAST) {
            game_state->speed.counter += SPEED_INCREASE;

        } else if (accelerometer_data.y < ACC_MIN_SLOW) {
            if (game_state->speed.counter > 0) {
                game_state->speed.counter--;
            }
        }

        if (M5.BtnA.wasPressed() && !M5.BtnB.wasPressed()) {
            tetromino.rotation = (tetromino.rotation + 1) % 4;
        }

        uint8_t piece_orientation = piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT);

        if (piece_orientation == CAN_MOVE) {
            game_state->tetromino_state = tetromino;
        } else if (game_state->bomb.active && !(piece_orientation == OUTSIDE_WIDTH)) {
            explode(game_state, game_state->tetromino_state.column_in_field, game_state->tetromino_state.row_in_field);
            spawn_piece(game_state);
        }
    }
}

/**
 * The current state of the game can be saved by pressing the A and B buttons at the same time.
 */
void write_data(struct game_state *game_state) {

    uint address = 0;

    uint8_t game_mode;
    switch (game_state->game_mode) {
    case mines_and_timed_blocks:
        game_mode = 1;
    case no_mines_no_timed_blocks:
        game_mode = 2;
    case mines_only:
        game_mode = 3;
    case timed_blocks_only:
        game_mode = 4;
    }
    uint8_t shifted_game_mode = game_mode << 6;
    uint8_t shifted_game_over = game_state->game_over << 5;
    uint8_t shifted_bomb_active = game_state->bomb.active << 4;
    uint8_t shifted_timed_block_active = game_state->timed_block.active << 3;
    uint8_t encoded_1 = shifted_game_mode | shifted_game_over | shifted_bomb_active | shifted_timed_block_active | game_state->timed_block.index;

    Serial.printf("encoded: %u\n", encoded_1);
    EEPROM.writeByte(address, encoded_1);
    address++;

    // It is a variabel struct member so that a programmer can make changes to it in his game (i.e. to promote extensibility and adaptability).
    // In my implementation, however, the timed_block limit can be up to 7.

    uint8_t shifted_timed_block_limit = game_state->timed_block.limit << 5;
    uint8_t encoded_2 = shifted_timed_block_limit | game_state->speed.counter;
    EEPROM.writeByte(address, encoded_2);
    address++;

    // Also, technically, no maximum can be placed on the speed_counter and speed_limit.
    // They are variabel struct members so that a programmer can make it changes to it in his game,
    // for example, when introducing multiple levels (i.e. to promote extensibility and adaptability).
    // In my implementation, however, the counter can be up to 31 and the limit up to 30.

    EEPROM.writeByte(address, game_state->speed.limit);
    address++;

    // field pieces
    struct double_node *field_pieces = game_state->active_pieces;

    // A field with my type of tetrominos can contain up to 50 tetrominos.
    // That means 6 bits. Splitting it up with other information makes little sense and is also not possible.
    uint8_t amount = count_nodes(field_pieces);
    EEPROM.writeByte(address, amount);
    address++;

    uint8_t shifted_color;
    uint8_t shifted_blocks_left;
    uint8_t encoded_3;

    while (field_pieces != NULL) {
        // Another programmer could introduce many more colors, but in my case there are only 9 different colors. => 4 bits
        // A tetromino could contain a maximum of 4 blocks. => 3 bits
        shifted_color = field_pieces->value.color << 3;
        shifted_blocks_left = field_pieces->value.blocks_left;
        encoded_3 = shifted_color | shifted_blocks_left;
        EEPROM.writeByte(address, encoded_3);
        address++;
        for (uint8_t i = 0; i < field_pieces->value.free_index; i++) {
            EEPROM.writeByte(address, field_pieces->value.positions[i]);
            Serial.printf("points sending: %u \n", field_pieces->value.positions[i]);
            address++;
        }
        field_pieces = field_pieces->next;
    }

    // current tetromino
    EEPROM.writeByte(address, determine_color(game_state->tetromino_state.tetromino->blocks, game_state->tetromino_state.tetromino->length));
    address++;
    EEPROM.writeByte(address, (uint8_t)game_state->tetromino_state.column_in_field);
    address++;
    EEPROM.writeByte(address, (uint8_t)game_state->tetromino_state.row_in_field);
    address++;
    EEPROM.writeByte(address, game_state->tetromino_state.rotation);
    address++;

    // score
    EEPROM.writeUInt(address, game_state->score); // Sticking a limit on the score would not be a good idea.
    address += sizeof(uint32_t);

    EEPROM.commit();
}

/**
 * The saved state can be loaded by pressing the B button.
 */
void load_data(struct game_state *game_state) {

    uint address = 0;

    uint8_t encoded_1 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_game_mode = 0b11000000;
    uint8_t shifted_game_mode_encoded = encoded_1 & bitmask_game_mode;
    switch (shifted_game_mode_encoded >> 6) {
    case 1:
        game_state->game_mode = mines_and_timed_blocks;
    case 2:
        game_state->game_mode = no_mines_no_timed_blocks;
    case 3:
        game_state->game_mode = mines_only;
    case 4:
        game_state->game_mode = timed_blocks_only;
    }

    uint8_t bitmask_game_over = 0b00100000;
    uint8_t shifted_game_over_encoded = encoded_1 & bitmask_game_over;
    game_state->game_over = shifted_game_over_encoded >> 5;

    uint8_t bitmask_bomb_active = 0b00010000;
    uint8_t shifted_bomb_active_encoded = encoded_1 & bitmask_bomb_active;
    game_state->bomb.active = shifted_bomb_active_encoded >> 4;

    uint8_t bitmask_timed_block_active = 0b00001000;
    uint8_t shifted_timed_block_active_encoded = encoded_1 & bitmask_timed_block_active;
    game_state->timed_block.active = shifted_timed_block_active_encoded >> 3;

    uint8_t bitmask_timed_block_index = 0b00000111;
    uint8_t shifted_timed_block_index_encoded = encoded_1 & bitmask_timed_block_index;
    game_state->timed_block.index = shifted_timed_block_index_encoded;

    //(timed_block &) speed

    uint8_t encoded_2 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_timed_block_limit = 0b11100000;
    uint8_t shifted_timed_block_limit_encoded = encoded_2 & bitmask_timed_block_limit;
    game_state->timed_block.limit = shifted_timed_block_limit_encoded >> 5;

    uint8_t bitmask_speed_counter = 0b00011111;
    uint8_t shifted_speed_counter_encoded = encoded_2 & bitmask_speed_counter;
    game_state->speed.counter = shifted_speed_counter_encoded;
    Serial.printf("decoded: %u\n", encoded_2);

    game_state->speed.limit = EEPROM.readByte(address);
    address++;

    // field pieces
    for (uint8_t row = 0; row < FIELD_HEIGHT; row++) {
        for (uint8_t column = 0; column < FIELD_WIDTH; column++) {
            if (game_state->field[row * FIELD_WIDTH + column] != NULL) {
                game_state->active_pieces = delete_node(game_state->field[row * FIELD_WIDTH + column]->value.id, game_state->active_pieces);
                game_state->field[row * FIELD_WIDTH + column] = NULL;
            }
        }
    }
    uint8_t counter = EEPROM.readByte(address);
    address++;
    Serial.printf("amount receiving: %u\n", counter);
    game_state->active_pieces = NULL;
    game_state->current_id = 1;

    uint8_t bitmask_timed_block_active = 0b00001000;
    uint8_t shifted_timed_block_active_encoded = encoded_1 & bitmask_timed_block_active;
    game_state->timed_block.active = shifted_timed_block_active_encoded >> 3;

    uint8_t encoded_3;
    address++;
    uint8_t bitmask_color = 0b01111000;
    uint8_t shifted_color;
    uint8_t bitmask_blocks_left = 0b00000111;
    uint8_t shifted_blocks_left;
    uint8_t color;
    uint8_t blocks_left;

    shifted_color = field_pieces->value.color << 3;
    shifted_blocks_left = field_pieces->value.blocks_left;
    encoded_3 = shifted_color | shifted_blocks_left;
    EEPROM.writeByte(address, encoded_3);

    for (uint8_t i = counter; i > 0; i--) {
        encoded_3 = EEPROM.readByte(address);
        shifted_color = encoded_3 & bitmask_color;
        shifted_blocks_left = encoded_3 & bitmask_blocks_left;
         color = shifted_color >> shifted_blocks_left;
        blocks_left = shifted;
        Serial.printf("blocks receiving: %i\n", blocks_left);
        address++;
        struct installed_piece_data value = create_installed_piece(game_state->current_id, color, blocks_left);
        game_state->active_pieces = insert_before(value, game_state->active_pieces);
        game_state->current_id++;
        for (uint8_t i = 0; i < game_state->active_pieces->value.blocks_left; i++) {
            uint8_t column = EEPROM.readByte(address);
            address++;
            uint8_t row = EEPROM.readByte(address);
            address++;
            game_state->field[row * FIELD_WIDTH + column] = game_state->active_pieces;
            uint8_t free_index = game_state->active_pieces->value.free_index;
            game_state->active_pieces->value.positions[free_index] = column;
            game_state->active_pieces->value.positions[free_index + 1] = row; // beter aparte functie hiervoor maken???
            game_state->active_pieces->value.free_index += 2;
        }
    }

    // current tetromino
    uint8_t color = EEPROM.readByte(address);
    if (color != 9) {
        game_state->tetromino_state.tetromino = &TETROMINOS[color - 1];
    } else {
        game_state->tetromino_state.tetromino = &TETROMINOS[game_state->timed_block.index];
    }
    address++;
    game_state->tetromino_state.column_in_field = (short)EEPROM.readByte(address);
    address++;
    game_state->tetromino_state.row_in_field = (short)EEPROM.readByte(address);
    address++;
    game_state->tetromino_state.rotation = EEPROM.readByte(address);
    address++;

    // score
    game_state->score = EEPROM.readUInt(address);
    Serial.printf("score receiving: %u\n", game_state->score);
    address += sizeof(uint32_t);
}

/**
 * Numbers are associated with colors to save memory and promote extensibility and adaptability.
 */
uint32_t give_color(uint8_t n) {
    uint32_t color;
    switch (n) {
    case 0:
        color = TFT_BLACK;
        break;
    case 1:
        color = TFT_BLUE;
        break;
    case 2:
        color = TFT_GREEN;
        break;
    case 3:
        color = TFT_PURPLE;
        break;
    case 4:
        color = TFT_CYAN;
        break;
    case 5:
        color = TFT_ORANGE;
        break;
    case 6:
        color = TFT_PINK;
        break;
    case 7:
        color = TFT_YELLOW;
        break;
    case 8:
        color = TFT_DARKGREY;
        break;
    case 9:
        color = TFT_RED;
        break;
    default:
        color = TFT_BLACK;
        break;
    }
    return color;
}

/**
 * Given a column and a row, this function brings down all components that
 * lie above the given row in the given column until a hole is encountered.
 * This function is needed to implement the logic of mines.
 */
void bring_down(struct double_node **field, short x, short y) {
    for (short line_y = y; line_y > 0; line_y--) {
        struct double_node *component_above = field_get(field, line_y - 1, x);
        field_set(field, line_y, x, component_above);
        if (!component_above) { // check for NULL (i.e. empty) block
            return;
        } else {
            change_position(&field_get(field, line_y, x)->value, x, line_y - 1, x, line_y);
            field_set(field, line_y - 1, x, NULL);
        }
    }
}

#define LINE_CLEAR_SCORE_INCREASE 50

/**
 * Deletes a field block from the playing field. In case of a timed block, the dynamically allocated
 * memory is feeed.
 */
void delete_component(struct game_state *game_state, struct double_node *component, uint8_t column, uint8_t row) {
    if (component) { // checking if not NULL
        delete_position(&component->value, column, row);
        if (field_get(game_state->field, row, column)->value.color == 9) {
            if (field_get(game_state->field, row, column)->value.blocks_left == 0) {
                game_state->timed_block.active = 0;
            }
        } else if (field_get(game_state->field, row, column)->value.blocks_left == 0) {
            Serial.printf("giuuu");
            game_state->score += LINE_CLEAR_SCORE_INCREASE;
            game_state->active_pieces = delete_node(component->value.id, game_state->active_pieces);
        }
        field_set(game_state->field, row, column, NULL);
    }
}

/**
 * When a mine explodes in column x and row y, every block lying above and on row y-1
 * in the columns x-1, x and x +1 are brought down until a hole is encountered.
 */
void explode(struct game_state *game_state, short bomb_x, short bomb_y) {
    game_state->bomb.active = 0;
    for (short off_y = -1; off_y <= 1; off_y++) {
        for (short off_x = -1; off_x <= 1; off_x++) { // checking all the neighbours of the bomb
            short neighbour_y = bomb_y + off_y;
            short neighbour_x = bomb_x + off_x;
            if ((neighbour_y < 0) || (neighbour_y >= FIELD_HEIGHT)) { // checking if we are not off the field
                continue;
            }
            if ((neighbour_x < 0) || (neighbour_x >= FIELD_WIDTH)) { // checking if we are not off the field
                continue;
            }
            {
                struct double_node *current_component = field_get(game_state->field, neighbour_y, neighbour_x);
                delete_component(game_state, current_component, neighbour_x, neighbour_y);
                bring_down(game_state->field, neighbour_x, neighbour_y);
            }
        }
    }
}

/**
 * Removes the given row and brings down all the rows above it.
 */
void clear_line(game_state *game_state, short y) {
    for (uint8_t line_x = 0; line_x < FIELD_WIDTH; line_x++) {
        for (short line_y = y; line_y > 0; line_y--) {
            if (line_y == y) {
                struct double_node *current_component = field_get(game_state->field, line_y, line_x);
                delete_component(game_state, current_component, line_x, line_y);
            }
            field_set(game_state->field, line_y, line_x, field_get(game_state->field, line_y - 1, line_x));
            if (field_get(game_state->field, line_y - 1, line_x)) { // check for NULL (i.e. empty) block
                change_position(&field_get(game_state->field, line_y, line_x)->value, line_x, line_y - 1, line_x, line_y);
                field_set(game_state->field, line_y - 1, line_x, NULL);
            }
        }
        game_state->field[line_x] = NULL;
    }
}

#define MEM_SIZE 512

void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(MEM_SIZE);
    M5.Lcd.fillScreen(TFT_BLACK);
    Serial.begin(115200);
    Serial.flush();
}

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

uint8_t generate_random_value(uint8_t max) {
    return rand() % max;
}
#define RANDOM_MAX 20
#define RANDOM_LIMIT_TIMED_BLOCK 5
#define RANDOM_LIMIT_BOMB 3
#define TIMED_BLOCK_LIMIT 7
#define TETROMINOS_AMOUNT 7

uint8_t *copyArray(uint8_t *src, uint8_t length) {
    uint8_t *p = (uint8_t *)malloc(length * sizeof(uint8_t));
    memcpy(p, src, length * sizeof(uint8_t));
    return p;
}

/**
 * Creates a random tetromino. Every so often, special tetrominos are generated, namely: mines, which can be used to blow up its 8 surrounding blocks, and
 * timed blocks, which must be completely removed before TIMED_BLOCK_LIMIT (10) number of tetrominos have fallen after it.
 * If a timed block is active, when a tetromino is spawned, the limit is decremented until the timed_block is made inactive or the limit is lower than 0 (game_over).
 */
void spawn_piece(struct game_state *game) {
    game->current_id++;
    uint8_t index = generate_random_value(TETROMINOS_AMOUNT);
    game->tetromino_state.tetromino = &TETROMINOS[index];
    if (generate_random_value(RANDOM_MAX) < RANDOM_LIMIT_BOMB) {
        game->tetromino_state.tetromino = &TETROMINOS[7];
        game->bomb.active = 1;
    }
    if ((generate_random_value(RANDOM_MAX) < RANDOM_LIMIT_TIMED_BLOCK) && !game->bomb.active && !game->timed_block.active) { // counter verhogen
        Serial.printf("hello");
        tetromino *ptr = (tetromino *)malloc(sizeof(struct tetromino)); // dynamically allocated to change the color
        ptr->length = game->tetromino_state.tetromino->length;
        ptr->blocks_amount = game->tetromino_state.tetromino->blocks_amount;
        ptr->blocks = copyArray(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length * game->tetromino_state.tetromino->length);
        change_color(ptr->blocks, ptr->length, 9);
        game->tetromino_state.tetromino = ptr;
        game->timed_block.active = 1;
        game->timed_block.limit = TIMED_BLOCK_LIMIT;
        game->timed_block.index = index;
    } else if (game->timed_block.active) {
        if ((game->timed_block.limit) < 0) {
            game->game_over = 1;
        }
        game->timed_block.limit--;
    }
    game->tetromino_state.row_in_field = 0;
    game->tetromino_state.column_in_field = FIELD_WIDTH / 2;
    game->tetromino_state.rotation = 0;
    game->speed.limit = 30 - (uint8_t)0.1 * game->score; // test het eens door prints of het OK is
}

/**
 * This function checks whether the given row is fully occupied by blocks.
 * When it encounters a hole in the row it returns 0 (false), otherwise it returns 1(true).
 */
uint8_t is_line_full(double_node **field, short y) {
    uint8_t line_full = 1;
    for (uint8_t block_x = 1; block_x < FIELD_WIDTH; block_x++) {
        if (!field_get(field, y, block_x)) { // NULL-pointer check
            line_full = 0;
            return line_full;
        }
    }
    return line_full;
}

/**
 * Chechks if the tetromino that just got installed caused a full line formation.
 * If so, the affected lines are cleared.
 */
void handle_full_lines(struct game_state *game_state) {
    for (uint8_t block_y = 0; block_y < game_state->tetromino_state.tetromino->length; block_y++) {
        if (game_state->tetromino_state.row_in_field + block_y < FIELD_HEIGHT) {
            if (is_line_full(game_state->field, game_state->tetromino_state.row_in_field + block_y)) {
                clear_line(game_state, game_state->tetromino_state.row_in_field + block_y);
                game_state->speed.counter--;
                game_state->score += 150;
            }
        }
    }
}

void loop() {
    // Since the M5Stack has no clock, the random_generator cannot be seeded with time(NULL).
    // For that reason, the accelerometer is used here to obtain a somewhat random value.
    float random_seeder;
    M5.Imu.getAccelData(&random_seeder, &random_seeder, &random_seeder);
    srand(random_seeder);

    M5.update();
    struct accelerometer_data accelerometer_data;
    struct game_state game_state;
    game_state.game_mode = mines_and_timed_blocks;

    game_state.current_id = 0;
    game_state.timed_block.active = 0;

    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            field_set(game_state.field, y, x, NULL);
        }
    }
    game_state.active_pieces = NULL;
    spawn_piece(&game_state);

    while (!game_state.game_over) {
        M5.update(); // moet er gewoon standard staan
        delay(10);
        M5.Imu.getAccelData(&accelerometer_data.x, &accelerometer_data.y, &accelerometer_data.z);
        handle_input(&game_state, accelerometer_data);
        game_state.speed.counter++;

        if (game_state.speed.counter > game_state.speed.limit) {
            game_state.speed.counter = 0;
            if (game_state.bomb.active) {
                if (possible_to_lower(&game_state)) {
                    game_state.tetromino_state.row_in_field++;
                } else {
                    explode(&game_state, game_state.tetromino_state.column_in_field, game_state.tetromino_state.row_in_field);
                    spawn_piece(&game_state);
                }
            } else if (possible_to_lower(&game_state)) {
                game_state.tetromino_state.row_in_field++;
            } else {
                if (piece_fits(game_state.tetromino_state, &game_state, FIELD_WIDTH, FIELD_HEIGHT) != CAN_MOVE) {
                    game_state.game_over = 1;
                    break;
                }
                install_tetromino_in_field(&game_state);
                handle_full_lines(&game_state);

                game_state.score += 12;
                spawn_piece(&game_state);
            }
        }
        // Draw Field
        for (uint8_t x = 0; x < FIELD_WIDTH; x++)
            for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
                if (game_state.field[y * FIELD_WIDTH + x] != NULL) {
                    M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(game_state.field[y * FIELD_WIDTH + x]->value.color));
                } else
                    M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(0));
            }

        // Draw Current Piece
        struct tetromino *tetromino = game_state.tetromino_state.tetromino;
        for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
            for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
                uint8_t value = get_component(tetromino, block_y, block_x, game_state.tetromino_state.rotation);
                if (value) // binnen 1 piece geen 2D array, je houdt wel een 2D array bij van pieces
                {
                    M5.Lcd.fillRect((game_state.tetromino_state.column_in_field + block_x) * 8, (game_state.tetromino_state.row_in_field + block_y) * 8, 8, 8, give_color(value));
                }
            }
    }
}
