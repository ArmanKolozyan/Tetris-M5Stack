#include "EEPROM.h"
#include <M5StickC.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

enum Game_mode {
    no_mines_no_timed_blocks,
    mines_only,
    timed_blocks_only,
    mines_and_blocks
};

struct saved_piece_data {
    int id;
    uint8_t color;
    uint8_t blocks_left;
};

typedef struct double_node {
    struct saved_piece_data value;
    struct double_node *prev;
    struct double_node *next;
} Double_Node;

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

Double_Node *delete_node(int id, Double_Node *start) {
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
        free(p);
    }
    return start;
}

#define DISPLAY_HEIGHT 160
#define DISPLAY_WIDTH 80

#define FIELD_HEIGHT 20
#define FIELD_WIDTH 10

#define MIN_TILT 0.15

struct timed_block {
    uint8_t active;
    uint8_t limit;
};

struct accelerometer_data {
};

float acc_x = 0,
      acc_y = 0, acc_z = 0;

struct double_node *board_get(struct double_node **values, uint8_t width, int row, int col) { // betere naam geven want niet altijd tetromino
    int index = row * width + col;
    return values[index];
}

void board_set(struct double_node **values, uint8_t width, int row, int col, struct double_node *parent) {
    int index = row * width + col;
    values[index] = parent;
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

struct tetromino {
    uint8_t *data;
    uint8_t length;
    uint8_t total_blocks;
};

struct tetromino TETROMINOS[] = {
    {TETROMINO_1, 4, 4}, // misschien de lengte met een functie berekenen?
    {TETROMINO_2, 2, 4},
    {TETROMINO_3, 3, 4},
    {TETROMINO_4, 3, 4},
    {TETROMINO_5, 3, 4},
    {TETROMINO_6, 3, 4},
    {TETROMINO_7, 3, 4},
    {TETROMINO_8, 1, 1},
    {TETROMINO_9, 1, 1}};

struct tetromino_state {
    uint8_t id = 0;
    uint8_t tetromino_index = 0;
    int offset_row = 0;
    int offset_col = 0;
    uint8_t rotation = 0;
};

struct game_state {
    struct double_node *board[FIELD_WIDTH * FIELD_HEIGHT];
    uint8_t lines[FIELD_HEIGHT];

    uint8_t SPEED = 30;
    uint8_t speed_counter = 0;
    uint8_t move_down = 0;
    uint8_t current_id = 0;
    uint8_t bomb_active = 0;
    uint8_t bomb_handled = 0;

    struct tetromino_state tetromino_state;

    struct timed_block timed_block;

    struct double_node *active_pieces;

    int score;
    uint8_t game_over;
};

// de explodes moet hier weg, in een andere functie zetten!!!
// de argumenten van deze functie algemener maken!!!
uint8_t piece_fits(struct tetromino_state tetromino_state, struct game_state *game_state, uint8_t width, uint8_t height) {

    struct tetromino *tetromino = TETROMINOS + game_state->tetromino_state.tetromino_index;

    for (int row = 0;
         row < tetromino->length;
         ++row) {
        for (int col = 0;
             col < tetromino->length;
             ++col) {
            uint8_t tetromino_value = tetromino_get(tetromino, row, col, tetromino_state.rotation);
            if (tetromino_value > 0) {
                int board_row = tetromino_state.offset_row + row;
                int board_col = tetromino_state.offset_col + col;
                uint8_t field_value;
                if (board_col >= 0 && board_col < FIELD_WIDTH) {
                    if (board_row >= 0 && board_row < FIELD_HEIGHT) {
                        if (board_get(game_state->board, FIELD_WIDTH, board_row, board_col) != NULL) {
                            field_value = board_get(game_state->board, FIELD_WIDTH, board_row, board_col)->value.color;
                        } else {
                            field_value = 0;
                        };
                        if (field_value) {
                            if (game_state->bomb_active && !game_state->bomb_handled) {
                                explode(game_state, game_state->tetromino_state.offset_col, game_state->tetromino_state.offset_row);
                            }
                            return 0;
                        }
                    } else if (game_state->bomb_active && !game_state->bomb_handled) {
                        explode(game_state, game_state->tetromino_state.offset_col, game_state->tetromino_state.offset_row); // - 1 to revert position
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

uint8_t tetromino_get(struct tetromino *tetromino, int row, int col, uint8_t rotation) {
    uint8_t length = tetromino->length;
    switch (rotation) {
    case 0:
        return tetromino->data[row * length + col];
    case 1:
        return tetromino->data[(length - col - 1) * length + row];
    case 2:
        return tetromino->data[(length - row - 1) * length + (length - col - 1)];
    case 3:
        return tetromino->data[col * length + (length - row - 1)];
    default: // to catch wrong input
        return tetromino->data[row * length + col];
    }
}

void install_tetromino_in_field(struct game_state *game) {
    struct tetromino *tetromino = TETROMINOS + game->tetromino_state.tetromino_index;
    for (uint8_t row = 0;
         row < tetromino->length;
         ++row) {
        for (uint8_t col = 0;
             col < tetromino->length;
             ++col) {
            uint8_t value = tetromino_get(tetromino, row, col, game->tetromino_state.rotation);
            if (value) {
                int board_row = game->tetromino_state.offset_row + row;
                int board_col = game->tetromino_state.offset_col + col;
                double_node *mynode = game->active_pieces;
                board_set(game->board, FIELD_WIDTH, board_row, board_col, mynode);
            }
        }
    }
}

void move_position(struct game_state *game_state, float acc_x, float acc_y) {
    tetromino_state tetromino = game_state->tetromino_state;
    if (acc_x > MIN_TILT) {
        tetromino.offset_col -= 1;
        delay(100);
    } else if (acc_x < -MIN_TILT) {
        tetromino.offset_col += 1;
        delay(100);
    }
    if (acc_y > 0.95) {
        game_state->speed_counter = game_state->speed_counter + 10;
    } else if (acc_y < -0.50) {
        if (game_state->speed_counter > 0) {
            game_state->speed_counter--;
        }
    }

    if (M5.BtnA.wasPressed()) {
        tetromino.rotation = (tetromino.rotation + 1) % 4;
    }

    if (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT)) {
        game_state->tetromino_state = tetromino;
    }
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
    tetromino.offset_row += 1;
    return (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT));
}

void bring_down(double_node **board, int x, int y) { // gebruik die ook in clear_line
    for (int line_y = y; line_y >= 0; line_y--) {
        board_set(board, FIELD_WIDTH, line_y, x, board_get(board, FIELD_WIDTH, line_y - 1, x));
        if (board_get(board, FIELD_WIDTH, line_y - 1, x) == NULL) {
            return;
        }
    }
    board[x] = NULL;
}

void explode(struct game_state *game_state, int bomb_x, int bomb_y) { // bomb-dingen in een aparte struct zettenen zo is pece_fits bv. ook veel generieker!!!
    for (int off_y = -1; off_y <= 1; off_y++) {
        for (int off_x = -1; off_x <= 1; off_x++) { // checking all the neighbours of the bomb
            int neighbour_y = bomb_y + off_y;
            int neighbour_x = bomb_x + off_x;
            if ((neighbour_y < 0) || (neighbour_y >= FIELD_HEIGHT)) { // checking if we are not off the field
                continue;
            }
            if ((neighbour_x < 0) || (neighbour_x >= FIELD_WIDTH)) { // checking if we are not off the field
                continue;
            }
            {
                game_state->bomb_handled = 1;
                if (!(board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x) == NULL)) {
                    board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.blocks_left--;
                    if (board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.color == 9) { // voor zo'n dingen moet je de get_board gebruiken!!!
                        game_state->timed_block.active = 0;
                    }
                    if (board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.blocks_left == 0) {
                        game_state->score += 50;
                        delete_node(board_get(game_state->board, FIELD_WIDTH, neighbour_y, neighbour_x)->value.id, game_state->active_pieces); //board_set gebruiken en misschien moet dit niet eens hier staan??
                    }
                    bring_down(game_state->board, neighbour_x, neighbour_y);
                }
            }
        }
    }
}

void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(512);
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
    game->tetromino_state.tetromino_index = (uint8_t)random_int(0, 7);
    if ((game->current_id % 4) == 0) {             // counter verhogen
        game->tetromino_state.tetromino_index = 7; // ECHT random maken!
        game->bomb_active = 1;
    }
    if (((game->current_id % 5) == 0) && !game->bomb_active && !game->timed_block.active) { // counter verhogen
        game->tetromino_state.tetromino_index = 8;                                          // CONSTANTEN WEGWERKEN!
        game->timed_block.active = 1;
        game->timed_block.limit = 100;
    }
    if (game->timed_block.active) {
        if (!(game->timed_block.limit)) {
            game->game_over = 1;
        }
        game->timed_block.limit--;
    }
    game->tetromino_state.offset_row = 0;
    game->tetromino_state.offset_col = FIELD_WIDTH / 2;
    game->tetromino_state.rotation = 0;
    game->SPEED = 30;

    if (!game->bomb_active) {
        struct saved_piece_data value = {.id = game->current_id, .color = game->tetromino_state.tetromino_index + 1, .blocks_left = TETROMINOS[game->tetromino_state.tetromino_index].total_blocks};
        game->active_pieces = insert_before(value, game->active_pieces);
    }
}

uint8_t is_line_full(double_node **board, int y) {
    uint8_t line_full = 1;
    for (int block_x = 1; block_x < FIELD_WIDTH; block_x++) {
        if (board_get(board, FIELD_WIDTH, y, block_x) == NULL) {
            line_full = 0;
        }
    }
    return line_full;
}

// hier ook bring_down gebruiken!!!
void clear_line(game_state *game_state, int y) {
    for (uint8_t line_x = 0; line_x < FIELD_WIDTH; line_x++) {
        for (int line_y = y; line_y >= 0; line_y--) {
            if (line_y == y) {
                board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.blocks_left--;
                if (board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.color == 8) {
                    game_state->timed_block.active = 0;
                }
                if (board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.blocks_left == 0) {
                    game_state->score += 50;
                    delete_node(board_get(game_state->board, FIELD_WIDTH, line_y, line_x)->value.id, game_state->active_pieces);
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
        if (game_state->tetromino_state.offset_row + block_y < FIELD_HEIGHT) {
            if (is_line_full(game_state->board, game_state->tetromino_state.offset_row + block_y)) {
                clear_line(game_state, game_state->tetromino_state.offset_row + block_y);
                game_state->SPEED--;
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

    struct game_state game_state;

    game_state.current_id = 0;
    game_state.tetromino_state.id = 0;

    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            game_state.board[y * FIELD_WIDTH + x] = NULL;
        }
    }

    game_state.current_id++;
    game_state.tetromino_state.tetromino_index = (uint8_t)random_int(0, 7);
    game_state.tetromino_state.offset_row = 0;
    game_state.tetromino_state.offset_col = FIELD_WIDTH / 2;
    game_state.tetromino_state.rotation = 0;
    game_state.SPEED = 30;
    struct saved_piece_data first_value = {.id = 1, .color = game_state.tetromino_state.tetromino_index + 1, .blocks_left = TETROMINOS[game_state.tetromino_state.tetromino_index].total_blocks};
    struct double_node *first_node = insert_before(first_value, NULL);
    game_state.active_pieces = first_node;

    while (!game_state.game_over) {
        M5.update(); // moet er gewoon standard staan
        delay(10);
        M5.Imu.getAccelData(&acc_x, &acc_y, &acc_z);
        move_position(&game_state, acc_x, acc_y);
        game_state.speed_counter++;
        if (game_state.speed_counter > game_state.SPEED) {
            game_state.move_down = 1;
        }

        if (game_state.move_down) {
            game_state.move_down = 0;
            game_state.speed_counter = 0;
            if (game_state.bomb_active) {
                if (game_state.bomb_handled) {
                    game_state.bomb_active = 0;
                    spawn_piece(&game_state);
                    game_state.bomb_handled = 0;
                } else if (possible_to_lower(&game_state)) {
                    game_state.tetromino_state.offset_row++;
                } else {
                    explode(&game_state, game_state.tetromino_state.offset_col, game_state.tetromino_state.offset_row); // - 1 to revert position
                }
            } else if (possible_to_lower(&game_state)) {
                game_state.tetromino_state.offset_row++;
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
        struct tetromino *tetromino = TETROMINOS + game_state.tetromino_state.tetromino_index;
        for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
            for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
                uint8_t value = tetromino_get(tetromino, block_y, block_x, game_state.tetromino_state.rotation);
                if (value) // binnen 1 piece geen 2D array, je houdt wel een 2D array bij van pieces
                {
                    M5.Lcd.fillRect((game_state.tetromino_state.offset_col + block_x) * 8, (game_state.tetromino_state.offset_row + block_y) * 8, 8, 8, give_color(value));
                }
            }
    }
}

/*
void explode() {

    for (int off_y = -1; off_y <= 1; off_y++) {
        for (int off_x = -1; off_x <= 1; off_x++) { // checking all the neighbours of the bomb
            int neighbour_y = current_y + off_y;
            int neighbour_x = current_x + off_x;
            if ((neighbour_y < 0) || (neighbour_y >= FIELD_HEIGHT)) { // checking if we are not off the field
                continue;
            }
            if ((neighbour_x < 0) || (neighbour_x >= FIELD_WIDTH)) { // checking if we are not off the field
                continue;
            }
            {
                struct tetromino neighbour = field[neighbour_y * FIELD_WIDTH + neighbour_x];
                bomb_handled = true;
                bring_down(neighbour_x, neighbour_y);
            }
        }
    }
}
*/

/*
    delay(20);
    SPEED = 30;
    score = 0;
    game_over = 0;

    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            field[y * FIELD_WIDTH + x] = 0;
        }
    }

    while (!game_over) {
        M5.update(); // moet er gewoon standard staan
        delay(10);
        move_down = 0;

        M5.Imu.getAccelData(&acc_x, &acc_y, &acc_z);
        move_position(&current_x, &current_y, acc_x, acc_y, current_piece, &current_rotation);
        speed_counter++;
        if (SPEED <= speed_counter) {
            move_down = 1;
        }

        if (move_down) {
            speed_counter = 0;
            if (bomb_active) {
                boolean does_piece_fit = piece_fits(current_piece, current_rotation, current_x, current_y + 1);

                if (bomb_handled) {
                    SPEED = 30;
                    score += 12;
                    current_piece = rand() % 7; // weer random maken
                    current_rotation = 0;       // deze vier beter in een struct game_state
                    current_x = FIELD_WIDTH / 2;
                    current_y = 0;
                    bomb_handled = false;
                    bomb_active = false;
                } else if (piece_fits(current_piece, current_rotation, current_x, current_y + 1)) {
                    current_y++;
                }
            } else if (piece_fits(current_piece, current_rotation, current_x, current_y + 1)) {
                current_y++;
            } else {

                if (!piece_fits(current_piece, current_rotation, current_x, current_y)) {
                    game_over = 1;
                    break;
                }

                install_tetromino_in_field(current_piece, current_rotation);
                handle_full_lines(current_y);

                SPEED = 30;
                score += 12;
                current_piece = rand() % 7;     // weer random maken
                if ((total_counter % 2) == 0) { // counter verhogen
                    current_piece = 7;
                    bomb_active = true;
                }
                total_counter++;
                current_rotation = 0; // deze vier beter in een struct game_state
                current_x = FIELD_WIDTH / 2;
                current_y = 0;
            }
        }

        // Draw Field
        for (uint8_t x = 0; x < FIELD_WIDTH; x++)
            for (uint8_t y = 0; y < FIELD_HEIGHT; y++)
                M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(field[y * FIELD_WIDTH + x].color));

        // Draw Current Piece
        for (uint8_t block_x = 0; block_x < 4; block_x++)
            for (uint8_t block_y = 0; block_y < 4; block_y++)
                if (tetrominos[current_piece][rotate(block_x, block_y, current_rotation)] != 0) // binnen 1 piece geen 2D array, je houdt wel een 2D array bij van pieces
                {
                    M5.Lcd.fillRect((current_x + block_x) * 8, (current_y + block_y) * 8, 8, 8, give_color(current_piece + 1));
                }
    }

    M5.Lcd.fillScreen(TFT_GREEN);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("YOUR SCORE WAS: %i", score);
    delay(1000);

    while (true) {
        M5.update();
        if (M5.BtnA.isPressed()) {
            M5.Lcd.fillScreen(black_color);
            break;
        }
    }
    */

/*
boolean is_line_full(int current_y, uint8_t block_y) {
    uint8_t line_full = 1;
    for (int block_x = 1; block_x < FIELD_WIDTH; block_x++) {
        if (!field[(current_y + block_y) * FIELD_WIDTH + block_x].id) {
            line_full = 0;
        }
    }
    return line_full;
}

void clear_line(int current_y, uint8_t block_y) {
    for (uint8_t block_x = 0; block_x < FIELD_WIDTH; block_x++) {
        for (int line_y = current_y + block_y; line_y >= 0; line_y--) {
            field[line_y * FIELD_WIDTH + block_x] = field[(line_y - 1) * FIELD_WIDTH + block_x];
        }
        field[block_x] = empty_tetromino;
    }
}
*/

/*
void install_tetromino_in_field(uint8_t current_piece, uint8_t current_rotation) {
    uint8_t id = generate_id();
    for (int block_x = 0; block_x < 4; block_x++)
        for (int block_y = 0; block_y < 4; block_y++)
            if (tetrominos[current_piece][rotate(block_x, block_y, current_rotation)] != 0) {
                struct tetromino current_tetromino = {id, tetrominos[current_piece][rotate(block_x, block_y, current_rotation)]};
                field[(current_y + block_y) * FIELD_WIDTH + (current_x + block_x)] = current_tetromino;
            }
}
*/

/*
void handle_full_lines(int current_y) {
    for (uint8_t block_y = 0; block_y < 4; block_y++) {
        if (current_y + block_y < FIELD_HEIGHT) {
            if (is_line_full(current_y, block_y)) {
                clear_line(current_y, block_y);
                SPEED--;
                score += 150;
            }
        }
    }
}

void bring_down(int x, int y) { // gebruik die ook in clear_line
    for (int line_y = y; line_y >= 0; line_y--) {
        field[line_y * FIELD_WIDTH + x] = field[(line_y - 1) * FIELD_WIDTH + x];
    }
    field[x] = empty_tetromino;
}
*/

/*
// 0 = > 0 degrees 1 = > 90 degrees 2 = > 180 degrees 3 = > 270 degrees
uint8_t rotate(uint8_t x, uint8_t y, uint8_t rotation) // returns: which index in the rotated one relative to the original
{
    uint8_t rotated_index = 0;
    switch (rotation % 4)
    {
    case 0:
        rotated_index = y * 4 + x;
        break;
        // 0  1  2  3
        // 4  5  6  7
        // 8  9 10 11
        // 12 13 14 15

    case 1:
        rotated_index = 12 + y - (x * 4);
        break;
        // 12  8  4  0
        //  13  9  5  1
        //  14 10  6  2
        //  15 11  7  3

    case 2:
        rotated_index = 15 - (y * 4) - x;
        break;
        // 15 14 13 12
        //  11 10  9  8
        //  7  6  5  4
        //  3  2  1  0

    case 3:
        rotated_index = 3 - y + (x * 4);
        break;
        // 3  7 11 15
        // 2  6 10 14
        // 1  5  9 13
        // 0  4  8 12
    }
    return rotated_index;
}
*/

/*
uint8_t piece_fits(uint8_t tetromino, uint8_t rotation, int tetromino_x, int tetromino_y)
{
    for (uint8_t block_x = 0; block_x < (4 - 3 * bomb_active); block_x++)
        for (uint8_t block_y = 0; block_y < (4 - 3 * bomb_active); block_y++)
        {
            uint8_t rotated_piece_index = rotate(block_x, block_y, rotation);
            uint8_t field_index = (tetromino_y + block_y) * FIELD_WIDTH + (tetromino_x + block_x);

            uint8_t current_block = tetrominos[tetromino][rotated_piece_index];
            if (tetromino_x + block_x >= 0 && tetromino_x + block_x < FIELD_WIDTH)
            {
                if (tetromino_y + block_y >= 0 && tetromino_y + block_y < FIELD_HEIGHT)
                {
                    if (current_block != 0 && field[field_index].id)
                    {
                        if (bomb_active)
                            explode();
                        return 0;
                    }
                }
                else if (current_block != 0)
                {
                    if (bomb_active)
                        explode();
                    return 0;
                }
            }
            else if (current_block != 0)
            {
                return 0;
            }
        }

    return 1;
}


*/

/*
  for (int i = 0; i<16; i++) {
   if (figure[i]) {
   M5.Lcd.fillRect(20 + (i % 4)*8, 20 + (i / 4)*8, 8, 8, black_color);
   }
  }
    delay(20);

  for (int i = 0; i<16; i++) {
    if (figure[i])
    M5.Lcd.fillRect(20 + (i % 4)*8, 20 + (i / 4)*8, 8, 8, red_color);
  }
  */

/*

#define DOT_SIZE 6
int dot[6][6][2] {
  {{35,35}},
  {{15,15},{55,55}},
  {{15,15},{35,35},{55,55}},
  {{15,15},{15,55},{55,15},{55,55}},
  {{15,15},{15,55},{35,35},{55,15},{55,55}},
  {{15,15},{15,35},{15,55},{55,15},{55,35},{55,55}},
  };

float accX = 0;
float accY = 0;
float accZ = 0;


void setup(void) {
  M5.begin();
  M5.IMU.Init();

  M5.Lcd.setRotation(1);

  M5.Lcd.fillScreen(TFT_GREEN);

  M5.Lcd.setTextColor(TFT_BLACK);  // Adding a background colour erases previous text automatically


  M5.Lcd.setCursor(10, 30);
  M5.Lcd.setTextSize(3);
  M5.Lcd.print("SHAKE ME");
  delay(1000);
}

void loop() {


  while(1) {
    M5.IMU.getAccelData(&accX,&accY,&accZ);
    if (accX > 1.5 ||  accY > 1.5 ) {
      break;
    }
  }

  M5.Lcd.fillScreen(TFT_GREEN);

  // Draw first dice
  delay(500);  // A little delay to increase suspense :-D
  int number = random(0, 6);
  draw_dice(5,5,number);

  // Draw second dice
  delay(500);
  number = random(0, 6);
  draw_dice(85,5,number);


}

void draw_dice(int16_t x, int16_t y, int n) {

  M5.Lcd.fillRect(x, y, 70, 70, WHITE);

  for(int d = 0; d < 6; d++) {
    if (dot[n][d][0] > 0) {
        M5.Lcd.fillCircle(x+dot[n][d][0], y+dot[n][d][1], DOT_SIZE, TFT_BLACK);
    }
  }

}




*/

/*

// oefening 4



uint32_t black_color = M5.Lcd.color565(0, 0, 0);
uint32_t red_color = M5.Lcd.color565(255, 0, 0);

void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(512);
    M5.Lcd.fillScreen(black_color);
}

#define FIELD_HEIGHT 160
#define FIELD_WIDTH 80

#define RECT_WIDTH 50
#define RECT_HEIGHT 30

// oefening 3

uint8_t x = 0, y = 0;

#define MIN_TILT 0.90
#define SPEED 3

// andere mogelijkheid zou zijn om steeds te vermenigvuldigen met een getal afhankelijk van acc_x enz.
// maar dan zou de controller te gevoelig zijn
void move_position(int acc_x, int acc_y) {
    if (acc_x > MIN_TILT) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        x -= SPEED;
    } else if (acc_x < -MIN_TILT) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        x += SPEED;
    } else if (acc_y > MIN_TILT) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        y += SPEED;
    } else if (acc_y < -MIN_TILT) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        y -= SPEED;
    }
    if (x >= FIELD_WIDTH || x < SPEED) { // kleiner dan SPEED en niet 0 want uint8_t kan nooit 0 zijn en zo is je rechthoek niet deels erbuiten want je reset op tijd
        x = 0;
    } else if (y >= FIELD_HEIGHT || y < SPEED) {
        y = 0;
    }
}

void loop() {

    M5.update(); // moet er gewoon standard staan
    delay(20);

    float acc_x = 0, acc_y = 0, acc_z = 0;
    M5.Imu.getAccelData(&acc_x, &acc_y, &acc_z);
    move_position(acc_x, acc_y);
    M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, red_color);
}

*/

/*

// oefening 1

uint8_t y = 0; //kon ook Int zijn, maar we moeten geheugen beperken!!

void loop() {
  M5.update(); //moet er gewoon standard staan
  delay(20);
  M5.Lcd.fillRect(20, y, RECT_WIDTH, RECT_HEIGHT, black_color); // enkel het gebied waar er iets staat anders flickering!!
  y++;
  M5.Lcd.fillRect(20, y, RECT_WIDTH, RECT_HEIGHT, red_color);
  if (y >= FIELD_HEIGHT) {
    y = 0;
  }
  delay(20);
}

*/

// oefening 2

/*
uint8_t x = 0, y = 0;

void loop() {

    M5.update(); // moet er gewoon standard staan
    delay(20);
    if (M5.BtnA.wasPressed() && M5.BtnB.wasPressed()) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        x = 0;
        y = 0;
    } else if (M5.BtnA.wasPressed()) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);  // enkel tekenen als het nodig is anders heel veek flickering!
        x += 20;
    } else if (M5.BtnB.wasPressed()) {
        M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, black_color);
        y += 20;
    }
    M5.Lcd.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, red_color);
}
*/