#include "EEPROM.h"
#include <M5StickC.h>
#include <stdint.h>
#include <stdlib.h>

#define DISPLAY_HEIGHT 160
#define DISPLAY_WIDTH 80

#define FIELD_HEIGHT 20
#define FIELD_WIDTH 10

uint8_t field[FIELD_WIDTH * FIELD_HEIGHT];

// 0 = > 0 degrees 1 = > 90 degrees 2 = > 180 degrees 3 = > 270 degrees
uint8_t rotate(uint8_t x, uint8_t y, uint8_t rotation) // returns: which index in the rotated one relative to the original
{
    uint8_t rotated_index = 0;
    switch (rotation % 4) {
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

uint8_t tetrominos[7][16] =
    {
        {
            0,
            0,
            1,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            1,
            0,
        },
        {
            0,
            0,
            2,
            0,
            0,
            2,
            2,
            0,
            0,
            2,
            0,
            0,
            0,
            0,
            0,
            0,
        },
        {
            0,
            3,
            0,
            0,
            0,
            3,
            3,
            0,
            0,
            0,
            3,
            0,
            0,
            0,
            0,
            0,
        },
        {
            0,
            0,
            0,
            0,
            0,
            4,
            4,
            0,
            0,
            4,
            4,
            0,
            0,
            0,
            0,
            0,
        },
        {
            0,
            0,
            5,
            0,
            0,
            5,
            5,
            0,
            0,
            0,
            5,
            0,
            0,
            0,
            0,
            0,
        },
        {
            0,
            6,
            0,
            0,
            0,
            6,
            0,
            0,
            0,
            6,
            6,
            0,
            0,
            0,
            0,
            0,
        },
        {
            0,
            0,
            7,
            0,
            0,
            0,
            7,
            0,
            0,
            7,
            7,
            0,
            0,
            0,
            0,
            0,
        }};

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
        color = red_color;
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
        color = TFT_DARKGREY;
        break;
    case 6:
        color = TFT_ORANGE;
        break;
    case 7:
        color = TFT_YELLOW;
        break;
    }
    return color;
}

uint8_t piece_fits(uint8_t tetromino, uint8_t rotation, int tetromino_x, int tetromino_y) {
    for (uint8_t block_x = 0; block_x < 4; block_x++)
        for (uint8_t block_y = 0; block_y < 4; block_y++) {
            uint8_t rotated_piece_index = rotate(block_x, block_y, rotation);
            uint8_t field_index = (tetromino_y + block_y) * FIELD_WIDTH + (tetromino_x + block_x);

            uint8_t current_block = tetrominos[tetromino][rotated_piece_index];
            if (tetromino_x + block_x >= 0 && tetromino_x + block_x < FIELD_WIDTH) {
                if (tetromino_y + block_y >= 0 && tetromino_y + block_y < FIELD_HEIGHT) {
                    if (current_block != 0 && field[field_index] != 0)
                        return 0;
                } else if (current_block != 0) {
                    return 0;
                }
            } else if (current_block != 0) {
                return 0;
            }
        }

    return 1;
}

void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(512);
    M5.Lcd.fillScreen(black_color);
    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            field[y * FIELD_WIDTH + x] = 0;
        }
    }
    Serial.begin(115200);
    Serial.flush();
}

#define MIN_TILT 0.15

uint8_t SPEED = 10;
uint8_t speed_counter = 0;
uint8_t move_down;

void move_position(int *x, int *y, float acc_x, float acc_y, uint8_t current_piece, uint8_t *current_rotation) {
    if (acc_x > MIN_TILT) {
        if (piece_fits(current_piece, *current_rotation, *x - 1, *y)) {
            *x -= 1;
            delay(50);
        }
    } else if (acc_x < -MIN_TILT) {
        if (piece_fits(current_piece, *current_rotation, *x + 1, *y)) {
            *x += 1;
            delay(50);
        }
    }

    if (acc_y > 0.25) {
        if (SPEED > 0) {
            SPEED--;
            delay(10);
        }
    } else if (acc_y < -0.25) {
        // SPEED++;
        //  delay(50);
    }

    if (M5.BtnA.wasPressed()) {
        if (piece_fits(current_piece, *current_rotation + 1, *x, *y)) {
            (*current_rotation)++;
        }
    }
}

uint8_t current_piece = 2;
uint8_t current_rotation = 0; // deze vier beter in een struct game_state
int current_x = FIELD_WIDTH / 2;
int current_y = 0;
float acc_x = 0, acc_y = 0, acc_z = 0;
uint8_t game_over = false;

int score = 0;

void loop() {
    srand(time(NULL));
    M5.update(); // moet er gewoon standard staan
    delay(20);
    SPEED = 30;
    score = 0;
    current_piece = rand() % 7;
    current_rotation = 0; // deze vier beter in een struct game_state
    current_x = FIELD_WIDTH / 2;
    current_y = 0;
    game_over = 0;

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
            if (piece_fits(current_piece, current_rotation, current_x, current_y + 1)) {
                current_y++;
            } else {

                if (!piece_fits(current_piece, current_rotation, current_x, current_y)) {
                    game_over = 1;
                    break;
                }
                for (uint8_t block_x = 0; block_x < 4; block_x++)
                    for (uint8_t block_y = 0; block_y < 4; block_y++)
                        if (tetrominos[current_piece][rotate(block_x, block_y, current_rotation)] != 0)
                            field[(current_y + block_y) * FIELD_WIDTH + (current_x + block_x)] = tetrominos[current_piece][rotate(block_x, block_y, current_rotation)];
                for (uint8_t block_y = 0; block_y < 4; block_y++) {
                    if (current_y + block_y < FIELD_HEIGHT - 1) {
                        uint8_t bLine = 1;
                        for (uint8_t block_x = 1; block_x < FIELD_WIDTH; block_x++) {
                            if (!field[(current_y + block_y) * FIELD_WIDTH + block_x]) {
                                bLine = 0;
                            }
                        }
                        if (bLine) {
                            for (uint8_t block_x = 0; block_x < FIELD_WIDTH; block_x++) {
                                for (uint8_t line_y = current_y + block_y; line_y >= 0; line_y--)
                                    field[line_y * FIELD_WIDTH + block_x] = field[(line_y - 1) * FIELD_WIDTH + block_x];
                                field[block_x] = 0;
                            }
                            SPEED--;
                            score += 150;
                        }
                    }
                }
                SPEED = 30;
                score += 12;
                current_piece = rand() % 7;
                current_rotation = 0; // deze vier beter in een struct game_state
                current_x = FIELD_WIDTH / 2;
                current_y = 0;
            }
        }

        // Draw Field
        for (uint8_t x = 0; x < FIELD_WIDTH; x++)
            for (uint8_t y = 0; y < FIELD_HEIGHT; y++)
                M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(field[y * FIELD_WIDTH + x]));

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
            M5.Lcd.fillScreen(TFT_BLACK);
            break;
        }
    }
}

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