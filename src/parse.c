#include "parse.h"
#include "border.h"
#include "hashtable.h"
#include <stdlib.h> // For malloc, realloc, free, strtoul
#include <ctype.h>  // For isxdigit, tolower

static bool str_starts_with(char* string, char* prefix) {
  if (!string || !prefix) return false;
  if (strlen(string) < strlen(prefix)) return false;
  if (strncmp(prefix, string, strlen(prefix)) == 0) return true;
  return false;
}

static bool parse_list(struct table* list, char* token) {
  uint32_t token_len = strlen(token) + 1;
  char copy[token_len];
  memcpy(copy, token, token_len);

  char* name;
  char* cursor = copy;
  bool entry_found = false;

  table_clear(list);
  while((name = strsep(&cursor, ","))) {
    if (strlen(name) > 0) {
      _table_add(list, name, strlen(name) + 1, (void*)true);
      entry_found = true;
    }
  }
  return entry_found;
}

static bool parse_color(struct color_style* style, char* token) {
  if (sscanf(token, "=0x%x", &style->color) == 1) {
    style->stype = COLOR_STYLE_SOLID;
    return true;
  }
  else if (sscanf(token, "=glow(0x%x)", &style->color) == 1) {
    style->stype = COLOR_STYLE_GLOW;
    return true;
  }
  else if (sscanf(token,
             "=gradient(top_left=0x%x,bottom_right=0x%x)",
             &style->gradient.color1,
             &style->gradient.color2) == 2) {
    style->stype = COLOR_STYLE_GRADIENT;
    style->gradient.direction = TL_TO_BR;
    return true;
  }
  else if (sscanf(token,
             "=gradient(top_right=0x%x,bottom_left=0x%x)",
             &style->gradient.color1,
             &style->gradient.color2) == 2) {
    style->stype = COLOR_STYLE_GRADIENT;
    style->gradient.direction = TR_TO_BL;
    return true;
  }
  else printf("[?] Borders: Invalid color argument color%s\n", token);

  return false;
}

// --- Helper functions for animated gradient parsing ---

// Converts a hex character to its integer value (0-15)
// Returns -1 if not a valid hex character.
static int hex_char_to_int(char c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// Converts a hex string (e.g., "RRGGBB" or "AARRGGBB") to uint32_t (0xAARRGGBB)
// Assumes FF for alpha if only 6 hex digits are provided.
static bool hex_string_to_uint32(const char* hex_str, uint32_t* out_color) {
    if (!hex_str || !out_color) return false;

    int len = strlen(hex_str);
    uint32_t color_val = 0;
    const char* p = hex_str;

    if (len == 8) { // AARRGGBB
        // Use strtoul for simplicity and error checking
        char *endptr;
        unsigned long ul_color = strtoul(hex_str, &endptr, 16);
        if (*endptr != '\0') { // Invalid characters in string
            printf("[?] Borders: Invalid hex color string '%s'\n", hex_str);
            return false;
        }
        *out_color = (uint32_t)ul_color;
        return true;
    } else if (len == 6) { // RRGGBB, assume FF alpha
        color_val = 0xFF000000; // Default alpha FF
        char *endptr;
        unsigned long ul_color = strtoul(hex_str, &endptr, 16);
         if (*endptr != '\0') { // Invalid characters in string
            printf("[?] Borders: Invalid hex color string '%s'\n", hex_str);
            return false;
        }
        color_val |= (uint32_t)ul_color;
        *out_color = color_val;
        return true;
    } else {
        printf("[?] Borders: Invalid hex color string length for '%s' (must be 6 or 8 chars)\n", hex_str);
        return false;
    }
}

static bool parse_animated_gradient_colors(struct settings* settings, char* token) {
    if (settings->parsed_gradient_colors) {
        free(settings->parsed_gradient_colors);
        settings->parsed_gradient_colors = NULL;
        settings->num_parsed_gradient_colors = 0;
    }

    uint32_t token_len = strlen(token) + 1;
    char copy[token_len];
    memcpy(copy, token, token_len);

    char* color_str;
    char* cursor = copy;
    int count = 0;
    int capacity = 10; // Initial capacity

    settings->parsed_gradient_colors = malloc(capacity * sizeof(uint32_t));
    if (!settings->parsed_gradient_colors) {
        perror("[!] Borders: Failed to allocate memory for gradient colors");
        return false;
    }

    while ((color_str = strsep(&cursor, ","))) {
        if (strlen(color_str) > 0) {
            uint32_t color_val;
            if (hex_string_to_uint32(color_str, &color_val)) {
                if (count >= capacity) {
                    capacity *= 2;
                    uint32_t* new_alloc = realloc(settings->parsed_gradient_colors, capacity * sizeof(uint32_t));
                    if (!new_alloc) {
                        perror("[!] Borders: Failed to reallocate memory for gradient colors");
                        free(settings->parsed_gradient_colors); // Free old block
                        settings->parsed_gradient_colors = NULL;
                        settings->num_parsed_gradient_colors = 0;
                        return false;
                    }
                    settings->parsed_gradient_colors = new_alloc;
                }
                settings->parsed_gradient_colors[count++] = color_val;
            } else {
                // hex_string_to_uint32 already printed an error
                // Continue parsing other colors if possible, or decide to fail all
            }
        }
    }
    settings->num_parsed_gradient_colors = count;
    if (count == 0 && settings->parsed_gradient_colors) { // No valid colors parsed
        free(settings->parsed_gradient_colors);
        settings->parsed_gradient_colors = NULL;
    }
    return count > 0;
}

// --- End Helper functions ---

uint32_t parse_settings(struct settings* settings, int count, char** arguments) {
  static char active_color[] = "active_color";
  static char inactive_color[] = "inactive_color";
  static char background_color[] = "background_color";
  static char blacklist[] = "blacklist=";
  static char whitelist[] = "whitelist=";
  // --- Added for Gradient Animation ---
  static char animated_gradient_opt[] = "animated_gradient";
  static char animated_gradient_colors_opt[] = "animated_gradient_colors=";
  static char animated_gradient_steps_opt[] = "animated_gradient_steps=";
  static char animated_gradient_duration_opt[] = "animated_gradient_duration=";
  // --- End Added for Gradient Animation ---


  char order = 'a';
  uint32_t update_mask = 0;
  for (int i = 0; i < count; i++) {
    if (str_starts_with(arguments[i], active_color)) {
      if (parse_color(&settings->active_window,
                                 arguments[i] + strlen(active_color))) {
        update_mask |= BORDER_UPDATE_MASK_ACTIVE;
      }
    }
    else  if (str_starts_with(arguments[i], inactive_color)) {
      if (parse_color(&settings->inactive_window,
                                 arguments[i] + strlen(inactive_color))) {
        update_mask |= BORDER_UPDATE_MASK_INACTIVE;
      }
    }
    else if (str_starts_with(arguments[i], background_color)) {
      if (parse_color(&settings->background,
                                 arguments[i] + strlen(background_color))) {
        update_mask |= BORDER_UPDATE_MASK_ALL;
        settings->show_background = settings->background.color & 0xff000000;
      }
    }
    else if (str_starts_with(arguments[i], blacklist)) {
      settings->blacklist_enabled = parse_list(&settings->blacklist,
                                               arguments[i]
                                               + strlen(blacklist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (str_starts_with(arguments[i], whitelist)) {
      settings->whitelist_enabled = parse_list(&settings->whitelist,
                                               arguments[i]
                                               + strlen(whitelist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    // --- Added for Gradient Animation ---
    else if (str_starts_with(arguments[i], animated_gradient_colors_opt)) {
        if (parse_animated_gradient_colors(settings, arguments[i] + strlen(animated_gradient_colors_opt))) {
            update_mask |= BORDER_UPDATE_MASK_ACTIVE; // Or a new animation-specific mask
        }
    }
    else if (sscanf(arguments[i], "animated_gradient_steps=%d", &settings->animated_gradient_steps) == 1) {
        if (settings->animated_gradient_steps <= 0) settings->animated_gradient_steps = 1; // Ensure positive
        update_mask |= BORDER_UPDATE_MASK_ACTIVE;
    }
    else if (sscanf(arguments[i], "animated_gradient_duration=%f", &settings->animated_gradient_duration_sec) == 1) {
        if (settings->animated_gradient_duration_sec <= 0.0f) settings->animated_gradient_duration_sec = 1.0f; // Ensure positive
        update_mask |= BORDER_UPDATE_MASK_ACTIVE;
    }
    else if (str_starts_with(arguments[i], animated_gradient_opt)) {
        char* value = arguments[i] + strlen(animated_gradient_opt);
        if (*value == '=') value++; // Skip '=' if present, e.g., animated_gradient=on
        if (strcmp(value, "on") == 0) {
            settings->animated_gradient_enabled = true;
            update_mask |= BORDER_UPDATE_MASK_ACTIVE;
        } else if (strcmp(value, "off") == 0) {
            settings->animated_gradient_enabled = false;
            update_mask |= BORDER_UPDATE_MASK_ACTIVE;
        } else {
            printf("[?] Borders: Invalid value for animated_gradient: '%s' (expected 'on' or 'off')\n", value);
        }
    }
    // --- End Added for Gradient Animation ---
    else if (sscanf(arguments[i], "width=%f", &settings->border_width) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "order=%c", &order) == 1) {
      if (order == 'a') settings->border_order = BORDER_ORDER_ABOVE;
      else settings->border_order = BORDER_ORDER_BELOW;
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "style=%c", &settings->border_style) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (strcmp(arguments[i], "hidpi=on") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = true;
    }
    else if (strcmp(arguments[i], "hidpi=off") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = false;
    }
    else if (strcmp(arguments[i], "ax_focus=on") == 0) {
      settings->ax_focus = true;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (strcmp(arguments[i], "ax_focus=off") == 0) {
      settings->ax_focus = false;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (sscanf(arguments[i], "apply-to=%d", &settings->apply_to) == 1) {
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else {
      printf("[?] Borders: Invalid argument '%s'\n", arguments[i]);
    }
  }
  return update_mask;
}
