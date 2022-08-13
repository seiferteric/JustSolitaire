#include "solitaire.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

const char *const card_names[] = {"ace", "2", "3",  "4",    "5",     "6",   "7",
                                  "8",   "9", "10", "jack", "queen", "king"};
const char *const suite_names[] = {"diamond", "club", "heart", "spade"};
const char *const face_name[] = {"Down", "Up"};

SDL_Window *win;
SDL_Renderer *ren;
SDL_Surface *s_cards[DSIZE + 4] = {};
SDL_Surface *s_cards_r[DSIZE + 4] = {};
SDL_Texture *t_cards_r[DSIZE + 4] = {};

SDL_Rect ng_rect, undo_rect;

int BUT_ORIG_W;
int BUT_ORIG_H;

int CARD_ORIG_W;
int CARD_ORIG_H;

int CARD_W;
int CARD_H;
float CARD_SCALE;

int WIN_W;
int WIN_H;

int MAX_LEN = 0;

int GAME_END_TIME = 0;
float RND_OFF = 0;
enum STATE GAME_STATE = RUNNING;
int LAST_FRAME_TICKS = 0;
int game_num = 0;

struct {
  BOOL clicked;
  BOOL moving;
  SDL_Point hand_pos;
  SDL_Point click_offset;
  struct Deck *hand_from;
} HandState;

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
void update_undo(void) {
  if(init_done) {
    for(int i=0;i<DECKS;i++) {
      memcpy(&undo_decks2[i], &undo_decks[i], sizeof(struct Deck));      
      memcpy(&undo_decks[i], card_table.deck_list[i], sizeof(struct Deck));
    }
  }
}
void undo_update_undo(void) {
  if(init_done) {
    for(int i=0;i<DECKS;i++) {
      memcpy(&undo_decks[i], &undo_decks2[i], sizeof(struct Deck));
    }
  }
}
void undo(void) {
  if(card_table.hand.len == 0) {
    for(int i=0;i<DECKS;i++) {
      memcpy(card_table.deck_list[i], &undo_decks[i], sizeof(struct Deck));
    }
  }
  scale_if_needed();
  need_update();
}
void shuffle_init(struct Deck *deck) {
  uint8_t cards[DSIZE];
  int count = DSIZE;
  init_deck(deck);
  srand(time(NULL) + game_num);
  game_num++;
  for (int i = 0; i < DSIZE; i++) {
    cards[i] = i;
  }
	while(count > 0) {
		int r = rand()%count;
		add_card(deck, cards[r], DOWN);
		cards[r] = cards[--count];
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
  if(drop_on->facing == DOWN || &drop_on->deck->cards[drop_on->deck->len-1] != drop_on)
    return FALSE;
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
  scale_if_needed();
  GAME_STATE = RUNNING;
}

void init_table(void) {
  if(init_done)
    update_undo();

  int ndecks = 0;
  memset(&card_table, 0, sizeof(struct Table));
  shuffle_init(&card_table.stock);
  card_table.stock.type = STOCK;
  card_table.stock.index = 0;
  card_table.deck_list[ndecks++] = &card_table.stock;
  for (int i = 0; i < FOUNDATIONS; i++) {
    init_deck(&card_table.foundations[i]);
    card_table.deck_list[ndecks++] = &card_table.foundations[i];
    card_table.foundations[i].type = FOUNDATION;
    card_table.foundations[i].index = i;
  }
  for (int p = 0; p < PILES; p++) {
    init_deck(&card_table.piles[p]);
    card_table.piles[p].stagger_y = TRUE;
    card_table.piles[p].stagger_n = -1;
    card_table.piles[p].type = PILE;
    card_table.piles[p].index = p;
    card_table.deck_list[ndecks++] = &card_table.piles[p];
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

  card_table.deck_list[ndecks++] = &card_table.waste;

  init_deck(&card_table.hand);
  card_table.hand.stagger_y = TRUE;
  card_table.hand.stagger_n = -1;
  card_table.hand.type = HAND;
  card_table.hand.index = 0;
  card_table.deck_list[ndecks++] = &card_table.hand;
  card_table.decks = ndecks;
  if(!init_done) {
    init_done = TRUE;
    update_undo();
  }
}
int load_textures(void) {
  for (int i = 0; i < DSIZE; i++) {
    char img_fname[256] = {};
    img_name_from_num(i, img_fname);
    s_cards[i] = IMG_Load(img_fname);
    
    if (NULL == s_cards[i]) {
      SDL_Log("Unable to load texture %s", img_fname);
      return -1;
    }
  }
  s_cards[DSIZE] = IMG_Load("img/back.png");
  if (NULL == s_cards[DSIZE]) {
    SDL_Log("Unable to load texture img/back.png");
    return -1;
  }
  s_cards[DSIZE + 1] = IMG_Load("img/outline.png");
  if (NULL == s_cards[DSIZE + 1]) {
    SDL_Log("Unable to load texture img/outline.png");
    return -1;
  }
  s_cards[DSIZE + 2] = IMG_Load("img/newgame.png");
  if (NULL == s_cards[DSIZE + 2]) {
    SDL_Log("Unable to load texture img/newgame.png");
    return -1;
  }
  s_cards[DSIZE + 3] = IMG_Load("img/undo.png");
  if (NULL == s_cards[DSIZE + 3]) {
    SDL_Log("Unable to load texture img/undo.png");
    return -1;
  }
  CARD_ORIG_H = CARD_H = s_cards[0]->h;
  CARD_ORIG_W = CARD_W = s_cards[0]->w;
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
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
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

void scale_if_needed(void) {

  int max_len = 0;
  for(int i=0;i<7;i++) {
    if(card_table.piles[i].len > max_len)
      max_len = card_table.piles[i].len;
  }
  if(max_len != MAX_LEN){
    MAX_LEN = max_len;
    scale();
  }
}

void scale(void) {
  
  float ch = (float)WIN_H/(2.0 + SEP_Y + (float)(MAX_LEN-1)*STAGGER_Y);
  float cw = (float)WIN_W/(7.0+SEP_X*6.0);
  if(ch/(float)CARD_ORIG_H < cw/(float)CARD_ORIG_W)
    CARD_SCALE = ch/(float)CARD_ORIG_H;
  else
    CARD_SCALE = cw/(float)CARD_ORIG_W;
  CARD_W = (int)((float)CARD_ORIG_W * CARD_SCALE);
  CARD_H = (int)((float)CARD_ORIG_H * CARD_SCALE);
  SDL_Point pf, pw;
  deck_xy(&card_table.foundations[0], &pf);
  deck_xy(&card_table.waste, &pw);
  SDL_Rect ng = {pw.x+CARD_W+STAGGER_X_N(2), pf.y, pf.x-(pw.x+CARD_W+STAGGER_X_N(2)), CARD_H/2};
  ng_rect = ng;
  ng.y += CARD_H/2;
  undo_rect = ng;
  

  for(int i=0;i<DSIZE+4;i++) {
    if(t_cards_r[i]) {
      SDL_FreeSurface(s_cards_r[i]);
      SDL_DestroyTexture(t_cards_r[i]);
    }
  }
  
  for(int i=0;i<(DSIZE+2);i++) {
    s_cards_r[i] = SDL_CreateRGBSurface(0, CARD_W, CARD_H, s_cards[i]->format->BitsPerPixel, s_cards[i]->format->Rmask, s_cards[i]->format->Gmask, s_cards[i]->format->Bmask, s_cards[i]->format->Amask);
    SDL_BlitScaled(s_cards[i], NULL, s_cards_r[i], NULL);
    t_cards_r[i] = SDL_CreateTextureFromSurface(ren, s_cards_r[i]);
  }
  s_cards_r[DSIZE+2] = SDL_CreateRGBSurface(0, ng_rect.w, ng_rect.h, s_cards[DSIZE+2]->format->BitsPerPixel, s_cards[DSIZE+2]->format->Rmask, s_cards[DSIZE+2]->format->Gmask, s_cards[DSIZE+2]->format->Bmask, s_cards[DSIZE+2]->format->Amask);
  SDL_BlitScaled(s_cards[DSIZE+2], NULL, s_cards_r[DSIZE+2], NULL);
  t_cards_r[DSIZE+2] = SDL_CreateTextureFromSurface(ren, s_cards_r[DSIZE+2]);

  s_cards_r[DSIZE+3] = SDL_CreateRGBSurface(0, undo_rect.w, undo_rect.h, s_cards[DSIZE+3]->format->BitsPerPixel, s_cards[DSIZE+3]->format->Rmask, s_cards[DSIZE+3]->format->Gmask, s_cards[DSIZE+3]->format->Bmask, s_cards[DSIZE+3]->format->Amask);
  SDL_BlitScaled(s_cards[DSIZE+3], NULL, s_cards_r[DSIZE+3], NULL);
  t_cards_r[DSIZE+3] = SDL_CreateTextureFromSurface(ren, s_cards_r[DSIZE+3]);


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
  while(SDL_PollEvent(&event)){
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
      if (event.key.keysym.sym == SDLK_u)
        undo();
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
    }
#endif
    }
#ifdef __EMSCRIPTEN__
    // need_update();
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
    update_undo();
    for (int i = 0; i < DRAW_NUM; i++) {
      struct Card *card = draw(&card_table.stock);
      if (card) {
        add_card(&card_table.waste, card->num, UP);
      }
    }
  } else {
    struct Card *card;
    if(card_table.waste.len)
      update_undo();
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
  update_undo();
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
  update_undo();
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
        update_undo();
        draw(card->deck);
        add_card(deck, card->num, UP);
        if (card->deck->len && card->deck->tail->facing == DOWN)
          flip_card(card->deck->tail);
        
        
        scale_if_needed();
        need_update();
        check_game_over();
        return;
      }
    } else {
      if (deck->len && deck->tail->suite == card->suite &&
          (card->card - deck->tail->card) == 1) {
        update_undo();
        draw(card->deck);
        add_card(deck, card->num, UP);
        if (card->deck->len && card->deck->tail->facing == DOWN)
          flip_card(card->deck->tail);
        scale_if_needed();
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
  if (SDL_PointInRect(&mp, &ng_rect)) {
    new_game();
    need_update();
    return;
  }
  if (SDL_PointInRect(&mp, &undo_rect)) {
    undo();
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
      undo_update_undo();
      stack_deck(&card_table.hand, HandState.hand_from);
    }
  } else {
    undo_update_undo();
    stack_deck(&card_table.hand, HandState.hand_from);
  }
  
  scale_if_needed();
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
    //SDL_Event excess_events[10];
    //while (SDL_PeepEvents(excess_events, 10, SDL_GETEVENT, SDL_MOUSEMOTION,
    //                      SDL_MOUSEMOTION) > 0) {
    //}
    need_update();
  }

}

int deck_xy(struct Deck *deck, SDL_Point *point) {
  point->x = 0;
  point->y = 0;
  int STOCK_X = ((float)WIN_W/2.0) - (3.5*CARD_W+SEP_X_N(3));
  int STOCK_Y = 0;
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
void draw_outline(struct Deck *deck) {
  SDL_Texture *cardimg = t_cards_r[DSIZE + 1];
  SDL_Rect rect = {};
  SDL_Point corner = {};
  deck_xy(deck, &corner);
  rect.x = corner.x;
  rect.y = corner.y;
  rect.w = CARD_W;
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

void draw_table(void) {

  SDL_RenderCopy(ren, t_cards_r[DSIZE + 2], NULL, &ng_rect);
  SDL_RenderCopy(ren, t_cards_r[DSIZE + 3], NULL, &undo_rect);
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
  if (!deck->len)
    return;
  int n_cards = 1;
  if (deck->stagger_n > 0)
    n_cards = deck->stagger_n < deck->len ? deck->stagger_n : deck->len;
  else if (-1 == deck->stagger_n)
    n_cards = deck->len;

  int c = deck->len - n_cards > 0 ? deck->len - n_cards - 1 : 0;
  for (; c < deck->len; c++) {
    struct Card *card = &deck->cards[c];
    SDL_Point corner = {};
    card_xy(card, &corner);
    draw_card(card, corner.x, corner.y);
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
  float t = SDL_GetTicks() - GAME_END_TIME;

  SDL_RenderClear(ren);
  for (int d = 0; d < FOUNDATIONS; d++) {
    struct Deck *deck = &card_table.foundations[d];
    SDL_Point corner;
    deck_xy(deck, &corner);
    for (int c = 0; c < deck->len; c++) {
      srand(d*(DSIZE/FOUNDATIONS) + c);
      float ang = RND_OFF + (float)(rand() % 360) * PI/180.0f;
      float v = (10.0 + rand()%10) / 10.0f;
      const float x = corner.x + cos(ang) * t * v;
      const float y = corner.y + sin(ang) * t * v;
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

  
  if (-1 == gfx_init())
    return -1;
  memset(&undo_decks, 0, sizeof(struct Deck)*DECKS);
  memset(&undo_decks2, 0, sizeof(struct Deck)*DECKS);

  new_game();
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
