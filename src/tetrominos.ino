#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "tetrominos.h"
#include "constants.h"




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


uint8_t *copyArray(uint8_t *src, uint8_t length) {
    uint8_t *p = (uint8_t *)malloc(length * sizeof(uint8_t));
    memcpy(p, src, length * sizeof(uint8_t));
    return p;
}


void make_timed_block(struct game_state *game, uint8_t index) {
    tetromino *ptr = (tetromino *)malloc(sizeof(struct tetromino)); // dynamically allocated to change the color
    ptr->length = game->tetromino_state.tetromino->length;
    ptr->blocks_amount = game->tetromino_state.tetromino->blocks_amount;
    ptr->blocks = copyArray(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length * game->tetromino_state.tetromino->length);
    change_color(ptr->blocks, ptr->length, 9);
    Serial.printf("changed");
    game->tetromino_state.tetromino = ptr;
    game->timed_block.active = 1;
    game->timed_block.limit = TIMED_BLOCK_LIMIT;
    game->timed_block.index = index;
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
 * Creates a random tetromino. Every so often, special tetrominos are generated, namely: mines, which can be used to blow up its 8 surrounding blocks, and
 * timed blocks, which must be completely removed before TIMED_BLOCK_LIMIT (10) number of tetrominos have fallen after it.
 * If a timed block is active, when a tetromino is spawned, the limit is decremented until the timed_block is made inactive or the limit is lower than 0 (game_over).
 */
void spawn_piece(struct game_state *game) {
    game->current_id++;
    uint8_t index = generate_random_value(TETROMINOS_AMOUNT);
    game->tetromino_state.tetromino = &TETROMINOS[index];
    if ((generate_random_value(RANDOM_MAX) < RANDOM_LIMIT_BOMB) && ((game->game_mode == mines_and_timed_blocks) || (game->game_mode == mines_only))) {
        game->tetromino_state.tetromino = &TETROMINOS[7];
        game->bomb.active = 1;
    }
    if ((generate_random_value(RANDOM_MAX) < RANDOM_LIMIT_TIMED_BLOCK) && !game->bomb.active && !game->timed_block.active && ((game->game_mode == mines_and_timed_blocks) || (game->game_mode == timed_blocks_only))) { // counter verhogen
        make_timed_block(game, index);
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
