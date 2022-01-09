#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "constants.h"


void draw_menu(struct game_state *game_state) {
    M5.update();
    int clicks = 0;

    M5.Lcd.setTextColor(TFT_DARKCYAN);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(20, 3);
    M5.Lcd.printf("Choose"); // Pressing A = making choice, Pressing B = switching choice
    M5.Lcd.setCursor(20, 13);
    M5.Lcd.printf("Mode");
    M5.Lcd.setCursor(20, 30);
    M5.Lcd.printf("Mines &");
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.printf("timed");
    M5.Lcd.setCursor(20, 60);
    M5.Lcd.printf("Mines");
    M5.Lcd.setCursor(20, 90);
    M5.Lcd.printf("Timed");
    M5.Lcd.setCursor(20, 120);
    M5.Lcd.printf("Normal");

    struct tetromino *tetromino = &TETROMINOS[6];
    for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
        for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
            uint8_t value = get_component(tetromino, block_y, block_x, 0);
            if (value) {
                M5.Lcd.fillRect(block_x * 4, ((clicks % 4) + 1) * 30 + block_y * 4, 4, 4, TFT_BLUE);
            }
        }

    while (!(M5.BtnA.isPressed())) {
        M5.update();
        if (M5.BtnB.wasPressed()) {
            clicks++;
            M5.Lcd.fillRect(0, 0, 19, 160, TFT_BLACK);
            for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
                for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
                    uint8_t value = get_component(tetromino, block_y, block_x, 0);
                    if (value) {
                        M5.Lcd.fillRect(block_x * 4, ((clicks % 4) + 1) * 30 + block_y * 4, 4, 4, TFT_BLUE);
                    }
                }
        }
    }
    switch (clicks % 4) {
    case 1:
        game_state->game_mode = mines_and_timed_blocks;
    case 2:
        game_state->game_mode = mines_only;
    case 3:
        game_state->game_mode = timed_blocks_only;
    case 4:
        game_state->game_mode = no_mines_no_timed_blocks;
    }

    M5.Lcd.fillScreen(TFT_BLACK);
}