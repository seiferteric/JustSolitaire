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

#define SEP_X 0.05f
#define SEP_Y 0.05f
#define SEP_X_N(N) ((float)N*SEP_X*(float)CARD_W)
#define SEP_Y_N(N) ((float)N*SEP_Y*(float)CARD_H)

#define STAGGER_X 0.15f
#define STAGGER_Y 0.15f

#define STAGGER_X_N(N) ((float)N*STAGGER_X*(float)CARD_W)
#define STAGGER_Y_N(N) ((float)N*STAGGER_Y*(float)CARD_H)

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
  struct Deck *deck_list[DECKS];
  int decks;
} card_table;

struct Deck undo_decks[DECKS];
struct Deck undo_decks2[DECKS];

BOOL init_done = FALSE;

void shuffle_init(struct Deck *deck, int);
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

void init_table(int);

void new_game(int);

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
void game_over(void);
void need_update(void);
void scale_if_needed(void);
void undo(void);
void update_undo(void);
void undo_update_undo(void);

#define texture_from_card(CARD)                                                \
  (CARD->facing == UP ? t_cards_r[CARD->num] : t_cards_r[DSIZE]);
#define card_num_from_name_suite(NAME, SUITE) ((SUITE*13)+NAME)
#define suite_from_num(CARD) (CARD / (DSIZE / DSUITES))
#define card_from_num(CARD) (CARD % (DSIZE / DSUITES))
#define suite_name_from_suite(SUITE) suite_names[SUITE]
#define card_name_from_card(CARD) card_names[CARD]
#define card_name_from_num(NUM) card_name_from_card(card_from_num(NUM))
#define suite_name_from_num(NUM) suite_name_from_suite(suite_from_num(NUM))
#define face_name_from_face(FACE) (face_name[FACE])
