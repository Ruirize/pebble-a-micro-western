#include "pebble.h"

#define DIFFICULTY_BASE 100

#define STATE_RESTING 0
#define STATE_COUNTDOWN_3 1
#define STATE_COUNTDOWN_2 2
#define STATE_COUNTDOWN_1 3
#define STATE_COUNTDOWN_WAIT 4
#define STATE_COUNTDOWN_GO 5
#define STATE_SHOT 6
#define STATE_WIN 7
#define STATE_HIT 8
#define STATE_LOSE 9
	
#define WAIT_RESTING 3000
#define WAIT_COUNTDOWN 1000
#define WAIT_RESULT 750
	
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

static Window *window;
static GRect window_frame;
static Layer *game_layer;

static int game_state;
static AppTimer *game_timer;
static int game_difficulty;
static bool game_hand_open = false;
static bool game_waiting_for_reaction = false;
static bool game_player_reacted = false;
static int game_wins = 0;

static void game_update(void *data) {
	app_log(APP_LOG_LEVEL_DEBUG, "main.c", 42, "Game state update!");
	switch(game_state) {
		case STATE_RESTING:
			game_state = STATE_COUNTDOWN_3;
			app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_3:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_2;
				game_hand_open = false;
			} else { game_hand_open = true; }
			app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_2:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_1;
				game_hand_open = false;
			} else { game_hand_open = true; }
			app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 layer_mark_dirty(game_layer);
		break;
		case STATE_COUNTDOWN_1:
			if (game_hand_open) {
				game_state = STATE_COUNTDOWN_WAIT;
				game_hand_open = false;
				app_timer_register(WAIT_COUNTDOWN + (rand() % 5001), game_update, NULL);
				layer_mark_dirty(game_layer);
			} else {
				game_hand_open = true; 
				app_timer_register(WAIT_COUNTDOWN / 2, game_update, NULL);
		 	layer_mark_dirty(game_layer);
			}
		break;
		case STATE_COUNTDOWN_WAIT:
			game_state = STATE_COUNTDOWN_GO;
		 layer_mark_dirty(game_layer);
			game_timer = app_timer_register(DIFFICULTY_BASE + game_difficulty, game_update, NULL);
			game_waiting_for_reaction = true;
			game_player_reacted = false;
		break;
		case STATE_COUNTDOWN_GO:
			layer_mark_dirty(game_layer);
			game_waiting_for_reaction = false;
			if (game_player_reacted) {
				game_state = STATE_SHOT;
				app_timer_register(WAIT_RESULT, game_update, NULL);
				layer_mark_dirty(game_layer);
			} else {
				game_state = STATE_HIT;
				app_timer_register(WAIT_RESULT, game_update, NULL);
				layer_mark_dirty(game_layer);
			}
		break;
		
		case STATE_SHOT:
			game_state = STATE_WIN;
			game_wins++;
			app_timer_register(WAIT_RESTING, game_update, NULL);
			layer_mark_dirty(game_layer);
		break;
		
		case STATE_HIT:
			game_state = STATE_LOSE;
			game_wins = 0;
			app_timer_register(WAIT_RESTING, game_update, NULL);
			layer_mark_dirty(game_layer);
		break;
		
		case STATE_WIN:
		case STATE_LOSE:
			game_state = STATE_RESTING;
		 if (game_wins < 3) {
				app_timer_register(WAIT_COUNTDOWN, game_update, NULL);
				layer_mark_dirty(game_layer);
			}
		break;
	}
}

static void game_start() {
	srand(time(NULL));
	
	game_state = STATE_RESTING;
	game_difficulty = 200;
	game_wins = 0;
	game_player_reacted = false;
	game_waiting_for_reaction = false;
	game_hand_open = false;
	
	app_timer_register(WAIT_RESTING, game_update, NULL);
	layer_mark_dirty(game_layer);
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	Window *window = (Window *)context;
	
	if (game_waiting_for_reaction && game_state == STATE_COUNTDOWN_GO) {
		game_player_reacted = true;
		app_timer_reschedule(game_timer, 1);
	}
	
	if (game_state == STATE_RESTING && game_wins >= 3) {
		game_start();
	}
}

static void game_draw_wins(GContext* ctx) {
	int i;
	graphics_context_set_compositing_mode(ctx, GCompOpOr);
	for (i = 0; i < game_wins; i++) {
		graphics_draw_bitmap_in_rect(ctx, image_check, (GRect){.origin=(GPoint){109 + (i*6),147},  .size=(GSize){8,13}});
	}
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
}

static void game_draw(Layer *me, GContext* ctx) {
	switch(game_state) {
		case STATE_RESTING:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
		break;
		case STATE_COUNTDOWN_3:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_3, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			}
		break;
		case STATE_COUNTDOWN_2:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_2, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			}
		break;
		case STATE_COUNTDOWN_1:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_1, (GRect){.origin=(GPoint){41,147},  .size=(GSize){8,13}});
			if (game_hand_open) {
				graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			}
		break;
		case STATE_COUNTDOWN_WAIT:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
		break;
		case STATE_COUNTDOWN_GO:
			graphics_draw_bitmap_in_rect(ctx, image_resting, (GRect){.origin=(GPoint){0,0},  .size=(GSize){144,140}});
			graphics_draw_bitmap_in_rect(ctx, image_status,  (GRect){.origin=(GPoint){0,140},.size=(GSize){144,28}});
			graphics_draw_bitmap_in_rect(ctx, image_open, (GRect){.origin=(GPoint){29,76},  .size=(GSize){47,39}});
			graphics_draw_bitmap_in_rect(ctx, image_go, (GRect){.origin=(GPoint){41,147},  .size=(GSize){14,13}});
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
	
	game_draw_wins(ctx);
}

static void window_load(Window *window) {
 Layer *window_layer = window_get_root_layer(window);
 GRect frame = window_frame = layer_get_frame(window_layer);

 game_layer = layer_create(frame);
	layer_set_update_proc(game_layer, game_draw);
 layer_add_child(window_layer, game_layer);
	
	game_start();
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
}

static void deinit(void) {
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

 window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
