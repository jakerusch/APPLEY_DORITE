#include <pebble.h>
#define KEY_CITY 0
#define KEY_TEMP 1
#define KEY_ICON 2

static Window *s_main_window;
static Layer *s_battery_icon_layer, *s_line_layer, *s_health_line_layer;
static TextLayer *s_clock_layer, *s_date_layer, *s_temp_layer, *s_health_layer, *s_city_layer;
static GFont s_clock_font, s_date_font;
static int battery_percent, step_goal = 10000;
static GBitmap *s_bluetooth_bitmap, *s_charging_bitmap, *s_weather_bitmap, *s_health_bitmap;
static BitmapLayer *s_bluetooth_bitmap_layer, *s_charging_bitmap_layer, *s_weather_bitmap_layer, *s_health_bitmap_layer;
static bool charging;
static char icon_buf[32], health_buf[16];
static double step_count;

///////////////////////////
// update battery status //
///////////////////////////
static void battery_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  
  // draw horizontal battery icon (dynamic)
  graphics_draw_round_rect(ctx, GRect(128, 1, 14, 9), 1);
  int batt = battery_percent/10;
  graphics_fill_rect(ctx, GRect(130, 3, batt, 5), 1, GCornerNone);
  graphics_fill_rect(ctx, GRect(142, 3, 1, 5), 0, GCornerNone); 
  
  // set visibility of charging icon
  layer_set_hidden(bitmap_layer_get_layer(s_charging_bitmap_layer), !charging);  
}

//////////////////////////
// update health status //
//////////////////////////
static void health_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  // dynamically draw line across bottom
  graphics_draw_line(ctx, GPoint(0, 166), GPoint((step_count/step_goal)*144, 166));
}

///////////////////////////////////
// draw dividing lines on screen //
///////////////////////////////////
static void line_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(0, 94), GPoint(144, 94));
  graphics_draw_line(ctx, GPoint(0, 131), GPoint(144, 131));
}

//////////////////////
// load main window //
//////////////////////
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // background color
  window_set_background_color(s_main_window, GColorBlack);
  
  // fonts
  s_clock_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ULTRALIGHT_60));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ULTRALIGHT_12));
  
  // clock layer, forces 12hr time (24 hr won't fit at 60 font)
  s_clock_layer = text_layer_create(GRect(0, 0, bounds.size.w, 70));
  text_layer_set_background_color(s_clock_layer, GColorClear);
  text_layer_set_text_color(s_clock_layer, GColorWhite);
  text_layer_set_text_alignment(s_clock_layer, GTextAlignmentCenter);
  text_layer_set_font(s_clock_layer, s_clock_font);
  layer_add_child(window_layer, text_layer_get_layer(s_clock_layer));  
  
  // date layer, full weekday, month and day (Tuesday, August 18)
  s_date_layer = text_layer_create(GRect(0, 70, bounds.size.w, 16));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_layer, s_date_font);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));  
  
  // create battery layer for dynamic icon
  s_battery_icon_layer = layer_create(bounds);
  layer_set_update_proc(s_battery_icon_layer, battery_update_proc);
  layer_add_child(window_layer, s_battery_icon_layer);  
  
  // battery charging icon, displays to left of battery icon when charging or plugged in
  s_charging_bitmap = gbitmap_create_with_resource(RESOURCE_ID_LIGHTENING_WHITE_ICON);
  s_charging_bitmap_layer = bitmap_layer_create(GRect(118, 0, 10, 10));
  bitmap_layer_set_compositing_mode(s_charging_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_charging_bitmap_layer, s_charging_bitmap); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_charging_bitmap_layer));     
  
  // bluetooth disconnected icon, displays to left of battery charge icon when disconnected from phone
  s_bluetooth_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED_WHITE_ICON);
  s_bluetooth_bitmap_layer = bitmap_layer_create(GRect(111, 1, 9, 10));
  bitmap_layer_set_compositing_mode(s_bluetooth_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_bluetooth_bitmap_layer, s_bluetooth_bitmap); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bluetooth_bitmap_layer));    
  
  // draw lines on main screen
  s_line_layer = layer_create(bounds);
  layer_set_update_proc(s_line_layer, line_update_proc);
  layer_add_child(window_layer, s_line_layer);
  
  // create temperature text layer
  s_temp_layer = text_layer_create(GRect(30, 104, 28, 16));
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_text_color(s_temp_layer, GColorWhite);
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentCenter);
  text_layer_set_font(s_temp_layer, s_date_font);
  layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));      
  
  // create city text layer
  s_city_layer = text_layer_create(GRect(58, 104, 86, 16));
  text_layer_set_background_color(s_city_layer, GColorClear);
  text_layer_set_text_color(s_city_layer, GColorWhite);
  text_layer_set_text_alignment(s_city_layer, GTextAlignmentLeft);
  text_layer_set_font(s_city_layer, s_date_font);
  layer_add_child(window_layer, text_layer_get_layer(s_city_layer));      
  
  // create shoe icon
  s_health_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SHOE_WHITE_ICON);
  s_health_bitmap_layer = bitmap_layer_create(GRect(10, 141, 16, 16));
  bitmap_layer_set_compositing_mode(s_health_bitmap_layer, GCompOpSet);  
  bitmap_layer_set_bitmap(s_health_bitmap_layer, s_health_bitmap); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_health_bitmap_layer)); 
  
  // health text layer (number of steps)
  s_health_layer = text_layer_create(GRect(30, 141, 114, 16));
  text_layer_set_background_color(s_health_layer, GColorClear);
  text_layer_set_text_color(s_health_layer, GColorWhite);
  text_layer_set_text_alignment(s_health_layer, GTextAlignmentLeft);
  text_layer_set_font(s_health_layer, s_date_font);
  layer_add_child(window_layer, text_layer_get_layer(s_health_layer));   
  
  // draw health line across bottom (dynamic)
  s_health_line_layer = layer_create(bounds);
  layer_set_update_proc(s_health_line_layer, health_update_proc);
  layer_add_child(window_layer, s_health_line_layer);
}

////////////////////////////////
// update clock time and date //
////////////////////////////////
static void update_time() {
  // get a tm strucutre
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // write the current hours into a buffer
  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), "%l:%M", tick_time);  
  
  // write date to buffer
  static char date_buffer[32];
  strftime(date_buffer, sizeof(date_buffer), "%A, %B %e", tick_time);
  
  // display this time on the text layer
  text_layer_set_text(s_clock_layer, s_time_buffer);
  text_layer_set_text(s_date_layer, date_buffer);  
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
  }
}

// registers health update events
static void health_handler(HealthEventType event, void *context) {
  if(event==HealthEventMovementUpdate) {
    step_count = (double)health_service_sum_today(HealthMetricStepCount);
    
    // write to char_current_steps variable
    snprintf(health_buf, sizeof(health_buf), "%d/10000", (int)step_count);
    text_layer_set_text(s_health_layer, health_buf);
    
    // force update to circle
    layer_mark_dirty(s_health_line_layer);
  }
}

/////////////////////////////
// manage bluetooth status //
/////////////////////////////
static void bluetooth_callback(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(s_bluetooth_bitmap_layer), connected);
  if(!connected) {  
    // vibrate if disconnected from phone
    vibes_double_pulse();
  } 
}

/////////////////////////////////////
// registers battery update events //
/////////////////////////////////////
static void battery_handler(BatteryChargeState charge_state) {
  battery_percent = charge_state.charge_percent;
  if(charge_state.is_charging || charge_state.is_plugged) {
    charging = true;
  } else {
    charging = false;
  }
  // force update to circle
  layer_mark_dirty(s_battery_icon_layer);
}

//////////////////////////////////////
// display appropriate weather icon //
//////////////////////////////////////
static void load_icons(Window *window) {
  // populate icon variable
    if(strcmp(icon_buf, "clear-day")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_DAY_WHITE_ICON);  
    } else if(strcmp(icon_buf, "clear-night")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_NIGHT_WHITE_ICON);
    }else if(strcmp(icon_buf, "rain")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_RAIN_WHITE_ICON);
    } else if(strcmp(icon_buf, "snow")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SNOW_WHITE_ICON);
    } else if(strcmp(icon_buf, "sleet")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SLEET_WHITE_ICON);
    } else if(strcmp(icon_buf, "wind")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WIND_WHITE_ICON);
    } else if(strcmp(icon_buf, "fog")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FOG_WHITE_ICON);
    } else if(strcmp(icon_buf, "cloudy")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLOUDY_WHITE_ICON);
    } else if(strcmp(icon_buf, "partly-cloudy-day")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_DAY_WHITE_ICON);
    } else if(strcmp(icon_buf, "partly-cloudy-night")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_NIGHT_WHITE_ICON);
    }
  // populate weather icon
  s_weather_bitmap_layer = bitmap_layer_create(GRect(10, 104, 16, 16));
  bitmap_layer_set_compositing_mode(s_weather_bitmap_layer, GCompOpSet);  
  bitmap_layer_set_bitmap(s_weather_bitmap_layer, s_weather_bitmap); 
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_weather_bitmap_layer)); 
}

// unload main window
static void main_window_unload(Window *window) {
  layer_destroy(s_battery_icon_layer);
  layer_destroy(s_line_layer);
  layer_destroy(s_health_line_layer);
  text_layer_destroy(s_clock_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_temp_layer);
  text_layer_destroy(s_health_layer);
  text_layer_destroy(s_city_layer);
  gbitmap_destroy(s_bluetooth_bitmap);
  gbitmap_destroy(s_charging_bitmap);
  gbitmap_destroy(s_weather_bitmap);
  gbitmap_destroy(s_health_bitmap);
  bitmap_layer_destroy(s_bluetooth_bitmap_layer);
  bitmap_layer_destroy(s_charging_bitmap_layer);
  bitmap_layer_destroy(s_weather_bitmap_layer);
  bitmap_layer_destroy(s_health_bitmap_layer);
}

///////////////////
// weather calls //
///////////////////
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char city_buf[32];
  static char temp_buf[32];

  // Read tuples for data
  Tuple *city_tuple = dict_find(iterator, KEY_CITY);
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMP);
  Tuple *icon_tuple = dict_find(iterator, KEY_ICON);  

  // If all data is available, use it
  if(temp_tuple && icon_tuple && city_tuple) {
    
    // city
    snprintf(city_buf, sizeof(city_buf), "%s", city_tuple->value->cstring);
    text_layer_set_text(s_city_layer, city_buf);
    
    // temp
    snprintf(temp_buf, sizeof(temp_buf), "%s", temp_tuple->value->cstring);
    text_layer_set_text(s_temp_layer, temp_buf);

    // icon
    snprintf(icon_buf, sizeof(icon_buf), "%s", icon_tuple->value->cstring);
  }  
  
  load_icons(s_main_window);
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_callback");
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

////////////////////
// initialize app //
////////////////////
static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // show window on the watch with animated=true
  window_stack_push(s_main_window, true);
  
  // subscribe to time events
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Make sure the time is displayed from the start
  update_time();  
  
  // subscribe to health events 
  health_service_events_subscribe(health_handler, NULL); 
  // force initial update
  health_handler(HealthEventMovementUpdate, NULL);     
  
  // register with Battery State Service
  battery_state_service_subscribe(battery_handler);
  // force initial update
  battery_handler(battery_state_service_peek());  
  
  // register with bluetooth state service
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  bluetooth_callback(connection_service_peek_pebble_app_connection()); 
  
// Register weather callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);  
  
  // Open AppMessage for weather callbacks
  const int inbox_size = 96;
  const int outbox_size = 96;
  app_message_open(inbox_size, outbox_size);    
}

///////////////////////
// de-initialize app //
///////////////////////
static void deinit() {
  window_destroy(s_main_window);
}

/////////////
// run app //
/////////////
int main(void) {
  init();
  app_event_loop();
  deinit();
}