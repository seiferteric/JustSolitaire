#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DSIZE 52
#define DECKS 14
#define DSUITES 4
#define DRAW_NUM 3

#define FOUNDATIONS 4
#define PILES 7

#define PAD_X 0.02f
#define PAD_Y 0.01f

#define SEP_X 0.05f
#define SEP_Y 0.05f
#define SEP_X_N(N) ((float)N*SEP_X*CARD_W)
#define SEP_Y_N(N) ((float)N*SEP_Y*CARD_H)

#define PAD_X_N(N) ((float)N*PAD_X*WIN_W)
#define PAD_Y_N(N) ((float)N*PAD_Y*WIN_H)

#define STAGGER_X 0.1f
#define STAGGER_Y 0.1f

#define PI 3.141f

typedef uint8_t bool;
typedef enum { FALSE, TRUE } BOOL;
enum SUITE { DIAMOND, CLUB, HEART, SPADE };
enum COLOR { RED, BLACK };
enum CARD {
  ACE,
  TWO,
  THREE,
  FOUR,
  FIVE,
  SIX,
  SEVEN,
  EIGHT,
  NINE,
  TEN,
  JACK,
  QUEEN,
  KING
};
enum FACE { DOWN, UP };
enum DECK_TYPE { FOUNDATION, PILE, WASTE, STOCK, HAND };
enum STATE { RUNNING, ENDING, END };
struct Card {
  uint8_t num;
  enum FACE facing;
  enum SUITE suite;
  enum CARD card;
  struct Deck *deck;
  const char *card_name;
  const char *suite_name;
};
struct Deck {
  struct Card cards[DSIZE];
  struct Card *head;
  struct Card *tail;
  uint8_t len;
  bool stagger_x;
  bool stagger_y;
  int stagger_n;
  enum DECK_TYPE type;
  int index;
};

struct Table {
  struct Deck foundations[4];
  struct Deck piles[7];
  struct Deck waste;
  struct Deck stock;
  struct Deck hand;
  struct Deck *deck_list[14];
  int decks;
} card_table;

enum SUITE suite_from_num(uint8_t card);
enum CARD card_from_num(uint8_t card);

const char *suite_name_from_suite(enum SUITE suite);
const char *card_name_from_card(enum CARD card);
const char *card_name_from_num(uint8_t num);
const char *suite_name_from_num(uint8_t num);
const char *face_name_from_face(enum FACE face);
void shuffle_init(struct Deck *deck);
struct Card *draw(struct Deck *deck);
void init_deck(struct Deck *deck);
void flip_deck(struct Deck *deck);
void flip_card(struct Card *card);
void add_card(struct Deck *deck, uint8_t num, enum FACE face);
enum COLOR suite_color(enum SUITE suite);
enum COLOR card_color(struct Card *card);
BOOL can_drop_pile(struct Card *hand_card, struct Card *drop_on);
BOOL can_drop_deck(struct Deck *hand, struct Deck *drop_on);
void stack_deck(struct Deck *src, struct Deck *dst);

void init_table(void);

void new_game(void);

void handle_click(SDL_Event *event);
void handle_dbl_click(SDL_Event *event);
void handle_unclick(SDL_Event *event);
void handle_motion(SDL_Event *event);
void img_name_from_num(uint8_t num, char *file_name);
void scale(void);
int gfx_init(void);
void draw_card(struct Card *card, int x, int y);
void draw_deck(struct Deck *deck);
void main_loop(void);
void draw_table(void);
int card_xy(struct Card *card, SDL_Point *point);
int deck_xy(struct Deck *deck, SDL_Point *point);
void update(void);
void check_game_over(void);
void quick_move(struct Card *card);
void draw_header(void);
void game_over(void);
void need_update(void);
#define texture_from_card(CARD)                                                \
  (CARD->facing == UP ? t_cards[CARD->num] : t_cards[DSIZE]);
