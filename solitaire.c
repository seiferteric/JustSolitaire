#include "solitaire.h"
#include <GL/gl.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

const char *const card_names[] = {"ace", "2", "3",  "4",    "5",     "6",   "7",
                                  "8",   "9", "10", "jack", "queen", "king"};
const char *const suite_names[] = {"diamond", "club", "heart", "spade"};
const char *const face_name[] = {"Down", "Up"};

SDL_Window *win;
SDL_Renderer *ren;
SDL_Texture *t_cards[DSIZE + 3];

SDL_Rect button_rect = {};

int BUT_ORIG_W;
int BUT_ORIG_H;

int CARD_ORIG_W;
int CARD_ORIG_H;

int CARD_W;
int CARD_H;
float CARD_SCALE;

int WIN_W;
int WIN_H;

int GAME_END_TIME = 0;
float RND_OFF = 0;
enum STATE GAME_STATE = RUNNING;
int LAST_FRAME_TICKS = 0;


struct {
  BOOL clicked;
  BOOL moving;
  SDL_Point hand_pos;
  SDL_Point click_offset;
  struct Deck *hand_from;
} HandState;

enum SUITE suite_from_num(uint8_t card) {
  uint8_t cards_in_suite = DSIZE / DSUITES;
  return card / cards_in_suite;
}
enum CARD card_from_num(uint8_t card) {
  uint8_t cards_in_suite = DSIZE / DSUITES;
  return card % cards_in_suite;
}

const char *suite_name_from_suite(enum SUITE suite) {
  return suite_names[suite];
}
const char *card_name_from_card(enum CARD card) { return card_names[card]; }
const char *card_name_from_num(uint8_t num) {
  return card_name_from_card(card_from_num(num));
}
const char *suite_name_from_num(uint8_t num) {
  return suite_name_from_suite(suite_from_num(num));
}
const char *face_name_from_face(enum FACE face) { return face_name[face]; }
void add_card(struct Deck *deck, uint8_t num, enum FACE face) {
  struct Card *card;
  if (deck->len > 0) {
    deck->tail++;
  }
  deck->len++;
  card = deck->tail;
  card->num = num;
  card->suite = suite_from_num(num);
  card->card = card_from_num(num);
  card->facing = face;
  card->deck = deck;
  card->card_name = card_name_from_card(card->card);
  card->suite_name = suite_name_from_suite(card->suite);
}
void shuffle_init(struct Deck *deck) {
  uint64_t deck_mark = 0;
  init_deck(deck);
  srand(time(NULL));
  for (int i = 0; i < DSIZE; i++) {
    while (1) {
      uint8_t drawn = rand() % DSIZE;
      uint64_t bit = UINT64_C(1) << drawn;
      if ((bit & deck_mark) == 0) {
        deck_mark |= bit;
        add_card(deck, drawn, DOWN);
        break;
      }
    }
  }
}

void init_deck(struct Deck *deck) {
  memset(deck, 0, sizeof(struct Deck));
  deck->head = deck->tail = deck->cards;
  deck->len = 0;
}

struct Card *draw(struct Deck *deck) {
  if (deck->len) {
    struct Card *card = deck->tail;
    if (deck->len > 1)
      deck->tail--;
    deck->len--;
    return card;
  }
  return NULL;
}
void flip_deck(struct Deck *deck) {
  struct Card card;
  for (int i = 0; i < deck->len / 2; i++) {
    card = deck->cards[deck->len - i - 1];
    deck->cards[deck->len - i - 1] = deck->cards[i];
    deck->cards[i] = card;
  }
  for (int i = 0; i < deck->len; i++) {
    flip_card(&deck->cards[i]);
  }
}
void flip_card(struct Card *card) {
  if (card->facing == UP) {
    card->facing = DOWN;
  } else {
    card->facing = UP;
  }
}

enum COLOR suite_color(enum SUITE suite) {
  if (suite == DIAMOND || suite == HEART)
    return RED;
  return BLACK;
}
enum COLOR card_color(struct Card *card) { return suite_color(card->suite); }

BOOL can_drop_pile(struct Card *hand_card, struct Card *drop_on) {
  if (drop_on->deck->type == PILE) {
    if (card_color(hand_card) != card_color(drop_on)) {
      if (drop_on->card - hand_card->card == 1 && hand_card->card != ACE) {
        if (drop_on->facing == UP && drop_on->deck != hand_card->deck) {
          return TRUE;
        }
      }
    }
  } else if (drop_on->deck->type == FOUNDATION) {
    if (drop_on->suite == hand_card->suite) {
      if (hand_card->card - drop_on->card == 1)
        return TRUE;
    }
  }
  return FALSE;
}
BOOL can_drop_deck(struct Deck *hand, struct Deck *drop_on) {
  if (drop_on->len)
    return FALSE;
  if (drop_on->type == FOUNDATION) {
    if (hand->len == 1 && hand->cards[0].card == ACE)
      return TRUE;
  }
  if (drop_on->type == PILE) {
    if (hand->len > 0 && hand->cards[0].card == KING) {
      return TRUE;
    }
  }
  return FALSE;
}
void stack_deck(struct Deck *src, struct Deck *dst) {
  struct Deck tmp_deck;
  init_deck(&tmp_deck);
  while (src->len) {
    struct Card *card = draw(src);
    add_card(&tmp_deck, card->num, UP);
  }
  while (tmp_deck.len) {
    struct Card *card = draw(&tmp_deck);
    add_card(dst, card->num, UP);
  }
}

void new_game(void) {
  init_table();
  GAME_STATE = RUNNING;
}

void init_table(void) {
  int ndecks = 0;
  memset(&card_table, 0, sizeof(struct Table));
  shuffle_init(&card_table.stock);
  card_table.stock.type = STOCK;
  card_table.stock.index = 0;
  card_table.deck_list[ndecks] = &card_table.stock;
  ndecks++;
  for (int i = 0; i < FOUNDATIONS; i++) {
    init_deck(&card_table.foundations[i]);
    card_table.deck_list[ndecks] = &card_table.foundations[i];
    card_table.foundations[i].type = FOUNDATION;
    card_table.foundations[i].index = i;
    ndecks++;
  }
  for (int p = 0; p < PILES; p++) {
    init_deck(&card_table.piles[p]);
    card_table.piles[p].stagger_y = TRUE;
    card_table.piles[p].stagger_n = -1;
    card_table.piles[p].type = PILE;
    card_table.piles[p].index = p;
    card_table.deck_list[ndecks] = &card_table.piles[p];
    ndecks++;
    for (int c = 0; c <= p; c++) {
      struct Card *card = draw(&card_table.stock);
      add_card(&card_table.piles[p], card->num, c == p ? UP : DOWN);
    }
  }
  init_deck(&card_table.waste);
  card_table.waste.stagger_x = TRUE;
  card_table.waste.stagger_n = 3;
  card_table.waste.type = WASTE;
  card_table.waste.index = 0;

  card_table.deck_list[ndecks] = &card_table.waste;
  ndecks++;

  init_deck(&card_table.hand);
  card_table.hand.stagger_y = TRUE;
  card_table.hand.stagger_n = -1;
  card_table.hand.type = HAND;
  card_table.hand.index = 0;
  card_table.deck_list[ndecks] = &card_table.hand;
  ndecks++;
  card_table.decks = ndecks;
}
int load_textures(void) {
  for (int i = 0; i < DSIZE; i++) {
    char img_fname[256] = {};
    img_name_from_num(i, img_fname);
    t_cards[i] = IMG_LoadTexture(ren, img_fname);
    if (NULL == t_cards[i]) {
      SDL_Log("Unable to load texture %s", img_fname);
      return -1;
    }
  }
  t_cards[DSIZE] = IMG_LoadTexture(ren, "img/back.png");
  if (NULL == t_cards[DSIZE]) {
    SDL_Log("Unable to load texture img/back.png");
    return -1;
  }
  t_cards[DSIZE + 1] = IMG_LoadTexture(ren, "img/outline.png");
  if (NULL == t_cards[DSIZE + 1]) {
    SDL_Log("Unable to load texture img/outline.png");
    return -1;
  }
  t_cards[DSIZE + 2] = IMG_LoadTexture(ren, "img/newgame.png");
  if (NULL == t_cards[DSIZE + 2]) {
    SDL_Log("Unable to load texture img/newgame.png");
    return -1;
  }
  SDL_QueryTexture(t_cards[DSIZE], NULL, NULL, &CARD_W, &CARD_H);
  CARD_ORIG_H = CARD_H;
  CARD_ORIG_W = CARD_W;
  return 0;
}

void img_name_from_num(uint8_t num, char *file_name) {
  const char *card_name = card_name_from_num(num);
  const char *suite_name = suite_name_from_num(num);
  sprintf(file_name, "img/%s_of_%ss.png", card_name, suite_name);
}
void quit(void) {
#ifdef __EMSCRIPTEN__
  emscripten_cancel_main_loop();
#endif
  SDL_Quit();
  exit(0);
}
void clear(void) {
  SDL_RenderClear(ren);
  SDL_RenderPresent(ren);
}
void update(void) {
  SDL_RenderClear(ren);
  draw_table();
  if (card_table.hand.len)
    draw_deck(&card_table.hand);
  SDL_RenderPresent(ren);
}

int gfx_init(void) {

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    return -1;
  }
  SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

  WIN_W = 1100;
  WIN_H = 768;
  win = SDL_CreateWindow("JustSolitaire", SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED, WIN_W, WIN_H,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  if (-1 == load_textures())
    return -1;
  SDL_SetRenderDrawColor(ren, 0x07, 0x63, 0x24, 0);
  scale();
  clear();

  return 0;
}
void scale(void) {
  
  float cw = WIN_W*(1.0-PAD_X*2.0)/(7.0+SEP_X*6.0);
  CARD_SCALE = cw/(float)CARD_ORIG_W;
  CARD_W = (int)((float)CARD_ORIG_W * CARD_SCALE);
  CARD_H = (int)((float)CARD_ORIG_H * CARD_SCALE);

  SDL_QueryTexture(t_cards[DSIZE + 2], NULL, NULL, &button_rect.w,
                   &button_rect.h);
}
void main_loop(void) {

  SDL_Event event;
  SDL_Event excess_events[10];

  while (1) {
    if(GAME_STATE == ENDING) {
      game_over();
#ifdef __EMSCRIPTEN__
      return;
#endif
      continue;
    }
#ifdef __EMSCRIPTEN__
  SDL_PollEvent(&event);
#else
    SDL_WaitEvent(&event);
#endif
    SDL_Point a, b;
    deck_xy(&card_table.piles[0], &a);
    deck_xy(&card_table.piles[PILES-1 ], &b);
    
    switch (event.type) {
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_CLOSE)
        quit();
      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        WIN_W = event.window.data1;
        WIN_H = event.window.data2;
        scale();
      }
      need_update();
      break;
    case SDL_KEYUP:
      if (event.key.keysym.sym == SDLK_ESCAPE)
        quit();
      if (event.key.keysym.sym == SDLK_n &&
          (event.key.keysym.mod & KMOD_CTRL)) {
        new_game();
        need_update();
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (1 == event.button.clicks)
        handle_click(&event);
      else
        handle_dbl_click(&event);
      break;
    case SDL_MOUSEBUTTONUP:
      handle_unclick(&event);
      break;
    case SDL_MOUSEMOTION:
      handle_motion(&event);
      break;
    case SDL_USEREVENT:
      //Remove excess update events, only need it once.
      while (SDL_PeepEvents(excess_events, 10, SDL_GETEVENT, SDL_USEREVENT,
                            SDL_USEREVENT) > 0) {
      }
      update();
      break;
#ifdef __EMSCRIPTEN__
    case SDL_QUIT:
      emscripten_cancel_main_loop();
      break;
#endif
    }
#ifdef __EMSCRIPTEN__
    need_update();
    return;
#endif
  }

  return;
}

struct Deck *find_deck(int x, int y) {
  SDL_Point mouse;
  SDL_Rect card_rect;
  mouse.x = x;
  mouse.y = y;
  // Not including hand deck
  for (int i = 0; i < DECKS - 1; i++) {
    struct Deck *deck = card_table.deck_list[i];
    SDL_Point corner = {};
    deck_xy(deck, &corner);
    card_rect.x = corner.x;
    card_rect.y = corner.y;
    card_rect.w = CARD_W;
    card_rect.h = CARD_H;

    if (SDL_PointInRect(&mouse, &card_rect)) {
      return deck;
    }
  }
  return NULL;
}
struct Card *find_card(int x, int y) {
  struct Card *clicked_card = NULL;
  SDL_Point mouse;
  SDL_Rect card_rect;
  mouse.x = x;
  mouse.y = y;
  // Not including hand deck
  for (int i = 0; i < DECKS - 1; i++) {
    struct Deck *deck = card_table.deck_list[i];
    if (!deck->len)
      continue;
    int n_cards = 1;
    if (-1 == deck->stagger_n)
      n_cards = deck->len;
    else if (deck->stagger_n > 0)
      n_cards = deck->len < deck->stagger_n ? deck->len : deck->stagger_n;

    for (int l = deck->len - 1; l >= deck->len - n_cards; l--) {
      struct Card *card = &deck->cards[l];
      if (card->facing == DOWN)
        break;
      SDL_Point corner = {};
      card_xy(card, &corner);
      card_rect.x = corner.x;
      card_rect.y = corner.y;
      card_rect.w = CARD_W;
      card_rect.h = CARD_H;
      if (SDL_PointInRect(&mouse, &card_rect)) {
        clicked_card = card;
        break;
      }
    }
  }
  return clicked_card;
}
void stock_click(void) {
  if (card_table.stock.len > 0) {
    for (int i = 0; i < DRAW_NUM; i++) {
      struct Card *card = draw(&card_table.stock);
      if (card) {
        add_card(&card_table.waste, card->num, UP);
      }
    }
  } else {
    struct Card *card;
    while ((card = draw(&card_table.waste))) {
      add_card(&card_table.stock, card->num, DOWN);
    }
  }
  need_update();
}
void waste_foundation_click(SDL_Event *event, struct Card *card) {
  if (NULL == card)
    return;
  if (card->deck->type == WASTE && card != card_table.waste.tail)
    return;

  SDL_Point corner;
  card_xy(card, &corner);
  HandState.hand_from = card->deck;
  HandState.clicked = 1;
  HandState.click_offset.x = event->button.x - corner.x;
  HandState.click_offset.y = event->button.y - corner.y;
  HandState.hand_pos.x = event->motion.x - HandState.click_offset.x;
  HandState.hand_pos.y = event->motion.y - HandState.click_offset.y;

  draw(card->deck);
  add_card(&card_table.hand, card->num, card->facing);
}
void pile_click(SDL_Event *event, struct Card *card) {
  if (NULL == card || card->facing != UP)
    return;
  SDL_Point corner;
  card_xy(card, &corner);
  HandState.hand_from = card->deck;
  HandState.clicked = 1;
  HandState.click_offset.x = event->button.x - corner.x;
  HandState.click_offset.y = event->button.y - corner.y;
  HandState.hand_pos.x = event->motion.x - HandState.click_offset.x;
  HandState.hand_pos.y = event->motion.y - HandState.click_offset.y;

  struct Card *dcard = NULL;
  while ((dcard = draw(card->deck)) != card) {
    add_card(&card_table.hand, dcard->num, DOWN);
  }
  add_card(&card_table.hand, dcard->num, DOWN);

  flip_deck(&card_table.hand);
}

void handle_click(SDL_Event *event) {

  struct Card *card = find_card(event->button.x, event->button.y);
  struct Deck *deck;
  if (NULL == card)
    deck = find_deck(event->button.x, event->button.y);
  else
    deck = card->deck;
  if (NULL == deck)
    return;

  switch (deck->type) {
  case STOCK:
    stock_click();
    break;
  case WASTE:
  case FOUNDATION:
    waste_foundation_click(event, card);
    break;
  case PILE:
    pile_click(event, card);
    break;
  default:
    return;
  }
}

void quick_move(struct Card *card) {
  if (card != card->deck->tail)
    return;
  for (int f = 0; f < FOUNDATIONS; f++) {
    struct Deck *deck = &card_table.foundations[f];
    if (card->card == ACE) {
      if (0 == deck->len) {
        draw(card->deck);
        add_card(deck, card->num, UP);
        if (card->deck->len && card->deck->tail->facing == DOWN)
          flip_card(card->deck->tail);
        need_update();
        check_game_over();
        return;
      }
    } else {
      if (deck->len && deck->tail->suite == card->suite &&
          (card->card - deck->tail->card) == 1) {
        draw(card->deck);
        add_card(deck, card->num, UP);
        if (card->deck->len && card->deck->tail->facing == DOWN)
          flip_card(card->deck->tail);
        need_update();
        check_game_over();
        return;
      }
    }
  }
  return;
}

void handle_dbl_click(SDL_Event *event) {
  SDL_Point mp;
  mp.x = event->button.x;
  mp.y = event->button.y;
  if (SDL_PointInRect(&mp, &button_rect)) {
    new_game();
    need_update();
    return;
  }
  struct Card *card = find_card(event->button.x, event->button.y);
  struct Deck *deck;
  if (NULL == card)
    deck = find_deck(event->button.x, event->button.y);
  else
    deck = card->deck;
  if (NULL == deck)
    return;
  switch (deck->type) {
  case WASTE:
  case PILE:
    if (card && card == card->deck->tail)
      quick_move(card);
    break;
  case STOCK:
    stock_click();
    break;
  default:
    return;
  }
}
void handle_unclick(SDL_Event *event) {
  if (!HandState.clicked)
    return;
  HandState.clicked = FALSE;
  HandState.moving = FALSE;
  struct Card *drop_on = find_card(event->button.x, event->button.y);
  struct Deck *drop_deck = NULL;
  if (drop_on && can_drop_pile(&card_table.hand.cards[0], drop_on)) {
    stack_deck(&card_table.hand, drop_on->deck);
    if (HandState.hand_from->len && HandState.hand_from->tail->facing == DOWN)
      flip_card(HandState.hand_from->tail);
  } else if ((drop_deck = find_deck(event->button.x, event->button.y)) &&
             !drop_deck->len) {
    if (can_drop_deck(&card_table.hand, drop_deck)) {
      stack_deck(&card_table.hand, drop_deck);
      if (HandState.hand_from->len && HandState.hand_from->tail->facing == DOWN)
        flip_card(HandState.hand_from->tail);
    } else {
      stack_deck(&card_table.hand, HandState.hand_from);
    }
  } else {
    stack_deck(&card_table.hand, HandState.hand_from);
  }
  need_update();
  check_game_over();
}
void handle_motion(SDL_Event *event) {
  if (!HandState.clicked)
    return;
  int cur_x, cur_y;
  SDL_GetMouseState(&cur_x, &cur_y);
  HandState.hand_pos.x = cur_x - HandState.click_offset.x;
  HandState.hand_pos.y = cur_y - HandState.click_offset.y;
  if (!HandState.moving) {
    HandState.moving = TRUE;
  } else {
    // This just skips updates if we are moving too fast
    // since we don't need all those redraws...
    SDL_Event excess_events[10];
    while (SDL_PeepEvents(excess_events, 10, SDL_GETEVENT, SDL_MOUSEMOTION,
                          SDL_MOUSEMOTION) > 0) {
    }
    need_update();
  }

}

int deck_xy(struct Deck *deck, SDL_Point *point) {
  point->x = 0;
  point->y = 0;
  int STOCK_X = PAD_X_N(1);
  int STOCK_Y = PAD_Y_N(1);
  switch (deck->type) {
  case STOCK:
    point->x = STOCK_X;
    point->y = STOCK_Y;
    return 0;
  case HAND:
    point->x = HandState.hand_pos.x;
    point->y = HandState.hand_pos.y;
    return 0;
  case WASTE:
    point->x = STOCK_X + CARD_W + SEP_X_N(1);
    point->y = STOCK_Y;
    return 0;
  case FOUNDATION:
    point->x = STOCK_X + CARD_W + SEP_X_N(3) + 2.0*CARD_W + deck->index * (CARD_W + SEP_X_N(1));
    point->y = STOCK_Y;
    return 0;
  case PILE:
    point->x = STOCK_X + deck->index * (CARD_W + SEP_X_N(1));
    point->y = STOCK_Y + CARD_H + SEP_Y_N(1);
    return 0;
  default:
    return -1;
  }
  return -1;
}

int card_xy(struct Card *card, SDL_Point *point) {
  SDL_Point dp = {};
  deck_xy(card->deck, &dp);
  if (!card->deck->stagger_n ||
      (!card->deck->stagger_x && !card->deck->stagger_y)) {
    *point = dp;
    return 0;
  }
  for (int c = 0; c < card->deck->len; c++) {
    if (card == &card->deck->cards[c]) {
      if (card->deck->stagger_x) {
        if (card->deck->stagger_n == -1) {
          dp.x += (CARD_W * STAGGER_X) * c;
        } else if (card->deck->stagger_n > 0) {
          if (card->deck->len - c <= card->deck->stagger_n) {
            int n_or_len = card->deck->stagger_n > card->deck->len
                               ? card->deck->stagger_n
                               : card->deck->len;
            dp.x += (CARD_W * STAGGER_X) *
                    (card->deck->stagger_n - (n_or_len - c));
          }
        }
      }
      if (card->deck->stagger_y) {
        if (card->deck->stagger_n == -1) {
          dp.y += (CARD_H * STAGGER_Y) * c;
        } else if (card->deck->stagger_n > 0) {
          if (card->deck->len - c <= card->deck->stagger_n) {
            int n_or_len = card->deck->stagger_n > card->deck->len
                               ? card->deck->stagger_n
                               : card->deck->len;
            dp.y += (CARD_H * STAGGER_Y) *
                    (card->deck->stagger_n - (n_or_len - c));
          }
        }
      }

      *point = dp;
      return 0;
    }
  }
  return -1;
}
void draw_card(struct Card *card, int x, int y) {
  SDL_Texture *cardimg = texture_from_card(card);
  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = CARD_W;
  rect.h = CARD_H;
  SDL_RenderCopy(ren, cardimg, NULL, &rect);
}
void draw_partial_card(struct Card *card, int x, int y, int w, int h) {
  SDL_Texture *cardimg = texture_from_card(card);
  const SDL_Rect drect = {x, y, w, h};
  const SDL_Rect srect = {0, 0, (float)w/CARD_SCALE, (float)h/CARD_SCALE};
  SDL_RenderCopy(ren, cardimg, &srect, &drect);
}
void draw_outline(struct Deck *deck) {
  SDL_Texture *cardimg = t_cards[DSIZE + 1];
  SDL_Rect rect = {};
  SDL_Point corner = {};
  deck_xy(deck, &corner);
  rect.x = corner.x;
  rect.y = corner.y;
  rect.w = CARD_W;
  if(deck->stagger_x && deck->stagger_n)
    rect.w += STAGGER_X*CARD_W*(deck->stagger_n-1);
  rect.h = CARD_H;

  SDL_RenderCopy(ren, cardimg, NULL, &rect);
}
void need_update(void) {
  SDL_Event event;
  SDL_UserEvent userevent;

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;
  userevent.data1 = NULL;
  userevent.data2 = NULL;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_PushEvent(&event);
}

void draw_header(void) {
  SDL_Point p;
  deck_xy(&card_table.foundations[0], &p);
  SDL_Rect hrect = {p.x-CARD_W+SEP_X_N(3), p.y, CARD_W-SEP_X_N(4), CARD_H};

  button_rect = hrect;
  SDL_RenderCopy(ren, t_cards[DSIZE + 2], NULL, &button_rect);

}
void draw_table(void) {

  draw_header();
  // Except HAND deck
  for (int d = 0; d < card_table.decks - 1; d++) {
    struct Deck *deck = card_table.deck_list[d];
    draw_deck(deck);
  }
}

void draw_deck(struct Deck *deck) {
  if (deck->type != HAND && !deck->len) {
    draw_outline(deck);
    return;
  }
  // if (!deck->len)
    // return;
  int n_cards = 1;
  if (deck->stagger_n > 0)
    n_cards = deck->stagger_n < deck->len ? deck->stagger_n : deck->len;
  else if (-1 == deck->stagger_n)
    n_cards = deck->len;

  int c = deck->len - n_cards > 0 ? deck->len - n_cards - 1 : 0;

  for (; c < deck->len; c++) {
    int w = CARD_W;
    int h = CARD_H;
    struct Card *card = &deck->cards[c];
    SDL_Point corner = {};
    card_xy(card, &corner);
    if(n_cards != 1 && deck->stagger_x && c < deck->len-1) {
      w = STAGGER_X*(float)CARD_W*1.36;
    }
    if(n_cards != 1 && deck->stagger_y && c < deck->len-1) {
      h = STAGGER_Y*(float)CARD_H*1.25;
    }
    draw_partial_card(card, corner.x, corner.y, w, h);
  }
}

void check_game_over(void) {
  int fcards = 0;
  for (int f = 0; f < FOUNDATIONS; f++) {
    fcards += card_table.foundations[f].len;
  }
  if (fcards == DSIZE) {
    GAME_STATE = ENDING;
    RND_OFF = (float)(rand() % 360) * (PI/180.0);
    GAME_END_TIME = SDL_GetTicks();
  }
}

void game_over(void) {
  BOOL done = TRUE;
  int diff = SDL_GetTicks() - LAST_FRAME_TICKS;
  if(diff < 10)
    SDL_Delay(10 - diff);
  
  LAST_FRAME_TICKS = SDL_GetTicks();
  float i = (SDL_GetTicks() - GAME_END_TIME)/10.0;

  SDL_RenderClear(ren);
  for (int d = 0; d < FOUNDATIONS; d++) {
    struct Deck *deck = &card_table.foundations[d];
    SDL_Point corner;
    deck_xy(deck, &corner);
    for (int c = 0; c < deck->len; c++) {
      float ang = RND_OFF + 2.0 * PI *
                  ((float)(d + c) / (float)(FOUNDATIONS + deck->len - 2));
      const float x = corner.x + cos(ang) * i * 10.0;
      const float y = corner.y + sin(ang) * i * 10.0;
      draw_card(&deck->cards[c], x, y);
      if(x + CARD_W >= 0 && x <= WIN_W && y + CARD_H >= 0 && y <= WIN_H)
        done = FALSE;
      
    }
  }

  SDL_RenderPresent(ren);
  if(done) {
    GAME_STATE = END;
    need_update();
  }
}

int main(int argc, char* argv[]) {

  new_game();
  if (-1 == gfx_init())
    return -1;

  //Code for testing end of game animation:
  // int f = 0;
  // for(int i=0;i<PILES; i++) {
  //    while(card_table.piles[i].len) {
  //      struct Card *card = draw(&card_table.piles[i]);
  //      add_card(&card_table.foundations[f%4], card->num, UP);
  //      f++;
  //    }
  // }
  // while(card_table.stock.len > 1) {
  //  struct Card *card = draw(&card_table.stock);
  //  add_card(&card_table.foundations[f%4], card->num, UP);
  //  f++;
  // }
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(main_loop, 0, 1);
#else
  main_loop();
#endif
  return 0;
}
