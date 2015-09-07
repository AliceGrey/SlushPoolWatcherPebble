#include "pebble.h"

// POST variables

// Received variables
enum {
	USERNAME = 1,
  API_KEY = 2,
	FIVE_HASH = 3,
	//HOUR_HASH = 4,
	INVERT_COLOR_KEY = 4
};
	

static Window *window;
static AppTimer *timer;
static AppSync sync;
static uint8_t sync_buffer[124];

static TextLayer *text_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_five_layer;
static TextLayer *text_hash_layer;
static TextLayer *text_hour_layer;

static InverterLayer *inverter_layer = NULL;

static GFont font_last_price_small;
static GFont font_last_price_large;
static GFont font_hash_large;

static bool using_smaller_font = false;

static void set_timer();

//void failed(int32_t cookie, int http_status, void* context) {
//	if(cookie == 0 || cookie == BTC_HTTP_COOKIE) {
//		text_layer_set_text(&text_price_layer, "---");
//	}
//}

//static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d, %d", app_message_error, dict_error);
//}

static void set_invert_color(bool invert) {
	if (invert && inverter_layer == NULL) {
		// Add inverter layer
		Layer *window_layer = window_get_root_layer(window);

		inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
		layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
	} else if (!invert && inverter_layer != NULL) {
		// Remove Inverter layer
		layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
		inverter_layer_destroy(inverter_layer);
		inverter_layer = NULL;
	}
	// No action required
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	
	static char str[35] = "";
	bool invert;
	switch (key) {
		case FIVE_HASH:
			text_layer_set_text(text_five_layer, new_tuple->value->cstring);
			size_t len = strlen(new_tuple->value->cstring);
      text_layer_set_text(text_hash_layer, "TH/S");
			if (len > 6 && !using_smaller_font) {
				text_layer_set_font(text_five_layer, font_last_price_small);
				using_smaller_font = true;
        text_layer_set_text(text_hash_layer, "TH/S");
			} else if (len <= 6 && using_smaller_font) {
				text_layer_set_font(text_five_layer, font_last_price_large);
        text_layer_set_text(text_hash_layer, "TH'/'S");
				using_smaller_font = false;
			}
			break;
//		case HOUR_HASH:
			//text_layer_set_text(text_buy_price_layer, new_tuple->value->cstring);
	//		break;
		case INVERT_COLOR_KEY:
			invert = new_tuple->value->uint8 != 0;
			persist_write_bool(INVERT_COLOR_KEY, invert);
			set_invert_color(invert);
			break;
	}
}
char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}
void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "... Sync Error: %s", translate_error(app_message_error));
}

static void send_cmd(void) {
	Tuplet value = TupletInteger(0, 1);
	
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	
	if (iter == NULL) {
		return;
	}
	
	dict_write_tuplet(iter, &value);
	dict_write_end(iter);
	
	app_message_outbox_send();
}

static void timer_callback(void *data) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer tick");
	send_cmd();
	set_timer();
}

static void set_timer() {
	// Update again in 30 minutes.
	const uint32_t timeout_ms = 30 * 60 * 1000;
	timer = app_timer_register(timeout_ms, timer_callback, NULL);
}

static void update_time(struct tm *tick_time) {
	// Need to be static because they're used by the system later.
	static char time_text[] = "00:00";
	static char date_text[] = "Xxxxxxxxx 00";
	
	char *time_format;
	
	
	// TODO: Only update the date when it's changed.
	strftime(date_text, sizeof(date_text), "%B %e", tick_time);
	text_layer_set_text(text_date_layer, date_text);
	
	
	if (clock_is_24h_style()) {
		time_format = "%R";
	} else {
		time_format = "%I:%M";
	}
	
	strftime(time_text, sizeof(time_text), time_format, tick_time);
	
	// Kludge to handle lack of non-padded hour format string
	// for twelve hour clock.
	if (!clock_is_24h_style() && (time_text[0] == '0')) {
		memmove(time_text, &time_text[1], sizeof(time_text) - 1);
	}
	
	text_layer_set_text(text_time_layer, time_text);				
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	update_time(tick_time);
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	
	font_last_price_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIAVLO_MEDIUM_39));
	font_last_price_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIAVLO_MEDIUM_39));
  font_hash_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIAVLO_MEDIUM_39));
	
	text_five_layer = text_layer_create(GRect(0, 0, 144-0, 168-0));
	text_layer_set_text_color(text_five_layer, GColorWhite);
	text_layer_set_background_color(text_five_layer, GColorClear);
	text_layer_set_font(text_five_layer, font_last_price_large);
	text_layer_set_text_alignment(text_five_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(text_five_layer));
  
  text_hash_layer = text_layer_create(GRect(0, 64, 144-0, 168-40));
	text_layer_set_text_color(text_hash_layer, GColorWhite);
	text_layer_set_background_color(text_hash_layer, GColorClear);
	text_layer_set_font(text_hash_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	text_layer_set_text_alignment(text_hash_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(text_hash_layer));
  

	
	
	text_date_layer = text_layer_create(GRect(8, 96, 144-8, 168-96));
	text_layer_set_text_color(text_date_layer, GColorWhite);
	text_layer_set_background_color(text_date_layer, GColorClear);
	text_layer_set_font(text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIAVLO_15)));
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));
	
	text_time_layer = text_layer_create(GRect(7, 114, 144-7, 168-114));
	text_layer_set_text_color(text_time_layer, GColorWhite);
	text_layer_set_background_color(text_time_layer, GColorClear);
	text_layer_set_font(text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIAVLO_HEAVY_44)));
	layer_add_child(window_layer, text_layer_get_layer(text_time_layer));
	
	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	update_time(t);
	
	Tuplet initial_values[] = {
		TupletCString(FIVE_HASH, "---"),
    //TupletCString(FIVE_HASH, "---"),
//		TupletCString(HOUR_HASH, "---"),
		TupletInteger(INVERT_COLOR_KEY, persist_read_bool(INVERT_COLOR_KEY)),
	};
	
	app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
				  sync_tuple_changed_callback, sync_error_callback, NULL);
	
	//send_cmd();last_price
	timer = app_timer_register(2000, timer_callback, NULL);
	//set_timer();
}


static void window_unload(Window *window) {
	app_sync_deinit(&sync);
	
	tick_timer_service_unsubscribe();
	
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
	text_layer_destroy(text_five_layer);
  text_layer_destroy(text_hash_layer);

}

static void init(void) {
	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_set_fullscreen(window, true);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload
	});
	
	const int inbound_size = 124;
	const int outbound_size = 124;
	app_message_open(inbound_size, outbound_size);
	
	const bool animated = true;
	window_stack_push(window, animated);
}

static void deinit(void) {
	window_destroy(window);
}

int main(void){
	init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
	app_event_loop();
	deinit();
}


