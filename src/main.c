#include "pebble.h"

#define STATE_SPLASH 0
#define STATE_RESTING 1
#define STATE_COUNTDOWN_1 2
#define STATE_COUNTDOWN_2 3
#define STATE_COUNTDOWN_3 4
#define STATE_COUNTDOWN_WAIT 5
#define STATE_COUNTDOWN_GO 6
#define STATE_SHOT 7
#define STATE_WIN 8
#define STATE_HIT 9
#define STATE_LOSE 10
	
#define WAIT_RESTING 3000
#define WAIT_COUNTDOWN 1000
#define WAIT_RESULT 750

#define GAME_MAX_WINS 20
#define GAME_MIN_EASINESS 225
#define GAME_MAX_EASINESS 850
#define GAME_MIN_SUSPENSE 2000
#define GAME_MAX_SUSPENSE 20000

static GBitmap *image_1;
static GBitmap *image_2;
static GBitmap *image_3;
static GBitmap *image_go;
static GBitmap *image_check;
static GBitmap *image_rip;
static GBitmap *image_resting;
static GBitmap *image_shot;
static GBitmap *image_win;
static GBitmap *image_hit;
static GBitmap *image_lose;
static GBitmap *image_closed;
static GBitmap *image_open;
static GBitmap *image_status;
static GBitmap *image_wait;
static GBitmap *image_splash;
static GFont *font_score;

static Window *window;
static GRect window_frame;
static Layer *game_layer;

static int game_state;
static AppTimer *game_timer;
static int game_easiness;
static int game_suspense;
static bool game_hand_open = false;
static bool game_waiting_for_reaction = false;
static bool game_player_reacted = false;
static int game_wins = 0;
static bool game_is_first = true;

static void game_update(void *data);

static void round_start() {
	int win_multiplier = (game_wins * 1000) / GAME_MAX_WINS;
	game_easiness = (GAME_MAX_EASINESS*1000) - ((GAME_MAX_EASINESS-GAME_MIN_EASINESS) * win_multiplier);
	game_suspense = (GAME_MIN_SUSPENSE*1000) + ((GAME_MAX_SUSPENSE-GAME_MIN_SUSPENSE) * win_multiplier);
	
	game_easiness /= 1000;
	game_suspense /= 1000;

	game_player_reacted = false;
	game_waiting_for_reaction = false;
	game_hand_open = false;
	game_state = STATE_RESTING;
}
static void game_start() {
	srand(time(NULL));
	
	game_wins = 0;
	if (game_is_first && persist_exists(0)) {
		game_wins = persist_read_int(0);
	}
	game_is_first = false;
	round_start();
	
	game_timer = app_timer_register(WAIT_RESTING, game_update, NULL);
	layer_mark_dirty(game_layer);
}

static void game_update(void *data) {
	switch(game_state) {
		case STATE_RESTING:
			game_state = STATE_COUNTDOWN_1;
			game_timer = app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_1:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_2;
				game_hand_open = false;
			} else { game_hand_open = true; }
			game_timer = app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_2:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_3;
				game_hand_open = false;
			} else { game_hand_open = true; }
			game_timer = app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_3:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_WAIT;
				game_hand_open = false;
				game_timer = app_timer_register(WAIT_COUNTDOWN + (rand() % game_suspense), game_update, NULL);
				layer_mark_dirty(game_layer);
			} else {
				game_hand_open = true; 
				game_timer = app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 	layer_mark_dirty(game_layer);
			}
		break;
		case STATE_COUNTDOWN_WAIT:
			game_state = STATE_COUNTDOWN_GO;
		 layer_mark_dirty(game_layer);
			game_timer = app_timer_register(game_easiness, game_update, NULL);
			game_waiting_for_reaction = true;
			game_player_reacted = false;
		break;
		case STATE_COUNTDOWN_GO:
			layer_mark_dirty(game_layer);
			game_waiting_for_reaction = false;
			vibes_short_pulse();
			if (game_player_reacted) {
				game_state = STATE_SHOT;
				game_timer = app_timer_register(WAIT_RESULT, game_update, NULL);
				layer_mark_dirty(game_layer);
			} else {
				game_state = STATE_HIT;
				game_timer = app_timer_register(WAIT_RESULT, game_update, NULL);
				layer_mark_dirty(game_layer);
			}
		break;
		
		case STATE_SHOT:
			game_state = STATE_WIN;
			game_wins++;
			game_timer = app_timer_register(WAIT_RESTING, game_update, NULL);
			layer_mark_dirty(game_layer);
		break;
		
		case STATE_HIT:
			game_state = STATE_LOSE;
			game_wins = 0;
			game_timer = app_timer_register(WAIT_RESTING, game_update, NULL);
			layer_mark_dirty(game_layer);
		break;
		
		case STATE_WIN:
		case STATE_LOSE:
		 if (game_wins < GAME_MAX_WINS) {
				round_start();
				game_timer = app_timer_register(WAIT_COUNTDOWN, game_update, NULL);
				layer_mark_dirty(game_layer);
			} else {
				game_state = STATE_SPLASH;
				persist_write_int(0, 0);
			}
		break;
	}
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (game_waiting_for_reaction && game_state == STATE_COUNTDOWN_GO) {
		game_player_reacted = true;
		app_timer_reschedule(game_timer, 1);
	}
	
	if (!game_waiting_for_reaction && game_state <= STATE_COUNTDOWN_GO && game_state > STATE_RESTING) {
		game_state = STATE_COUNTDOWN_GO;
		game_waiting_for_reaction = true;
		game_player_reacted = false;
		app_timer_reschedule(game_timer, 1);
	}
	
	if (game_state == STATE_SPLASH) {
		game_start();
	}
}

static void game_draw_wins(GContext* ctx) {
	if (game_wins <= 5) {
		int i;
		graphics_context_set_compositing_mode(ctx, GCompOpOr);
		for (i = 0; i < game_wins; i++) {
			graphics_draw_bitmap_in_rect(ctx, image_check, (GRect){.origin=(GPoint){101 + (i*6),147},  .size=(GSize){8,13}});
		}
		graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	} else {
		char score_text[3];
		snprintf(score_text, 3, "%d", game_wins);
		graphics_draw_text(ctx, score_text, font_score, (GRect){.origin=(GPoint){100,141},  .size=(GSize){30,14}}, GTextOverflowModeFill, GTextAlignmentRight, NULL);
	}
}

static void game_draw(Layer *me, GContext* ctx) {
	switch(game_state) {
		case STATE_SPLASH:
			graphics_draw_bitmap_in_rect(ctx, image_splash, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,168}});
		break;
		case STATE_RESTING:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
		graphics_draw_bitmap_in_rect(ctx, image_wait, (GRect){.origin=(GPoint){56,147},  .size=(GSize){28,13}});
		break;
		case STATE_COUNTDOWN_1:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_1, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			} else {
				graphics_draw_bitmap_in_rect(ctx, image_wait, (GRect){.origin=(GPoint){56,147},  .size=(GSize){28,13}});
			}
		break;
		case STATE_COUNTDOWN_2:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_2, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			} else {
				graphics_draw_bitmap_in_rect(ctx, image_wait, (GRect){.origin=(GPoint){56,147},  .size=(GSize){28,13}});
			}
		break;
		case STATE_COUNTDOWN_3:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_3, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			} else {
				graphics_draw_bitmap_in_rect(ctx, image_wait, (GRect){.origin=(GPoint){56,147},  .size=(GSize){28,13}});
			}
		break;
		case STATE_COUNTDOWN_WAIT:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_wait, (GRect){.origin=(GPoint){56,147},  .size=(GSize){28,13}});
		break;
		case STATE_COUNTDOWN_GO:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			graphics_draw_bitmap_in_rect(ctx, image_go, (GRect){.origin=(GPoint){39,147},  .size=(GSize){43,13}});
		break;
		
		case STATE_SHOT:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_shot, (GRect){.origin=(GPoint){8,0},  .size=(GSize){136,140}});
		break;
		case STATE_WIN:
			graphics_draw_bitmap_in_rect(ctx, image_win, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
		break;
		
		case STATE_HIT:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_hit, (GRect){.origin=(GPoint){14,0},  .size=(GSize){84,116}});
		break;
		case STATE_LOSE:
			graphics_draw_bitmap_in_rect(ctx, image_lose, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status, (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_rip, (GRect){.origin=(GPoint){60,149},.size=(GSize){19,10}});
		break;
	}
	
	if (game_state != STATE_SPLASH) {
	 game_draw_wins(ctx);
	}
}

static void window_load(Window *window) {
 Layer *window_layer = window_get_root_layer(window);
 GRect frame = window_frame = layer_get_frame(window_layer);

 game_layer = layer_create(frame);
	layer_set_update_proc(game_layer, game_draw);
 layer_add_child(window_layer, game_layer);
	
	game_state = STATE_SPLASH;
}

static void window_unload(Window *window) {
 layer_destroy(game_layer);
}

void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}

static void init(void) {	
 window = window_create();
	window_set_fullscreen(window, true);
 window_set_window_handlers(window, (WindowHandlers) {
	 .load = window_load,
  .unload = window_unload
 });
 window_stack_push(window, true);
 window_set_background_color(window, GColorBlack);
	
	window_set_click_config_provider(window, (ClickConfigProvider)config_provider);
	
	image_1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_1);
	image_2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_2);
	image_3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_3);
	image_go = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_GO);
	image_check = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_CHECK);
	image_rip = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_RIP);
	image_resting = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SCREEN_RESTING);
	image_shot = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SCREEN_SHOT);
	image_win = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SCREEN_WIN);
	image_hit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SCREEN_HIT);
	image_lose = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SCREEN_LOSE);
	image_closed = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HAND_CLOSED);
	image_open = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HAND_OPEN);
	image_status = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS);
	image_wait = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WAIT);
	image_splash = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPLASH);
	
	font_score = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

static void deinit(void) {
	if (!game_is_first) {
		persist_write_int(0, game_wins);
	}

	gbitmap_destroy(image_1);
	gbitmap_destroy(image_2);
	gbitmap_destroy(image_3);
	gbitmap_destroy(image_go);
	gbitmap_destroy(image_check);
	gbitmap_destroy(image_rip);
	gbitmap_destroy(image_resting);
	gbitmap_destroy(image_shot);
	gbitmap_destroy(image_win);
	gbitmap_destroy(image_hit);
	gbitmap_destroy(image_lose);
	gbitmap_destroy(image_closed);
	gbitmap_destroy(image_open);
	gbitmap_destroy(image_status);
	gbitmap_destroy(image_wait);
	gbitmap_destroy(image_splash);

 window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
