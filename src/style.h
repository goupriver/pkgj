#pragma once

#define VITA_WIDTH 960
#define VITA_HEIGHT 544

#define PKGI_COLOR(r, g, b) ((r) | ((g) << 8) | ((b) << 16))

#define PKGI_UTF8_X "\xe2\x95\xb3" // 0x2573
#define PKGI_UTF8_O "\xe2\x97\x8b" // 0x25cb
#define PKGI_UTF8_T "\xe2\x96\xb3" // 0x25b3
#define PKGI_UTF8_S "\xe2\x96\xa1" // 0x25a1

#define PKGI_UTF8_INSTALLING "\xe2\x96\xb6" // 0x25b6
#define PKGI_UTF8_INSTALLED "\xe2\x97\x8f" // 0x25cf
#define PKGI_UTF8_PARTIAL "\xe2\x97\x8b" // 0x25cb
#define PKGI_COLOR_GAME_PRESENT PKGI_COLOR(50, 50, 255)

#define PKGI_UTF8_B "B"
#define PKGI_UTF8_KB "K" // "\xe3\x8e\x85" // 0x3385
#define PKGI_UTF8_MB "M" // "\xe3\x8e\x86" // 0x3386
#define PKGI_UTF8_GB "G" // "\xe3\x8e\x87" // 0x3387

#define PKGI_UTF8_CLEAR "\xc3\x97" // 0x00d7

#define PKGI_UTF8_SORT_ASC "\xe2\x96\xb2" // 0x25b2
#define PKGI_UTF8_SORT_DESC "\xe2\x96\xbc" // 0x25bc

#define PKGI_UTF8_CHECK_ON "\xe2\x97\x8f" // 0x25cf
#define PKGI_UTF8_CHECK_OFF "\xe2\x97\x8b" // 0x25cb

#define PKGI_COLOR_DIALOG_BACKGROUND PKGI_COLOR(48, 48, 48)
// при открытии где фильтры и сортировка [styler] menu.cpp
// #define PKGI_COLOR_MENU_BACKGROUND PKGI_COLOR(48, 48, 48)
#define PKGI_COLOR_MENU_BACKGROUND PKGI_COLOR(18, 18, 18)
// #define PKGI_COLOR_TEXT_MENU PKGI_COLOR(255, 255, 255)

// текст выбранный в фильтрах и сортировке [styler] menu.cpp
#define PKGI_COLOR_TEXT_MENU_SELECTED PKGI_COLOR(0, 255, 0)
#define PKGI_COLOR_TEXT_MENU PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_TEXT PKGI_COLOR(240, 240, 240)

// текст общий
// #define PKGI_COLOR_TEXT PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_HEAD PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_TAIL PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_DIALOG PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_ERROR PKGI_COLOR(255, 50, 50)

// полоска сверху и снизу
#define PKGI_COLOR_HLINE PKGI_COLOR(113, 131, 140)
// #define PKGI_COLOR_HLINE PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_SCROLL_BAR PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_BATTERY_LOW PKGI_COLOR(255, 50, 50)
#define PKGI_COLOR_BATTERY_CHARGING PKGI_COLOR(50, 255, 50)
// прорзраный бегунок в выборе игры
// #define PKGI_COLOR_SELECTED_BACKGROUND PKGI_COLOR(100, 100, 100)
#define PKGI_COLOR_PROGRESS_BACKGROUND PKGI_COLOR(100, 100, 100)
#define PKGI_COLOR_PROGRESS_BAR PKGI_COLOR(128, 255, 0)


// bg кнопок 
#define PKGI_COLOR_BUTTON PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_BUTTON_ACTIVE PKGI_COLOR(200, 200, 200)
//

//
// текст списоку игр
#define PKGI_COLOR_LIST_COLOR PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_LIST_TITLE_ID PKGI_COLOR(6, 144, 68)
#define PKGI_COLOR_LIST_REGION PKGI_COLOR(226, 150, 50) 
#define PKGI_COLOR_LIST_TITLE PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_LIST_SIZE PKGI_COLOR(128, 255, 0)
#define PKGI_COLOR_LIST_CIRCLE PKGI_COLOR(250, 221, 0)

// цвет текста и прямоугольной области в списоке игр в фокусе
#define PKGI_COLOR_SELECTED_BACKGROUND PKGI_COLOR(23, 23, 23)


// цвет кнопок пс отмена зарыть открыть 

#define PKGI_COLOR_PS_VITA_BUTTON PKGI_COLOR(200, 200, 200)

// end

#define PKGI_ANIMATION_SPEED 4000 // px/second

#define PKGI_MAIN_COLUMN_PADDING 10
#define PKGI_MAIN_HLINE_EXTRA 5
#define PKGI_MAIN_ROW_PADDING 2
#define PKGI_MAIN_HLINE_HEIGHT 2
#define PKGI_MAIN_TEXT_PADDING 5
#define PKGI_MAIN_SCROLL_WIDTH 2
#define PKGI_MAIN_SCROLL_PADDING 2
#define PKGI_MAIN_SCROLL_MIN_HEIGHT 50

#define PKGI_DIALOG_HMARGIN 100
#define PKGI_DIALOG_VMARGIN 150
#define PKGI_DIALOG_PADDING 30
#define PKGI_DIALOG_WIDTH (VITA_WIDTH - 2 * PKGI_DIALOG_HMARGIN)
#define PKGI_DIALOG_HEIGHT (VITA_HEIGHT - 2 * PKGI_DIALOG_VMARGIN)

#define PKGI_DIALOG_PROCESS_BAR_HEIGHT 10
#define PKGI_DIALOG_PROCESS_BAR_PADDING 10
#define PKGI_DIALOG_PROCESS_BAR_CHUNK 200

#define PKGI_MENU_WIDTH 300
#define PKGI_MENU_LEFT_PADDING 20
#define PKGI_MENU_TOP_PADDING 5
