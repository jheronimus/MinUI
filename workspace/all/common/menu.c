#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "menu.h"

///////////////////////////////

// hooks, set once by each binary that uses the menu
SDL_Surface* menu_screen;
PWR_callback_t menu_before_sleep;
PWR_callback_t menu_after_sleep;
void (*menu_hdmi_monitor)(void);
void (*menu_update_desc)(void);
char** menu_button_labels;

#define OPTION_PADDING 8

///////////////////////////////

int Menu_message(char* message, char** pairs) {
	GFX_setMode(MODE_MAIN);
	int dirty = 1;
	while (1) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) break;
		
		PWR_update(&dirty, NULL, menu_before_sleep, menu_after_sleep);
		
		if (dirty) {
			GFX_clear(menu_screen);
			GFX_blitMessage(font.medium, message, menu_screen, &(SDL_Rect){0,SCALE1(PADDING),menu_screen->w,menu_screen->h-SCALE1(PILL_SIZE+PADDING)});
			GFX_blitButtonGroup(pairs, 0, menu_screen, 1);
			GFX_flip(menu_screen);
			dirty = 0;
		}
		else GFX_sync();
		
		if (menu_hdmi_monitor) menu_hdmi_monitor();
	}
	GFX_setMode(MODE_MENU);
	return MENU_CALLBACK_NOP; // TODO: this should probably be an arg
}

int Menu_options(MenuList* list) {
	MenuItem* items = list->items;
	int type = list->type;

	int dirty = 1;
	int show_options = 1;
	int show_settings = 0;
	int await_input = 0;
	
	// dependent on option list offset top and bottom, eg. the gray triangles
	int max_visible_options = (menu_screen->h - ((SCALE1(PADDING + PILL_SIZE) * 2) + SCALE1(BUTTON_SIZE))) / SCALE1(BUTTON_SIZE); // 7 for 480, 10 for 720
	
	int count;
	int end;
	int visible_rows;
	for (count=0; items[count].name; count++);
	int selected = 0;
	int start = 0;
	end = MIN(count,max_visible_options);
	visible_rows = end;
	
	if (menu_update_desc) menu_update_desc();
	
	int defer_menu = 0;
	while (show_options) {
		// re-read items/count so callbacks may rebuild the list (eg. PAKs);
		// clamp scroll state if the list shrank between frames
		items = list->items;
		{
			int new_count;
			for (new_count=0; items[new_count].name; new_count++);
			if (new_count != count) {
				count = new_count;
				if (selected >= count) selected = count ? count - 1 : 0;
				if (end > count) end = count;
				if (start > selected) start = selected;
				if (end - start > visible_rows) end = start + visible_rows;
				if (end > count) end = count;
			}
		}

		if (await_input) {
			defer_menu = 1;
			list->on_confirm(list, selected);
			
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
			await_input = false;
		}
		
		GFX_startFrame();
		PAD_poll();

		if (list->on_update) {
			list->on_update(list);
			// on_update may rebuild the list; re-sync our local pointers
			items = list->items;
			{
				int new_count;
				for (new_count=0; items[new_count].name; new_count++);
				if (new_count != count) {
					count = new_count;
					if (selected >= count) selected = count ? count - 1 : 0;
					if (end > count) end = count;
					if (start > selected) start = selected;
					if (end - start > visible_rows) end = start + visible_rows;
					if (end > count) end = count;
				}
			}
			dirty = 1;
		}
		
		if (PAD_justRepeated(BTN_UP)) {
			selected -= 1;
			if (selected<0) {
				selected = count - 1;
				start = MAX(0,count - max_visible_options);
				end = count;
			}
			else if (selected<start) {
				start -= 1;
				end -= 1;
			}
			dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
		}
		else {
			MenuItem* item = &items[selected];
			if (item->values && item->values!=menu_button_labels) { // not an input binding
				if (PAD_justRepeated(BTN_LEFT)) {
					if (item->value>0) item->value -= 1;
					else {
						int j;
						for (j=0; item->values[j]; j++);
						item->value = j - 1;
					}
				
					if (item->on_change) item->on_change(list, selected);
					else if (list->on_change) list->on_change(list, selected);
				
					dirty = 1;
				}
				else if (PAD_justRepeated(BTN_RIGHT)) {
					if (item->values[item->value+1]) item->value += 1;
					else item->value = 0;
				
					if (item->on_change) item->on_change(list, selected);
					else if (list->on_change) list->on_change(list, selected);
				
					dirty = 1;
				}
			}
		}
		
		// uint32_t now = SDL_GetTicks();
		if (PAD_justPressed(BTN_B)) { // || PAD_tappedMenu(now)
			show_options = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			MenuItem* item = &items[selected];
			int result = MENU_CALLBACK_NOP;
			if (item->on_confirm) result = item->on_confirm(list, selected); // item-specific action, eg. Save for all games
			else if (item->submenu) result = Menu_options(item->submenu); // drill down, eg. main options menu
			// TODO: is there a way to defer on_confirm for MENU_INPUT so we can clear the currently set value to indicate it is awaiting input? 
			// eg. set a flag to call on_confirm at the beginning of the next frame?
			else if (list->on_confirm) {
				if (item->values==menu_button_labels) await_input = 1; // button binding
				else result = list->on_confirm(list, selected); // list-specific action, eg. show item detail view or input binding
			}
			if (result==MENU_CALLBACK_EXIT) show_options = 0;
			else {
				if (result==MENU_CALLBACK_NEXT_ITEM) {
					selected += 1;
					if (selected>=count) {
						selected = 0;
						start = 0;
						end = visible_rows;
					}
					else if (selected>=end) {
						start += 1;
						end += 1;
					}
				}
				dirty = 1;
			}
		}
		else if (type==MENU_INPUT) {
			if (PAD_justPressed(BTN_X)) {
				MenuItem* item = &items[selected];
				item->value = 0;
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				selected += 1;
				if (selected>=count) {
					selected = 0;
					start = 0;
					end = visible_rows;
				}
				else if (selected>=end) {
					start += 1;
					end += 1;
				}
				dirty = 1;
			}
		}
		else if (PAD_justPressed(BTN_X) && list->on_aux) {
			list->on_aux(list, selected);
			dirty = 1;
		}
		
		if (!defer_menu) PWR_update(&dirty, &show_settings, menu_before_sleep, menu_after_sleep);
		
		if (defer_menu && PAD_justReleased(BTN_MENU)) defer_menu = 0;
		
		if (dirty) {
			GFX_clear(menu_screen);
			GFX_blitHardwareGroup(menu_screen, show_settings);
			
			char* desc = NULL;
			SDL_Surface* text;

			if (type==MENU_LIST) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest item, including its badge
					// (right-aligned text) so names and badges never collide
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						int bw = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						if (item->badge[0])
							TTF_SizeUTF8(font.tiny, item->badge, &bw, NULL);
						w += SCALE1(OPTION_PADDING*2);
						if (bw) w += bw + SCALE1(OPTION_PADDING);
						if (w>mw) mw = w;
					}
					// cache the result
					list->max_width = mw = MIN(mw, menu_screen->w - SCALE1(PADDING *2));
				}
				
				int ox = (menu_screen->w - mw) / 2;
				int oy = SCALE1(PADDING + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						if (item->badge[0]) {
							int bw = 0;
							TTF_SizeUTF8(font.tiny, item->badge, &bw, NULL);
							w += bw + SCALE1(OPTION_PADDING);
						}
						
						GFX_blitPill(ASSET_BUTTON, menu_screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
					
					if (item->badge[0]) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->badge, COLOR_WHITE);
						SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
				}
			}
			else if (type==MENU_FIXED) {
				// NOTE: no need to calculate max width
				int mw = menu_screen->w - SCALE1(PADDING*2);
				// int lw,rw;
				// lw = rw = mw / 2;
				int ox,oy;
				ox = oy = SCALE1(PADDING);
				// Clear the top-right hardware group (battery/wifi pill, which
				// occupies SCALE1(PADDING)..SCALE1(PADDING)+SCALE1(PILL_SIZE))
				// so the first row no longer abuts it.
				oy += SCALE1(PILL_SIZE) + SCALE1(4);
				
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, menu_screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
					}
					
					if (item->badge[0]) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->badge, COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
					else if (item->value>=0 && item->values) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
					
					if (j==selected_row) {
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, menu_screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
				}
			}
			else if (type==MENU_VAR || type==MENU_INPUT) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest row
					int mrw = 0;
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						int lw = 0;
						int rw = 0;
						TTF_SizeUTF8(font.small, item->name, &lw, NULL);
						
						// every value list in an input table is the same
						// so only calculate rw for the first item...
						if (!mrw || type!=MENU_INPUT) {
							for (int j=0; item->values[j]; j++) {
								TTF_SizeUTF8(font.tiny, item->values[j], &rw, NULL);
								if (lw+rw>w) w = lw+rw;
								if (rw>mrw) mrw = rw;
							}
						}
						else {
							w = lw + mrw;
						}
						w += SCALE1(OPTION_PADDING*4);
						if (w>mw) mw = w;
					}
					fflush(stdout);
					// cache the result
					list->max_width = mw = MIN(mw, menu_screen->w - SCALE1(PADDING *2));
				}
				
				int ox = (menu_screen->w - mw) / 2;
				int oy = SCALE1(PADDING + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, menu_screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
						
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, menu_screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
					
					if (await_input && j==selected_row) {
						// buh
					}
					else if (item->badge[0]) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->badge, COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
					else if (item->value>=0 && item->values) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, menu_screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
				}
			}
			
			if (count>max_visible_options) {
				#define SCROLL_WIDTH 24
				#define SCROLL_HEIGHT 4
				int ox = (menu_screen->w - SCALE1(SCROLL_WIDTH))/2;
				int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
				if (start>0) GFX_blitAsset(ASSET_SCROLL_UP,   NULL, menu_screen, &(SDL_Rect){ox, SCALE1(PADDING) + oy});
				if (end<count) GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, menu_screen, &(SDL_Rect){ox, menu_screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + oy});
			}
			
			if (!desc && list->desc) desc = list->desc;
			
			if (desc) {
				int w,h;
				GFX_sizeText(font.tiny, desc, SCALE1(12), &w,&h);
				GFX_blitText(font.tiny, desc, SCALE1(12), COLOR_WHITE, menu_screen, &(SDL_Rect){
					(menu_screen->w - w) / 2,
					menu_screen->h - SCALE1(PADDING) - h,
					w,h
				});
			}
			
			GFX_flip(menu_screen);
			dirty = 0;
		}
		else GFX_sync();
		if (menu_hdmi_monitor) menu_hdmi_monitor();
	}
	
	return 0;
}
