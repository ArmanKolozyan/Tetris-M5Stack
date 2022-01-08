#include "EEPROM.h"
#include <M5StickC.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

enum game_mode : byte {
    no_mines_no_timed_blocks,
    mines_only,
    timed_blocks_only,
    mines_and_timed_blocks
};

#define DISPLAY_HEIGHT 160
#define DISPLAY_WIDTH 80

#define FIELD_HEIGHT 20
#define FIELD_WIDTH 10
#define BOARD_SIZE FIELD_HEIGHT *FIELD_WIDTH

#define MIN_TILT 0.15

struct timed_block {
    uint8_t active = 0;
    uint8_t limit = 0;
    uint8_t index = 0;
};

struct bomb {
    uint8_t active = 0;
    uint8_t handled = 0;
};

struct speed {
    uint8_t limit = 30;
    uint8_t counter = 0;
};

struct accelerometer_data {
    float x = 0;
    float y = 0;
    float z = 0;
};

struct saved_piece_data {
    uint id;
    uint8_t color;
    uint8_t blocks_left;
    uint8_t free_index;
    uint8_t *positions;
};

struct tetromino { // => gewoon index en kleur
    uint8_t *blocks;
    uint8_t length;
    uint8_t total_blocks;
    uint8_t color;
};

struct tetromino_state {
    short row_in_field = 0;
    short column_in_field = 0;
    uint8_t rotation = 0;
    struct tetromino *tetromino; // => gewoon index en kleur
};

struct game_state {
    struct double_node *board[FIELD_WIDTH * FIELD_HEIGHT];

    enum game_mode game_mode = mines_and_timed_blocks;

    uint8_t move_down = 0;
    uint8_t current_id = 0;

    struct tetromino_state tetromino_state;

    struct timed_block timed_block;

    struct bomb bomb;

    struct speed speed;

    struct double_node *active_pieces;

    uint score;
    uint8_t game_over;
};

typedef struct double_node {
    struct saved_piece_data value;
    struct double_node *prev;
    struct double_node *next;
} Double_Node;

struct saved_piece_data create_saved_piece(uint id, uint8_t color, uint8_t blocks) {

    struct saved_piece_data s;

    // Allocating memory according to blocks_left
    s.positions = (uint8_t *)malloc(sizeof(uint8_t) * 2 * blocks);

    s.id = id;
    s.color = color;
    s.blocks_left = blocks;
    s.free_index = 0;

    return s;
}

uint8_t count_elements(Double_Node *start) {
    Double_Node *p;
    p = start;
    uint8_t i = 0;

    while (p != NULL) {
        p = p->next;
        i++;
    }

    return i;
}

Double_Node *insert_before(saved_piece_data value, Double_Node *q) {
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

Double_Node *insert_after(saved_piece_data value, Double_Node *q) {
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

/*
 * Debugging functie.
 */
void print_double_linked_list(Double_Node *p) {
    while (p != NULL) {
        printf("%p: value: %i, prev: %p, next: %p\n", p, p->value, p->prev, p->next);
        p = p->next;
    }
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

/*
Double_Node *find_start(Double_Node *current) {
    Double_Node *start;
    start = current;
    if (start == NULL) {
        return start;
    }
    while (start->prev != NULL) {
        start = start->prev;
    }
    return start;
}
*/

struct double_node *board_get(struct double_node **values, uint8_t width, short row, short col) {
    short index = row * width + col;
    return values[index];
}

void board_set(struct double_node **values, uint8_t width, short row, short col, struct double_node *parent) {
    short index = row * width + col;
    values[index] = parent;
}

void delete_position(saved_piece_data *saved_piece_data, uint8_t x, uint8_t y) {
    uint8_t i = 0;

    while ((saved_piece_data->positions[i] != x) || (saved_piece_data->positions[i + 1] != y)) {
        i += 1;
    }
    saved_piece_data->positions[i] = saved_piece_data->positions[saved_piece_data->free_index - 2];
    saved_piece_data->positions[i + 1] = saved_piece_data->positions[saved_piece_data->free_index - 1];
    saved_piece_data->free_index -= 2;
}

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
    0, 4, 4,
    4, 4, 0,
    0, 0, 0};

uint8_t TETROMINO_5[] = {
    5, 5, 0,
    0, 5, 5,
    0, 0, 0};

uint8_t TETROMINO_6[] = {
    6, 0, 0,
    6, 6, 6,
    0, 0, 0};

uint8_t TETROMINO_7[] = {
    0, 0, 7,
    7, 7, 7,
    0, 0, 0};

uint8_t TETROMINO_8[] = {
    8};

uint8_t TETROMINO_9[] = {
    9};

struct tetromino TETROMINOS[] = {
    {TETROMINO_1, 4, 4, 1}, // misschien de lengte met een functie berekenen?
    {TETROMINO_2, 2, 4, 2},
    {TETROMINO_3, 3, 4, 3},
    {TETROMINO_4, 3, 4, 4},
    {TETROMINO_5, 3, 4, 5},
    {TETROMINO_6, 3, 4, 6},
    {TETROMINO_7, 3, 4, 7},
    {TETROMINO_8, 1, 1, 8},
    {TETROMINO_9, 1, 1, 9}};

// de explodes moet hier weg, in een andere functie zetten!!!
// de argumenten van deze functie algemener maken!!!
uint8_t piece_fits(struct tetromino_state tetromino_state, struct game_state *game_state, uint8_t width, uint8_t height) {

    struct tetromino *tetromino = game_state->tetromino_state.tetromino;

    for (short row = 0;
         row < tetromino->length;
         ++row) {
        for (short col = 0;
             col < tetromino->length;
             ++col) {
            uint8_t tetromino_value = tetromino_get(tetromino, row, col, tetromino_state.rotation);
            if (tetromino_value > 0) {
                short board_row = tetromino_state.row_in_field + row;
                short board_col = tetromino_state.column_in_field + col;
                uint8_t field_value;
                if (board_col >= 0 && board_col < FIELD_WIDTH) {
                    if (board_row >= 0 && board_row < FIELD_HEIGHT) {
                        if (board_get(game_state->board, FIELD_WIDTH, board_row, board_col) != NULL) {
                            field_value = board_get(game_state->board, FIELD_WIDTH, board_row, board_col)->value.color;
                        } else {
                            field_value = 0;
                        };
                        if (field_value) {
                            if (game_state->bomb.active && !game_state->bomb.handled) {
                                explode(game_state, game_state->tetromino_state.column_in_field, game_state->tetromino_state.row_in_field);
                            }
                            return 0;
                        }
                    } else if (game_state->bomb.active && !game_state->bomb.handled) {
                        explode(game_state, game_state->tetromino_state.column_in_field, game_state->tetromino_state.row_in_field); // - 1 to revert position
                        return 0;
                    } else {
                        return 0;
                    }
                } else {
                    return 0;
                }
            }
        }
    }
    return 1;
}

uint8_t tetromino_get(struct tetromino *tetromino, short row, short col, uint8_t rotation) {
    uint8_t length = tetromino->length;
    switch (rotation) {
    case 0:
        return tetromino->blocks[row * length + col];
    case 1:
        return tetromino->blocks[(length - col - 1) * length + row];
    case 2:
        return tetromino->blocks[(length - row - 1) * length + (length - col - 1)];
    case 3:
        return tetromino->blocks[col * length + (length - row - 1)];
    default: // to catch wrong input
        return tetromino->blocks[row * length + col];
    }
}

void install_tetromino_in_field(struct game_state *game) {
    if (!game->bomb.active) {
        struct saved_piece_data value = create_saved_piece(game->current_id, game->tetromino_state.tetromino->color, game->tetromino_state.tetromino->total_blocks);
        game->active_pieces = insert_before(value, game->active_pieces);
    }
    struct tetromino *tetromino = game->tetromino_state.tetromino;
    for (uint8_t row = 0;
         row < tetromino->length;
         ++row) {
        for (uint8_t col = 0;
             col < tetromino->length;
             ++col) {
            uint8_t value = tetromino_get(tetromino, row, col, game->tetromino_state.rotation);
            if (value) {
                short board_row = game->tetromino_state.row_in_field + row;
                short board_col = game->tetromino_state.column_in_field + col;
                double_node *mynode = game->active_pieces;
                uint8_t free_index = game->active_pieces->value.free_index; // in een aparte functie want komt 2 keer voor
                game->active_pieces->value.positions[free_index] = (uint8_t)board_col;
                game->active_pieces->value.positions[free_index + 1] = (uint8_t)board_row;
                game->active_pieces->value.free_index += 2;
                board_set(game->board, FIELD_WIDTH, board_row, board_col, mynode);
            }
        }
    }
}

void move_position(struct game_state *game_state, accelerometer_data accelerometer_data) {
    tetromino_state tetromino = game_state->tetromino_state; // dynamisch alloceren om geheugen te besparen!!! (vraag aan assistent)
    if (accelerometer_data.x > MIN_TILT) {
        tetromino.column_in_field -= 1;
        delay(100);
    } else if (accelerometer_data.x < -MIN_TILT) {
        tetromino.column_in_field += 1;
        delay(100);
    }
    if (accelerometer_data.y > 0.95) {
        game_state->speed.counter = game_state->speed.counter + 10;
    } else if (accelerometer_data.y < -0.50) {
        if (game_state->speed.counter > 0) {
            game_state->speed.counter--;
        }
    }
    if (M5.BtnA.isPressed() && M5.BtnB.isPressed()) {
        write_data(game_state);
    } else if (M5.BtnB.wasPressed()) {
        load_data(game_state);
    } else {
        if (M5.BtnA.isPressed()) {
            tetromino.rotation = (tetromino.rotation + 1) % 4;
        }
        if (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT)) {
            game_state->tetromino_state = tetromino;
        }
    }
}

void write_data(struct game_state *game_state) {

    uint address = 0;

    // vergeet niet current_id eerst 0 te maken bij het inlezen!!

    //game_mode
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
    EEPROM.writeByte(address, game_mode);
    Serial.printf("\ngame_mode sending\n: %u", game_mode);
    address++;

    //game over?
    EEPROM.writeByte(address, game_state->game_over);
    Serial.printf("game_over sending\n: %u", game_state->game_over);
    address++;

    // move_down?
    EEPROM.writeByte(address, game_state->move_down);
    Serial.printf("move_down sending\n: %u", game_state->move_down);
    address++;

    //bomb
    EEPROM.writeByte(address, game_state->bomb.active);
    Serial.printf("bomb.active sending\n: %u", game_state->bomb.active);
    address++;
    EEPROM.writeByte(address, game_state->bomb.handled);
    Serial.printf("bomb.handled sending\n: %u", game_state->bomb.handled);
    address++;

    //timed_block
    EEPROM.writeByte(address, game_state->timed_block.active);
    Serial.printf("timed_block.active sending\n: %u", game_state->timed_block.active);
    address++;
    EEPROM.writeByte(address, game_state->timed_block.index);
    Serial.printf("timed_block.index sending\n: %u", game_state->timed_block.index);

    address++;
    EEPROM.writeByte(address, game_state->timed_block.limit);
    Serial.printf("timed_block.limit sending\n: %u", game_state->timed_block.limit);

    address++;

    // speed
    EEPROM.writeByte(address, game_state->speed.counter);
    Serial.printf("speed.counter sending\n: %u", game_state->speed.counter);

    address++;
    EEPROM.writeByte(address, game_state->speed.limit);
    Serial.printf("speed.limit sending\n: %u", game_state->speed.counter);

    address++;

    // score
    EEPROM.writeUInt(address, game_state->score);
    Serial.printf("score sending\n: %u", game_state->score);

    address += sizeof(uint32_t);

    // board pieces
    struct double_node *board_pieces = game_state->active_pieces;
    uint8_t amount = count_elements(board_pieces);
    EEPROM.writeByte(address, amount);
    address++;
    Serial.printf("amount sending\n: %u", amount);
    while (board_pieces != NULL) {
        EEPROM.writeByte(address, board_pieces->value.color); // color 9?? => check index van mine, anders color - 1 is index
        Serial.printf("color sending\n: %u", board_pieces->value.color);
        address++;
        EEPROM.writeByte(address, board_pieces->value.blocks_left);
        Serial.printf("blocks_left sending\n: %u", board_pieces->value.blocks_left);
        address++;
        for (uint8_t i = 0; i < board_pieces->value.free_index; i++) {
            EEPROM.writeByte(address, board_pieces->value.positions[i]);
            Serial.printf("points sending\n: %u", board_pieces->value.positions[i]);
            address++;
        }
        board_pieces = board_pieces->next;
    }

    //current tetromino
    EEPROM.writeByte(address, game_state->tetromino_state.tetromino->color); // color 9?? => check index van mine, anders color - 1 is index
    address++;
    EEPROM.writeByte(address, (uint8_t)game_state->tetromino_state.column_in_field);
    address++;
    EEPROM.writeByte(address, (uint8_t)game_state->tetromino_state.row_in_field);
    address++;
    EEPROM.writeByte(address, game_state->tetromino_state.rotation);
    address++;

    EEPROM.commit();
}

void load_data(struct game_state *game_state) {

    uint address = 0;

    // vergeet niet current_id eerst 0 te maken bij het inlezen!!

    //game_mode
    switch (EEPROM.readByte(address)) {
    case 1:
        game_state->game_mode = mines_and_timed_blocks;
    case 2:
        game_state->game_mode = no_mines_no_timed_blocks;
    case 3:
        game_state->game_mode = mines_only;
    case 4:
        game_state->game_mode = timed_blocks_only;
    }
    Serial.printf("game_mode receiving: %u\n", game_state->game_mode);
    address++;

    //game over?
    game_state->game_over = EEPROM.readByte(address);
    Serial.printf("game_over receiving: %u\n", game_state->game_over);
    address++;

    //move_down?
    game_state->move_down = EEPROM.readByte(address);
    Serial.printf("move_down receiving: %u\n", game_state->move_down);
    address++;

    //bomb
    game_state->bomb.active = EEPROM.readByte(address);
    Serial.printf("bomb.activereceiving: %u\n", game_state->bomb.active);
    address++;
    game_state->bomb.handled = EEPROM.readByte(address);
    Serial.printf("bomb.handled receiving: %u\n", game_state->bomb.handled);
    address++;

    //timed_block
    game_state->timed_block.active = EEPROM.readByte(address);
    Serial.printf("timed_block.active receiving: %u\n", game_state->timed_block.active);
    address++;
    game_state->timed_block.index = EEPROM.readByte(address);
    Serial.printf("timed_block.index receiving: %u\n", game_state->timed_block.index);
    address++;
    game_state->timed_block.limit = EEPROM.readByte(address);
    Serial.printf("timed_block.limit receiving: %u\n", game_state->timed_block.limit);
    address++;

    // speed
    game_state->speed.counter = EEPROM.readByte(address);
    Serial.printf("speed.counter receiving: %u\n", game_state->speed.counter);
    address++;
    game_state->speed.limit = EEPROM.readByte(address);
    Serial.printf("speed.limit receiving: %u\n", game_state->speed.limit);
    address++;

    // score
    game_state->score = EEPROM.readUInt(address);
    Serial.printf("score receiving: %u\n", game_state->score);
    address += sizeof(uint32_t);

    // board pieces
    for (uint8_t row = 0; row < FIELD_HEIGHT; row++) {
        for (uint8_t column = 0; column < FIELD_WIDTH; column++) {
            if (game_state->board[row * FIELD_WIDTH + column] != NULL) {
                game_state->active_pieces = delete_node(game_state->board[row * FIELD_WIDTH + column]->value.id, game_state->active_pieces);
                game_state->board[row * FIELD_WIDTH + column] = NULL;
            }
        }
    }
    uint8_t counter = EEPROM.readByte(address); // de board size gewoon negeren voor nu, CHECK DIT LATER NA!
    address++;
    Serial.printf("amount receiving: %u\n", counter);
    game_state->active_pieces = NULL;
    game_state->current_id = 1;
    for (uint8_t i = counter; i > 0; i--) {
        uint8_t color = EEPROM.readByte(address);
        address++;
        uint8_t blocks_left = EEPROM.readByte(address);
        address++;
        struct saved_piece_data value = create_saved_piece(game_state->current_id, color, blocks_left);
        game_state->active_pieces = insert_before(value, game_state->active_pieces);
        game_state->current_id++;
        for (uint8_t i = 0; i < game_state->active_pieces->value.blocks_left; i++) {
            uint8_t column = EEPROM.readByte(address);
            Serial.printf("points receiving: %u\n", column);
            address++;
            uint8_t row = EEPROM.readByte(address);
            Serial.printf("points receiving: %u\n", row);
            address++;
            game_state->board[row * FIELD_WIDTH + column] = game_state->active_pieces;
            uint8_t free_index = game_state->active_pieces->value.free_index;
            game_state->active_pieces->value.positions[free_index] = column;
            game_state->active_pieces->value.positions[free_index + 1] = row; // beter aparte functie hiervoor maken???
            game_state->active_pieces->value.free_index += 2;
        }
    }

    //current tetromino
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
}

uint32_t black_color = M5.Lcd.color565(0, 0, 0);
uint32_t purple_color = M5.Lcd.color565(128, 0, 128);
uint32_t red_color = M5.Lcd.color565(255, 0, 0);
uint32_t green_color = M5.Lcd.color565(0, 128, 0);

uint32_t give_color(uint8_t n) {
    uint32_t color;
    switch (n) {
    case 0:
        color = black_color;
        break;
    case 1:
        color = TFT_BLUE;
        break;
    case 2:
        color = green_color;
        break;
    case 3:
        color = purple_color;
        break;
    case 4:
        color = TFT_CYAN;
        break;
    case 5:
        color = TFT_MAGENTA;
        break;
    case 6:
        color = TFT_ORANGE;
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
        color = black_color;
        break;
    }
    return color;
}

uint8_t possible_to_lower(struct game_state *game_state) {
    tetromino_state tetromino = game_state->tetromino_state;
    tetromino.row_in_field += 1;
    return (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT));
}

void bring_down(double_node **board, short x, short y) { // gebruik die ook in clear_line
    for (short line_y = y; line_y >= 0; line_y--) {
        board_set(board, FIELD_WIDTH, line_y, x, board_get(board, FIELD_WIDTH, line_y - 1, x));
        if (board_get(board, FIELD_WIDTH, line_y - 1, x) == NULL) {
            return;
        }
    }
    board[x] = NULL;
}

void explode(struct game_state *game_state, short bomb_x, short bomb_y) { // bomb-dingen in een aparte struct zettenen zo is pece_fits bv. ook veel generieker!!!
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
                game_state->bomb.handled = 1;
                if (!(board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x) == NULL)) {
                    board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.blocks_left--;
                    delete_position(&board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value, neighbour_x, neighbour_y);
                    if (board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.color == 9) { // voor zo'n dingen moet je de get_board gebruiken!!!
                        game_state->timed_block.active = 0;
                        free(board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x));
                    }
                    if (board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.blocks_left == 0) {
                        game_state->score += 50;
                        game_state->active_pieces = delete_node(board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.id, game_state->active_pieces); // board_set gebruiken en misschien moet dit niet eens hier staan??
                    }
                    bring_down(game_state->board, neighbour_x, neighbour_y);
                }
            }
        }
    }
}

#define MEM_SIZE 512

void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(MEM_SIZE);
    M5.Lcd.fillScreen(black_color);
    Serial.begin(115200);
    Serial.flush();
}

uint8_t random_int(uint8_t min, uint8_t max) {
    uint8_t range = max - min;
    return min + rand() % range;
}

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

void spawn_piece(struct game_state *game) {
    game->current_id++;
    uint8_t index = (uint8_t)random_int(0, 7); //(uint8_t)random_int(0, 7);
    game->tetromino_state.tetromino = &TETROMINOS[index];
    if ((game->current_id % 102) == 0) {                  // counter verhogen
        game->tetromino_state.tetromino = &TETROMINOS[7]; // ECHT random maken!
        game->bomb.active = 1;
    }
    if (((game->current_id % 100) == 0) && !game->bomb.active && !game->timed_block.active) { // counter verhogen
        tetromino *ptr = (tetromino *)malloc(sizeof(struct tetromino));
        ptr->blocks = game->tetromino_state.tetromino->blocks;
        ptr->length = game->tetromino_state.tetromino->length;
        ptr->total_blocks = game->tetromino_state.tetromino->total_blocks;
        ptr->color = 9; // constanten wegwerken!!
        game->tetromino_state.tetromino = ptr;
        game->timed_block.active = 1;
        game->timed_block.limit = 100;
        game->timed_block.index = index;
    }
    if (game->timed_block.active) {
        if (!(game->timed_block.limit)) {
            game->game_over = 1;
        }
        game->timed_block.limit--;
    }
    game->tetromino_state.row_in_field = 0;
    game->tetromino_state.column_in_field = FIELD_WIDTH / 2;
    game->tetromino_state.rotation = 0;
    game->speed.limit = 30 - (uint8_t)0.1 * game->score; //test het eens door prints of het OK is
}

uint8_t is_line_full(double_node **board, short y) {
    uint8_t line_full = 1;
    for (uint8_t block_x = 1; block_x < FIELD_WIDTH; block_x++) {
        if (board_get(board, FIELD_WIDTH, y, block_x) == NULL) {
            line_full = 0;
        }
    }
    return line_full;
}

// hier ook bring_down gebruiken!!!
void clear_line(game_state *game_state, short y) {
    for (uint8_t line_x = 0; line_x < FIELD_WIDTH; line_x++) {
        for (short line_y = y; line_y >= 0; line_y--) {
            if (line_y == y) {
                board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.blocks_left--;
                if (board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.color == 9) {
                    game_state->timed_block.active = 0;
                }
                if (board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.blocks_left == 0) {
                    game_state->score += 50;
                    game_state->active_pieces = delete_node(board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.id, game_state->active_pieces);
                    game_state->board[line_y * FIELD_WIDTH + line_x] = NULL;
                }
            }
            board_set(game_state->board, FIELD_WIDTH, line_y, line_x, board_get(game_state->board, FIELD_WIDTH, line_y - 1, line_x));
        }
        game_state->board[line_x] = NULL;
    }
}

void handle_full_lines(struct game_state *game_state) {
    for (uint8_t block_y = 0; block_y < 4; block_y++) {
        if (game_state->tetromino_state.row_in_field + block_y < FIELD_HEIGHT) {
            if (is_line_full(game_state->board, game_state->tetromino_state.row_in_field + block_y)) {
                clear_line(game_state, game_state->tetromino_state.row_in_field + block_y);
                game_state->speed.counter--;
                game_state->score += 150;
            }
        }
    }
}

#define ZERO_STRUCT(obj) memset(&(obj), 0, sizeof(obj))

void loop() {
    srand(time(NULL));
    M5.update(); // moet er gewoon standard staan
    delay(20);
    struct accelerometer_data accelerometer_data;

    struct game_state game_state;
    game_state.game_mode = mines_and_timed_blocks;

    game_state.current_id = 0;
    game_state.timed_block.active = 0;

    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            game_state.board[y * FIELD_WIDTH + x] = NULL;
        }
    }

    game_state.active_pieces = NULL;
    spawn_piece(&game_state);

    while (!game_state.game_over) {
        M5.update(); // moet er gewoon standard staan
        delay(10);
        M5.Imu.getAccelData(&accelerometer_data.x, &accelerometer_data.y, &accelerometer_data.z);
        move_position(&game_state, accelerometer_data);
        game_state.speed.counter++;
        if (game_state.speed.counter > game_state.speed.limit) {
            game_state.move_down = 1;
        }

        if (game_state.move_down) {
            game_state.move_down = 0;
            game_state.speed.counter = 0;
            if (game_state.bomb.active) {
                if (game_state.bomb.handled) {
                    game_state.bomb.active = 0;
                    spawn_piece(&game_state);
                    game_state.bomb.handled = 0;
                } else if (possible_to_lower(&game_state)) {
                    game_state.tetromino_state.row_in_field++;
                } else {
                    explode(&game_state, game_state.tetromino_state.column_in_field, game_state.tetromino_state.row_in_field); // - 1 to revert position
                }
            } else if (possible_to_lower(&game_state)) {
                game_state.tetromino_state.row_in_field++;
            } else {
                if (!piece_fits(game_state.tetromino_state, &game_state, FIELD_WIDTH, FIELD_HEIGHT)) {
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
                if (game_state.board[y * FIELD_WIDTH + x] != NULL) {
                    M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(game_state.board[y * FIELD_WIDTH + x]->value.color));
                } else
                    M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(0));
            }

        // Draw Current Piece
        struct tetromino *tetromino = game_state.tetromino_state.tetromino;
        for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
            for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
                uint8_t value = tetromino_get(tetromino, block_y, block_x, game_state.tetromino_state.rotation);
                if (value) // binnen 1 piece geen 2D array, je houdt wel een 2D array bij van pieces
                {
                    M5.Lcd.fillRect((game_state.tetromino_state.column_in_field + block_x) * 8, (game_state.tetromino_state.row_in_field + block_y) * 8, 8, 8, give_color(tetromino->color));
                }
            }
    }
}
