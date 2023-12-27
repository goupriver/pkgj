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

#define PKGI_UTF8_B "Байт"
#define PKGI_UTF8_KB "КБ" // "\xe3\x8e\x85" // 0x3385
#define PKGI_UTF8_MB "МБ" // "\xe3\x8e\x86" // 0x3386
#define PKGI_UTF8_GB "ГБ" // "\xe3\x8e\x87" // 0x3387

#define PKGI_UTF8_CLEAR "\xc3\x97" // 0x00d7

#define PKGI_UTF8_SORT_ASC "\xe2\x96\xb2" // 0x25b2
#define PKGI_UTF8_SORT_DESC "\xe2\x96\xbc" // 0x25bc

#define PKGI_UTF8_CHECK_ON "\xe2\x97\x8f" // 0x25cf
#define PKGI_UTF8_CHECK_OFF "\xe2\x97\x8b" // 0x25cb

#define PKGI_COLOR_DIALOG_BACKGROUND PKGI_COLOR(48, 48, 48)
// при открытии где фильтры и сортировка [styler] menu.cpp
// #define PKGI_COLOR_MENU_BACKGROUND PKGI_COLOR(48, 48, 48)
#define PKGI_COLOR_MENU_BACKGROUND PKGI_COLOR(13, 13, 13)
// #define PKGI_COLOR_TEXT_MENU PKGI_COLOR(255, 255, 255)

// текст выбранный в фильтрах и сортировке [styler] menu.cpp
#define PKGI_COLOR_TEXT_MENU_SELECTED PKGI_COLOR(250, 221, 0)
#define PKGI_COLOR_TEXT_MENU PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_TEXT PKGI_COLOR(240, 240, 240)
#define PKGI_COLOR_DATE_TIME PKGI_COLOR(174, 174, 174)

// текст общий
// #define PKGI_COLOR_TEXT PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_HEAD PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_TAIL PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_DIALOG PKGI_COLOR(255, 255, 255)
#define PKGI_COLOR_TEXT_ERROR PKGI_COLOR(255, 50, 50)

// полоска сверху и снизу
#define PKGI_COLOR_HLINE PKGI_COLOR(0, 0, 0)
// #define PKGI_COLOR_HLINE PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_SCROLL_BAR PKGI_COLOR(200, 200, 200)
#define PKGI_COLOR_SCROLL_BAR_BACKGROUND PKGI_COLOR(23, 23, 23)
#define PKGI_COLOR_BATTERY_LOW PKGI_COLOR(255, 50, 50)
#define PKGI_COLOR_BATTERY_CHARGING PKGI_COLOR(50, 255, 50)
// прорзраный бегунок в выборе игры
// #define PKGI_COLOR_SELECTED_BACKGROUND PKGI_COLOR(100, 100, 100)
#define PKGI_COLOR_PROGRESS_BACKGROUND PKGI_COLOR(100, 100, 100)
#define PKGI_COLOR_PROGRESS_BAR PKGI_COLOR(128, 255, 0)


// button and window
#define PKGI_COLOR_BUTTON ImVec4(0.02f, 0.086f, 0.361f, 1.00f)
#define PKGI_COLOR_BUTTON_ACTIVE_HOVERED ImVec4(0.016f, 0.067f, 0.286f, 1.00f)
#define PKGI_COLOR_WINDOW_TITLE ImVec4(0.02f, 0.086f, 0.361f, 1.00f)

// #define PKGI_COLOR_BUTTON PKGI_COLOR(200, 200, 200)
// #define PKGI_COLOR_BUTTON_ACTIVE PKGI_COLOR(200, 200, 200)

// 0.051
#define PKGI_COLOR_WINDOW_BG ImVec4(0.051f, 0.051f, 0.051f, 1.00f)
#define PKGI_COLOR_WINDOW_BG_CHILD ImVec4(0.051f, 0.051f, 0.051f, 1.00f)
#define PKGI_COLOR_POPUP_BG ImVec4(0.051f, 0.051f, 0.051f, 1.00f)

// БОРДЕР
// #define PKGI_COLOR_BORDER ImVec4(0.051f, 0.051f, 0.051f, 0.00f)
// Подсветка у кнопок , обводка, бордер
#define PKGI_COLOR_HIGHLIGHT ImVec4(0.051f, 0.051f, 0.051f, 0.00f)

//
#define PKGI_COLOR_HEAD_HLINE PKGI_COLOR(0, 0, 0)
#define PKGI_COLOR_BORDER_WINDOW ImVec4(0.00f, 0.00f, 0.00f, 1.0f)
#define PKGI_COLOR_SHADOW ImVec4(0.00f, 0.00f, 0.00f, 0.6f)

//
// текст списоку игр
#define PKGI_COLOR_LIST_COLOR PKGI_COLOR(200, 200, 200)
// #define PKGI_COLOR_LIST_TITLE_ID PKGI_COLOR(0, 145, 60)  // 0.00f, 0.569f, 0.235f, 1.00f
#define PKGI_COLOR_LIST_TITLE_ID PKGI_COLOR(52, 215, 152)  // 0.00f, 0.569f, 0.235f, 1.00f
// #define PKGI_COLOR_LIST_REGION PKGI_COLOR(178, 40, 48)
// #define PKGI_COLOR_LIST_REGION PKGI_COLOR(210, 95, 92) 
#define PKGI_COLOR_LIST_REGION PKGI_COLOR(198, 82, 82) 

// #define PKGI_COLOR_LIST_TITLE PKGI_COLOR(250, 221, 0)
#define PKGI_COLOR_LIST_TITLE PKGI_COLOR(107, 137, 201)

#define PKGI_COLOR_LIST_SIZE PKGI_COLOR(200, 126, 177)

// #define PKGI_COLOR_LIST_CIRCLE PKGI_COLOR(250, 221, 0)
#define PKGI_COLOR_LIST_CIRCLE PKGI_COLOR(107, 137, 201)

#define PKGI_COLOR_LIST_TITLE_ID_UNACTIVE PKGI_COLOR(113, 131, 140)
#define PKGI_COLOR_LIST_REGION_UNACTIVE PKGI_COLOR(113, 131, 140)
#define PKGI_COLOR_LIST_SIZE_UNACTIVE PKGI_COLOR(113, 131, 140)
#define PKGI_COLOR_LIST_CIRCLE_UNACTIVE PKGI_COLOR(200, 200, 200)



// цвет текста и прямоугольной области в списоке игр в фокусе
#define PKGI_COLOR_SELECTED_BACKGROUND PKGI_COLOR(23, 23, 23)


// цвет кнопок пс отмена зарыть открыть 

#define PKGI_COLOR_PS_VITA_BUTTON PKGI_COLOR(34, 140, 39)
#define PKGI_COLOR_PS_VITA_BUTTON_TEXT PKGI_COLOR(200, 200, 200)

// end

#define PKGI_ANIMATION_SPEED 4000 // px/second

#define PKGI_MAIN_DOWNLOAD_BAR_WIDTH 251
#define PKGI_MAIN_COLUMN_PADDING 10
#define PKGI_MAIN_HLINE_EXTRA 12  // не забывыть убрать
#define TWELAWE 12  // не забывыть убрать

// #define PKGI_MAIN_HLINE_EXTRA 0  // не забывыть убрать
#define PKGI_MAIN_ROW_PADDING 2
// #define PKGI_MAIN_HLINE_HEIGHT 2 // не забывыть убрать
#define PKGI_MAIN_HLINE_HEIGHT 0 // не забывыть убрать
#define PKGI_MAIN_TEXT_PADDING 5
#define PKGI_MAIN_SCROLL_WIDTH 10
#define PKGI_MAIN_SCROLL_PADDING 2
#define PKGI_MAIN_SCROLL_MIN_HEIGHT 50
#define PKGI_SCROLL_PADDING 28
#define PKGI_SCROLL_LEFT_MARGIN 10

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
