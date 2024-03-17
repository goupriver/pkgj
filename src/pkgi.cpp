#include "pkgi.hpp"

extern "C"
{
#include "style.h"
}
#include "bgdl.hpp"
#include "comppackdb.hpp"
#include "config.hpp"
#include "db.hpp"
#include "dialog.hpp"
#include "download.hpp"
#include "downloader.hpp"
#include "gameview.hpp"
#include "imgui.hpp"
#include "install.hpp"
#include "menu.hpp"
#include "update.hpp"
#include "utils.hpp"
#include "vitahttp.hpp"
#include "zrif.hpp"
#include "psm.hpp"
#include <cmath>

#include <typeinfo>

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <string.h>

#include <vita2d.h>

#include <fmt/format.h>

#include <memory>
#include <set>

#include <psp2common/npdrm.h>

#include <cstddef>
#include <cstring>

#define PKGI_UPDATE_URL \
    "https://api.github.com/repos/blastrock/pkgj/releases/latest"

namespace
{
typedef enum
{
    StateError,
    StateRefreshing,
    StateMain,
} State;

State state = StateMain;
Mode mode = ModeGames;

uint32_t first_item;
uint32_t selected_item;

int search_active;

Config config;
Config config_temp;

int font_height;
int avail_height;
int bottom_y;

char search_text[256];
char error_state[256];

// used for multiple things actually
Mutex refresh_mutex("refresh_mutex");
std::string current_action;
std::unique_ptr<TitleDatabase> db;
std::unique_ptr<CompPackDatabase> comppack_db_games;
std::unique_ptr<CompPackDatabase> comppack_db_updates;

std::set<std::string> installed_games;
std::set<std::string> installed_themes;

std::unique_ptr<GameView> gameview;
bool need_refresh = true;
bool runtime_install_queued = false;
std::string content_to_refresh;
void pkgi_reload();

const char* pkgi_get_ok_str(void)
{
    return pkgi_ok_button() == PKGI_BUTTON_X ? PKGI_UTF8_X : PKGI_UTF8_O;
}

const char* pkgi_get_cancel_str(void)
{
    return pkgi_cancel_button() == PKGI_BUTTON_O ? PKGI_UTF8_O : PKGI_UTF8_X;
}

Type mode_to_type(Mode mode)
{
    switch (mode)
    {
    case ModeGames:
        return Game;
    case ModeDlcs:
        return Dlc;
    case ModePsmGames:
        return PsmGame;
    case ModePsxGames:
        return PsxGame;
    case ModePspGames:
        return PspGame;
    case ModePspDlcs:
        return PspDlc;
    case ModeDemos:
    case ModeThemes:
        throw formatEx<std::runtime_error>(
                "unsupported mode {}", static_cast<int>(mode));
    }
    throw formatEx<std::runtime_error>(
            "unknown mode {}", static_cast<int>(mode));
}

BgdlType mode_to_bgdl_type(Mode mode)
{
    switch (mode)
    {
    case ModePspGames:
    case ModePsxGames:
    case ModePspDlcs:
        return BgdlTypePsp;
    case ModePsmGames:
        return BgdlTypePsm;
    case ModeGames:
    case ModeDemos:
        return BgdlTypeGame;
    case ModeDlcs:
        return BgdlTypeDlc;
    case ModeThemes:
        return BgdlTypeTheme;
    default:
        throw formatEx<std::runtime_error>(
                "unsupported bgdl mode {}", static_cast<int>(mode));
    }
}

void configure_db(TitleDatabase* db, const char* search, const Config* config)
{
    try
    {
        db->reload(
                mode,
                mode == ModeGames || mode == ModeDlcs
                        ? config->filter
                        : config->filter & ~DbFilterInstalled,
                config->sort,
                config->order,
                search ? search : "",
                installed_games);
    }
    catch (const std::exception& e)
    {
        snprintf(
                error_state,
                sizeof(error_state),
                "Список не перезагружен: %s",
                e.what());
        pkgi_dialog_error(error_state);
    }
}

std::string const& pkgi_get_url_from_mode(Mode mode)
{
    switch (mode)
    {
    case ModeGames:
        return config.games_url;
    case ModeDlcs:
        return config.dlcs_url;
    case ModeDemos:
        return config.demos_url;
    case ModeThemes:
        return config.themes_url;
    case ModePsmGames:
        return config.psm_games_url;
    case ModePspGames:
        return config.psp_games_url;
    case ModePspDlcs:
        return config.psp_dlcs_url;
    case ModePsxGames:
        return config.psx_games_url;
    }
    throw std::runtime_error(
            fmt::format("unknown mode: {}", static_cast<int>(mode)));
}

void pkgi_refresh_thread(void)
{
    LOG("starting update");
    try
    {
        auto mode_count = ModeCount + (config.comppack_url.empty() ? 0 : 2);

        ScopeProcessLock lock;
        for (int i = 0; i < ModeCount; ++i)
        {
            const auto mode = static_cast<Mode>(i);
            auto const url = pkgi_get_url_from_mode(mode);
            if (url.empty())
                continue;
            {
                std::lock_guard<Mutex> lock(refresh_mutex);
                current_action = fmt::format(
                        "Обновление {} [{}/{}]",
                        pkgi_mode_to_string(mode),
                        i + 1,
                        mode_count);
            }
            auto const http = std::make_unique<VitaHttp>();
            db->update(mode, http.get(), url);
        }
        if (!config.comppack_url.empty())
        {
            {
                std::lock_guard<Mutex> lock(refresh_mutex);
                current_action = fmt::format(
                        "Обновления пакета совместимости игр[{}/{}]",
                        mode_count - 1,
                        mode_count);
            }
            {
                auto const http = std::make_unique<VitaHttp>();
                comppack_db_games->update(
                        http.get(), config.comppack_url + "entries.txt");
            }
            {
                std::lock_guard<Mutex> lock(refresh_mutex);
                current_action = fmt::format(
                        "Обновление пакетов совместимости обновлений [{}/{}]",
                        mode_count,
                        mode_count);
            }
            {
                auto const http = std::make_unique<VitaHttp>();
                comppack_db_updates->update(
                        http.get(), config.comppack_url + "entries_patch.txt");
            }
        }
        first_item = 0;
        selected_item = 0;
        configure_db(db.get(), search_active ? search_text : NULL, &config);
    }
    catch (const std::exception& e)
    {
        snprintf(
                error_state,
                sizeof(error_state),
                "Невозможно получить список: %s",
                e.what());
        pkgi_dialog_error(error_state);
    }
    state = StateMain;
}

const char* pkgi_get_mode_partition()
{
    return mode == ModePspGames || mode == ModePspDlcs || mode == ModePsxGames
                   ? config.install_psp_psx_location.c_str()
                   : "ux0:";
}

void pkgi_refresh_installed_packages()
{
    auto games = pkgi_get_installed_games();
    installed_games.clear();
    installed_games.insert(
            std::make_move_iterator(games.begin()),
            std::make_move_iterator(games.end()));

    auto themes = pkgi_get_installed_themes();
    installed_themes.clear();
    installed_themes.insert(
            std::make_move_iterator(themes.begin()),
            std::make_move_iterator(themes.end()));
}

bool pkgi_is_installed(const char* titleid)
{
    return installed_games.find(titleid) != installed_games.end();
}

bool pkgi_theme_is_installed(std::string contentid)
{
    if (contentid.size() < 19)
        return false;

    contentid.erase(16, 3);
    contentid.erase(0, 7);
    return installed_themes.find(contentid) != installed_themes.end();
}

void do_download(Downloader& downloader, DbItem* item) {
    pkgi_start_download(downloader, *item);
    item->presence = PresenceUnknown;
}

void pkgi_install_package(Downloader& downloader, DbItem* item)
{
    std::string title_game = erase_string_elements(item->name);

    if (item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - already installed", item->content, item->name);

        gameview->render(true);
        
        // pkgi_dialog_question(
        // fmt::format(
        //         "{}уже установлено. "
        //         "Хотите переустановить?", 
        //         title_game)
        //         .c_str(),
        // {{"Да", [&downloader, item] { do_download(downloader, item); }},
        //  {"Нет", [] {} }});

        return;
    }
    
    do_download(downloader, item);
}

void pkgi_friendly_size(char* text, uint32_t textlen, int64_t size)
{
    if (size <= 0)
    {
        text[0] = 0;
    }
    else if (size < 1000LL)
    {
        pkgi_snprintf(text, textlen, "%u " PKGI_UTF8_B, (uint32_t)size);
    }
    else if (size < 1000LL * 1000)
    {
        pkgi_snprintf(text, textlen, "%.2f " PKGI_UTF8_KB, size / 1024.f);
    }
    else if (size < 1000LL * 1000 * 1000)
    {
        pkgi_snprintf(
                text, textlen, "%.2f " PKGI_UTF8_MB, size / 1024.f / 1024.f);
    }
    else
    {
        pkgi_snprintf(
                text,
                textlen,
                "%.2f " PKGI_UTF8_GB,
                size / 1024.f / 1024.f / 1024.f);
    }
}

void pkgi_set_mode(Mode set_mode)
{
    mode = set_mode;
    pkgi_reload();
    first_item = 0;
    selected_item = 0;
}

void pkgi_refresh_list()
{
    state = StateRefreshing;
    pkgi_start_thread("refresh_thread", &pkgi_refresh_thread);
}

void pkgi_do_main(Downloader& downloader, pkgi_input* input, pkgi_texture ps_ico)
{
    int col_titleid = 0;
    int col_region = col_titleid + pkgi_text_width("PCSE00000") +
                     PKGI_MAIN_COLUMN_PADDING;
    int col_installed =
            col_region + pkgi_text_width("USA") + PKGI_MAIN_COLUMN_PADDING;
    int col_name = col_installed + pkgi_text_width(PKGI_UTF8_INSTALLED) +
                   PKGI_MAIN_COLUMN_PADDING;

    uint32_t db_count = db->count();

    if (input)
    {
        if (input->active & PKGI_BUTTON_UP)
        {
            if (selected_item == first_item && first_item > 0)
            {
                first_item--;
                selected_item = first_item;
            }
            else if (selected_item > 0)
            {
                selected_item--;
            }
            else if (selected_item == 0)
            {
                selected_item = db_count - 1;
                uint32_t max_items =
                        avail_height / (font_height + PKGI_MAIN_ROW_PADDING) -
                        1;
                first_item =
                        db_count > max_items ? db_count - max_items - 1 : 0;
            }
        }

        if (input->active & PKGI_BUTTON_DOWN)
        {
            uint32_t max_items =
                    avail_height / (font_height + PKGI_MAIN_ROW_PADDING) - 1;
            if (selected_item == db_count - 1)
            {
                selected_item = first_item = 0;
            }
            else if (selected_item == first_item + max_items)
            {
                first_item++;
                selected_item++;
            }
            else
            {
                selected_item++;
            }
        }

        if (input->active & PKGI_BUTTON_LEFT)
        {
            uint32_t max_items =
                    avail_height / (font_height + PKGI_MAIN_ROW_PADDING) - 1;
            if (first_item < max_items)
            {
                first_item = 0;
            }
            else
            {
                first_item -= max_items;
            }
            if (selected_item < max_items)
            {
                selected_item = 0;
            }
            else
            {
                selected_item -= max_items;
            }
        }

        if (input->active & PKGI_BUTTON_RIGHT)
        {
            uint32_t max_items =
                    avail_height / (font_height + PKGI_MAIN_ROW_PADDING) - 1;
            if (first_item + max_items < db_count - 1)
            {
                first_item += max_items;
                selected_item += max_items;
                if (selected_item >= db_count)
                {
                    selected_item = db_count - 1;
                }
            }
        }
    }


























    // ЦЕНТР 

    int y = font_height + PKGI_MAIN_HLINE_EXTRA;  // 0px
    int line_height = font_height + PKGI_MAIN_ROW_PADDING;  // 2px


        // НЕ СМОТРЕТЬ НАЧАЛО

    for (uint32_t i = first_item; i < db_count; i++)
    {
        DbItem* item = db->get(i);

        uint32_t colorTextTitileId = PKGI_COLOR_LIST_TITLE_ID_UNACTIVE;
        uint32_t colorTextRegion = PKGI_COLOR_LIST_REGION_UNACTIVE;
        uint32_t colorTextTitile = PKGI_COLOR_LIST_COLOR;
        uint32_t colorTextSize = PKGI_COLOR_LIST_SIZE_UNACTIVE;
        uint32_t colorCircle = PKGI_COLOR_LIST_CIRCLE_UNACTIVE;

        const auto titleid = item->titleid.c_str();

        if (item->presence == PresenceUnknown)
        {
            switch (mode)
            {
            case ModeGames:
            case ModeDemos:
                if (pkgi_is_installed(titleid))
                    item->presence = PresenceInstalled;
                else if (downloader.is_in_queue(Game, item->content))
                    item->presence = PresenceInstalling;
                break;
            case ModePsmGames:
                if (pkgi_psm_is_installed(titleid))
                    item->presence = PresenceInstalled;
                else if (downloader.is_in_queue(PsmGame, item->content))
                    item->presence = PresenceInstalling;
                break;
            case ModePspDlcs:
                if (pkgi_psp_is_installed(
                            pkgi_get_mode_partition(), item->content.c_str()))
                    item->presence = PresenceGamePresent;
                else if (downloader.is_in_queue(PspGame, item->content))
                    item->presence = PresenceInstalling;
                break;
            case ModePspGames:
                if (pkgi_psp_is_installed(
                            pkgi_get_mode_partition(), item->content.c_str()))
                    item->presence = PresenceInstalled;
                else if (downloader.is_in_queue(PspGame, item->content))
                    item->presence = PresenceInstalling;
                break;
            case ModePsxGames:
                if (pkgi_psx_is_installed(
                            pkgi_get_mode_partition(), item->content.c_str()))
                    item->presence = PresenceInstalled;
                else if (downloader.is_in_queue(PsxGame, item->content))
                    item->presence = PresenceInstalling;
                break;
            case ModeDlcs:
                if (downloader.is_in_queue(Dlc, item->content))
                    item->presence = PresenceInstalling;
                else if (pkgi_dlc_is_installed(item->content.c_str()))
                    item->presence = PresenceInstalled;
                else if (pkgi_is_installed(titleid))
                    item->presence = PresenceGamePresent;
                break;
            case ModeThemes:
                if (pkgi_theme_is_installed(item->content))
                    item->presence = PresenceInstalled;
                else if (pkgi_is_installed(titleid))
                    item->presence = PresenceGamePresent;
                break;
            }

            if (item->presence == PresenceUnknown)
            {
                if (pkgi_is_incomplete(
                            pkgi_get_mode_partition(), item->content.c_str()))
                    item->presence = PresenceIncomplete;
                else
                    item->presence = PresenceMissing;
            }
        }

        // НЕ СМОТРЕТЬ КОНЕЦ

        

        char size_str[64];
        pkgi_friendly_size(size_str, sizeof(size_str), item->size);
        int sizew = pkgi_text_width(size_str);

        // pkgi_clip_set(0, y + TWELAWE, VITA_WIDTH, line_height);
        pkgi_clip_set(0, y, VITA_WIDTH, line_height);

        if (i == selected_item)
        {
            colorTextTitileId = PKGI_COLOR_LIST_TITLE_ID;
            colorTextRegion = PKGI_COLOR_LIST_REGION;
            colorTextTitile = PKGI_COLOR_LIST_TITLE;
            colorTextSize = PKGI_COLOR_LIST_SIZE;
            colorCircle = PKGI_COLOR_LIST_CIRCLE;
            
            pkgi_draw_rect(
                    0 + 26,
                    y + 3,
                    VITA_WIDTH - 34,
                    font_height + PKGI_MAIN_ROW_PADDING - 4,
                    PKGI_COLOR_SELECTED_BACKGROUND);
        }

        // id игры 
        pkgi_draw_text(col_titleid + PKGI_SCROLL_PADDING, y, colorTextTitileId, titleid);
        const char* region;
        switch (pkgi_get_region(item->titleid))
        {
        case RegionASA:
            region = "ASA";
            break;
        case RegionEUR:
            region = "EUR";
            break;
        case RegionJPN:
            region = "JPN";
            break;
        case RegionINT:
            region = "INT";
            break;
        case RegionUSA:
            region = "USA";
            break;
        default:
            region = "???";
            break;
        }

        // регион
        pkgi_draw_text(col_region + PKGI_SCROLL_PADDING, y, colorTextRegion, region);

        // кружок 
        // круг
        if (item->presence == PresenceIncomplete)
        {
            pkgi_draw_text(col_installed + PKGI_SCROLL_PADDING, y, colorCircle, PKGI_UTF8_PARTIAL);
        }
        // круг
        else if (item->presence == PresenceInstalled)
        {
            pkgi_draw_text(col_installed + PKGI_SCROLL_PADDING, y, colorCircle, PKGI_UTF8_INSTALLED);
            
            if (i == selected_item)
            {
                pkgi_draw_texture(ps_ico, col_installed + PKGI_SCROLL_PADDING + 1, y+7);
            }
            // pkgi_draw_texture(ps_ico, col_installed + PKGI_SCROLL_PADDING, y);
        }

        // круг
        else if (item->presence == PresenceGamePresent)
        {
            pkgi_draw_text(
                    col_installed + PKGI_SCROLL_PADDING,
                    y,
                    colorCircle,
                    PKGI_UTF8_INSTALLED);
        }
        else if (item->presence == PresenceInstalling)
        // круг
        {
            pkgi_draw_text(col_installed + PKGI_SCROLL_PADDING, y, colorCircle, PKGI_UTF8_INSTALLING);
        }
        // размер файла
        pkgi_draw_text(
                VITA_WIDTH - PKGI_MAIN_SCROLL_WIDTH - PKGI_MAIN_SCROLL_PADDING -
                        sizew,
                y,
                colorTextSize,
                size_str);
        pkgi_clip_remove();

        pkgi_clip_set(
                col_name + PKGI_SCROLL_PADDING,
                // y + TWELAWE,
                y,
                VITA_WIDTH - PKGI_MAIN_SCROLL_WIDTH - PKGI_MAIN_SCROLL_PADDING -
                        PKGI_MAIN_COLUMN_PADDING - sizew - col_name - 59,
                // line_height + TWELAWE);
                line_height);

        // текст
        // char elem = '(';
        // int len = item->name.length() + 1;
        // int indexStart = item->name.find(elem);
        // std::string fin = item->name.erase(indexStart, len);
        // pkgi_draw_text(col_name + PKGI_SCROLL_PADDING, y, colorTextTitile, fin.c_str());


        std::string title_game = mode == ModeGames ? erase_string_elements(item->name) : item->name;

        // if ()
            // title_game = ;
        // else
            // title_game = ;

        // pkgi_draw_text(160, 0, colorTextTitile, typeid(item->name).name());

        pkgi_draw_text(col_name + PKGI_SCROLL_PADDING, y, colorTextTitile, title_game.c_str());


        pkgi_clip_remove();

        y += font_height + PKGI_MAIN_ROW_PADDING;
        if (y > VITA_HEIGHT - (2 * font_height + PKGI_MAIN_HLINE_EXTRA))
        {
            break;
        }
        else if (
                y + font_height >
                VITA_HEIGHT - (2 * font_height + PKGI_MAIN_HLINE_EXTRA))
        {
            line_height =
                    (VITA_HEIGHT - (2 * font_height + PKGI_MAIN_HLINE_EXTRA)) -
                    (y + 1);
            if (line_height < PKGI_MAIN_ROW_PADDING)
            {
                break;
            }
        }
    }

    if (db_count == 0)
    {
        const char* text = "Список пуст! Попробуйте обновить.";

        int w = pkgi_text_width(text);
        pkgi_draw_text(
                (VITA_WIDTH - w) / 2, VITA_HEIGHT / 2, PKGI_COLOR_TEXT, text);
    }


    // scroll-bar
    pkgi_draw_rect(
                    10,
                    43,
                    8,
                    VITA_HEIGHT - 108,
                    PKGI_COLOR_SCROLL_BAR_BACKGROUND);
    
    if (db_count != 0)
    {


        uint32_t max_items =
        // max_items = 17, 52
                (avail_height + font_height + PKGI_MAIN_ROW_PADDING - 1) /
                        (font_height + PKGI_MAIN_ROW_PADDING) - 1;
        if (max_items < db_count)
        {   
            // 50
            uint32_t min_height = PKGI_MAIN_SCROLL_MIN_HEIGHT;
            // 3.7
            uint32_t height = max_items * avail_height / db_count;
            // 1 * 389 / 2015 = 0,2  (4)   (147)
            uint32_t start =
                    first_item *
                    (avail_height - (height < min_height ? min_height : 0)) /
                    db_count;
            height = max32(height, min_height);

            // // 2015
            // pkgi_draw_text(0, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("dc {}", db_count).c_str());
            // // 50
            // pkgi_draw_text(120, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("mh {}", min_height).c_str());
            // // 10
            // pkgi_draw_text(240, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("mi {}", max_items).c_str());
            // // 50
            // pkgi_draw_text(360, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("h {}", height).c_str());
            // // 0 (каждые 6 строк)
            // pkgi_draw_text(480, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("s {}", start).c_str());
            // // 439
            // pkgi_draw_text(600, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("ah {}", avail_height).c_str());
            // // 0 . после первого перелистывания начинает меняться
            // pkgi_draw_text(720, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("fi {}", first_item).c_str());
            // // 23
            // pkgi_draw_text(840, 0, PKGI_COLOR_SCROLL_BAR, fmt::format("fh {}", font_height).c_str());


            pkgi_draw_rect(
                    PKGI_SCROLL_LEFT_MARGIN - 1,
                    // 23 + 12 + 0.2 = 35.2
                    // надо 40
                    // 
                    font_height + PKGI_MAIN_HLINE_EXTRA + 8 + start,
                    // 
                    PKGI_MAIN_SCROLL_WIDTH,
                    height,
                    PKGI_COLOR_SCROLL_BAR);
        }
    }




























    if (input && (input->pressed & pkgi_ok_button()))
    {
        input->pressed &= ~pkgi_ok_button();

        if (selected_item >= db->count())
            return;
        DbItem* item = db->get(selected_item);

        if (mode == ModeGames)
            gameview = std::make_unique<GameView>(
                    &config,
                    &downloader,
                    item,
                    comppack_db_games->get(item->titleid),
                    comppack_db_updates->get(item->titleid));
        else if (mode == ModeThemes || mode == ModeDemos)
        {
            pkgi_start_download(downloader, *item);
        }
        else
        {
            if (downloader.is_in_queue(mode_to_type(mode), item->content))
            {
                downloader.remove_from_queue(mode_to_type(mode), item->content);
                item->presence = PresenceUnknown;
            }
            else
                pkgi_install_package(downloader, item);
        }
    }
    else if (input && (input->pressed & PKGI_BUTTON_T))
    {
        input->pressed &= ~PKGI_BUTTON_T;

        config_temp = config;
        int allow_refresh = !config.games_url.empty() << 0 |
                            !config.dlcs_url.empty() << 1 |
                            !config.demos_url.empty() << 6 |
                            !config.themes_url.empty() << 5 |
                            !config.psx_games_url.empty() << 2 |
                            !config.psp_games_url.empty() << 3 |
                            !config.psp_dlcs_url.empty() << 7 |
                            !config.psm_games_url.empty() << 4;
        pkgi_menu_start(search_active, &config, allow_refresh);
    }
}

void pkgi_do_refresh(void)
{
    std::string text;

    uint32_t updated;
    uint32_t total;
    db->get_update_status(&updated, &total);

    if (total == 0)
        text = fmt::format("{}...", current_action);
    else
        text = fmt::format("{}... {}%", current_action, updated * 100 / total);

    int w = pkgi_text_width(text.c_str());
    pkgi_draw_text(
            (VITA_WIDTH - w) / 2,
            VITA_HEIGHT / 2,
            PKGI_COLOR_TEXT,
            text.c_str());
}

void pkgi_do_head(auto ffont)
{
    ImGui::PushFont(ffont);
    const char* version = PKGI_VERSION;

    char title[256];
    pkgi_snprintf(title, sizeof(title), "PKGj v%s ", version);


    pkgi_draw_text(9, 0, PKGI_COLOR_DATE_TIME, title);


    // int rightw;
    int rightw = 22;
    // if (pkgi_battery_present())
    // {
        // char battery[256];
        // pkgi_snprintf(
        //         battery,
        //         sizeof(battery),
        //         "Аккумулятор: %u%%",
        //         pkgi_bettery_get_level());  

        // uint32_t color;
        // if (pkgi_battery_is_low())
        // {
        //     color = PKGI_COLOR_BATTERY_LOW;
        // }
        // else if (pkgi_battery_is_charging())
        // {
        //     color = PKGI_COLOR_BATTERY_CHARGING;
        // }
        // else
        // {
        //     color = PKGI_COLOR_TEXT_HEAD;
        // }

        // rightw = pkgi_text_width(battery);
        // pkgi_draw_text(
                // VITA_WIDTH - PKGI_MAIN_HLINE_EXTRA - rightw, 0, color, battery);
    // }
    // else
    // {
        // rightw = 0;
    // }

    ImGui::PopFont();


    char text[256];
    int left = pkgi_text_width(search_text) + PKGI_MAIN_TEXT_PADDING;
    int right = rightw + PKGI_MAIN_TEXT_PADDING;

    if (search_active)
        pkgi_snprintf(
                text,
                sizeof(text),
                "%s >> %s <<",
                pkgi_mode_to_string(mode).c_str(),
                search_text);
    else
        pkgi_snprintf(
                text, sizeof(text), "%s", pkgi_mode_to_string(mode).c_str());

    pkgi_clip_set(
            left,
            0,
            VITA_WIDTH - right - left,
            font_height + PKGI_MAIN_HLINE_EXTRA);
            // ДЛЯ ТЕСТА
    pkgi_draw_text(
            (VITA_WIDTH - pkgi_text_width(text)) / 2,
            0,
            PKGI_COLOR_TEXT_TAIL,
            text);
    pkgi_clip_remove();
}

uint64_t last_progress_time;
uint64_t last_progress_offset;
uint64_t last_progress_speed;

uint64_t get_speed(const uint64_t download_offset)
{
    const uint64_t now = pkgi_time_msec();
    const uint64_t progress_time = now - last_progress_time;
    if (progress_time < 1000)
        return last_progress_speed;

    const uint64_t progress_data = download_offset - last_progress_offset;
    last_progress_speed = progress_data * 1000 / progress_time;
    last_progress_offset = download_offset;
    last_progress_time = now;

    return last_progress_speed;
}


// НИЗ
void pkgi_do_tail(Downloader& downloader, pkgi_texture memoryCard, pkgi_texture btn_x, pkgi_texture btn_square, pkgi_texture btn_triangle, pkgi_texture btn_circle)
{

    // ЗАГРУЗКА НАЧАЛО

    char text[256];

    const auto current_download = downloader.get_current_download();

    uint64_t download_offset;
    uint64_t download_size;
    std::tie(download_offset, download_size) =
            downloader.get_current_download_progress();
    // avoid divide by 0
    if (download_size == 0)
        download_size = 1;


    
        // подложка
    pkgi_draw_rect(
            9+278,
            bottom_y + 5 + 2,
            PKGI_MAIN_DOWNLOAD_BAR_WIDTH,
            font_height - 10,
            // PKGI_COLOR_HEAD_HLINE);
            PKGI_DOWNLOAD_BAR_BACKGROUND);
            // PKGI_COLOR_DATE_TIME);

    // на подложку
    pkgi_draw_rect(
            11+278,
            bottom_y + 7 + 2,
            PKGI_MAIN_DOWNLOAD_BAR_WIDTH - 4,
            font_height - 14,
            // PKGI_DOWNLOAD_BAR_BACKGROUND);
            PKGI_COLOR_HEAD_HLINE);

    // 
    if (current_download)
    {
        // const auto speed = get_speed(download_offset);
        // std::string sspeed;

        // if (speed > 1000 * 1024)
            // sspeed = fmt::format("{:.3g} МБ/с", speed / 1024.f / 1024.f);
        // else if (speed > 1000)
            // sspeed = fmt::format("{:.3g} КБ/с", speed / 1024.f);
        // else
            // sspeed = fmt::format("{} Байт/с", speed);

        // прогрессбар
        pkgi_draw_rect(
                13+278,
                bottom_y + 10,
                (PKGI_MAIN_DOWNLOAD_BAR_WIDTH - 8) * download_offset / download_size,
                font_height - 18 + 2,
                PKGI_COLOR_DATE_TIME);
                // PKGI_COLOR_PROGRESS_BACKGROUND);

        pkgi_snprintf(
                text,
                sizeof(text),
                // "Загрузка %s: %s (%s, %d%%)",
                "%d%%",
                static_cast<int>(download_offset * 100 / download_size));
    }
    else
        pkgi_snprintf(text, sizeof(text), "");

    // pkgi_draw_text((PKGI_MAIN_DOWNLOAD_BAR_WIDTH + 10 - pkgi_text_width(text))/2, bottom_y - 2, PKGI_COLOR_TEXT_TAIL, text);
    
    pkgi_draw_text_with_size(
            VITA_WIDTH/2 - pkgi_text_width(text)/2 + 7,
            bottom_y - 2,
            0.700f,
            PKGI_COLOR_TEXT_TAIL,
            text);

    // ЗАГРУЗКА КОНЕЦ

















    const auto second_line = bottom_y + font_height + PKGI_MAIN_ROW_PADDING;

    // СЧЕТЧИК НАЧАЛО

    // uint32_t count = db->count();
    // uint32_t total = db->total();

    // if (count == total)
    // {
    //     pkgi_snprintf(text, sizeof(text), "Показано: %u", count);
    // }
    // else
    // {
    //     pkgi_snprintf(text, sizeof(text), "Показано: %u из %u", count, total);
    // }
    // pkgi_draw_text(9, second_line, PKGI_COLOR_TEXT_TAIL, text);

    // // СЧЕТЧИК КОНЕЦ

    // get free space of partition only if looking at psx or psp games else show
    // ux0:


    // НАЧАЛО СВОБОДНО ДИСК
    
    char size[64];
    if (mode == ModePsxGames || mode == ModePspGames)
    {
        pkgi_friendly_size(
                size,
                sizeof(size),
                pkgi_get_free_space(pkgi_get_mode_partition()));
    }
    else
    {
        pkgi_friendly_size(size, sizeof(size), pkgi_get_free_space("ux0:"));
    }

    char free[64];
    pkgi_snprintf(free, sizeof(free), "%s", size);
    // pkgi_snprintf(free, sizeof(free), "Свободно: %s", size);
    // char hello3[]{"1.28 ГБ"};

    int rightw = pkgi_text_width(free);
    pkgi_draw_text(
            VITA_WIDTH - 12 - pkgi_text_width(free), 
            second_line - 2,
            PKGI_COLOR_TEXT_TAIL,
            free);


    pkgi_draw_texture(memoryCard, VITA_WIDTH - 12 - 16 - 2 - 16 - pkgi_text_width(free), VITA_HEIGHT - 20);


    // КОНЕЦ СВОБОДНО ДИСК

    int left = pkgi_text_width(text) + PKGI_MAIN_TEXT_PADDING;
    int right = rightw + PKGI_MAIN_TEXT_PADDING;

    // КНОПКИ НИЖНИЕ НАЧАЛО

    // btn_x, 
    // btn_square, 
    // btn_triangle, 
    // btn_circle

    // pkgi_draw_texture(memoryCard, VITA_WIDTH - 12 - 16 - 2 - 16 - pkgi_text_width(free), VITA_HEIGHT - 20);


    pkgi_clip_set(
            left,
            second_line - 2,
            VITA_WIDTH - right - left,
            VITA_HEIGHT - second_line);

    // std::string bottom_text;

    if (gameview || pkgi_dialog_is_open())
    {
        std::string bottom_text_x = "Выбор";
        std::string bottom_text_circle = "Отмена";

        int width_all_buttons = pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_circle.c_str()) + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3;

        pkgi_draw_texture(btn_x, 
            VITA_WIDTH/2 - width_all_buttons/2, 
            second_line + 5);

        pkgi_draw_text(
            VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
            second_line - 2,
            PKGI_COLOR_TEXT_TAIL,
            bottom_text_x.c_str());

        pkgi_draw_texture(btn_circle, 
            VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING * 2 + pkgi_text_width(bottom_text_x.c_str()),
            second_line + 5);

        pkgi_draw_text(
            VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3 + pkgi_text_width(bottom_text_x.c_str()),
            second_line - 2,
            PKGI_COLOR_TEXT_TAIL,
            bottom_text_circle.c_str());
    }
    else if (pkgi_menu_is_open())
    {

            std::string bottom_text_x = "Выбор";
            std::string bottom_text_triangle = "Сохранить";
            std::string bottom_text_square = "Отмена";

            int width_all_buttons = pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()) + pkgi_text_width(bottom_text_square.c_str()) + PKGI_MAIN_BTN_WIDTH * 3 + PKGI_MAIN_BTN_PADDING * 5;

            pkgi_draw_texture(btn_x, 
                VITA_WIDTH/2 - width_all_buttons/2, 
                second_line + 5);

            pkgi_draw_text(
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
                second_line - 2,
                PKGI_COLOR_TEXT_TAIL,
                bottom_text_x.c_str());

            pkgi_draw_texture(btn_triangle, 
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING * 2 + pkgi_text_width(bottom_text_x.c_str()),
                second_line + 5);

            pkgi_draw_text(
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3 + pkgi_text_width(bottom_text_x.c_str()),
                second_line - 2,
                PKGI_COLOR_TEXT_TAIL,
                bottom_text_triangle.c_str());

            pkgi_draw_texture(btn_circle, 
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 4 + pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()),
                second_line + 5);

            pkgi_draw_text(
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 3 + PKGI_MAIN_BTN_PADDING * 5 + pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()),
                second_line - 2,
                PKGI_COLOR_TEXT_TAIL,
                bottom_text_square.c_str()); 
    }
    else
    {
        if (mode == ModeGames) 
        {

            std::string bottom_text_x = "Просмотр";
            std::string bottom_text_triangle = "Меню";

            int width_all_buttons = pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()) + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3;

            pkgi_draw_texture(btn_x, 
                VITA_WIDTH/2 - width_all_buttons/2, 
                second_line + 5);

            pkgi_draw_text(
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
                second_line - 2,
                PKGI_COLOR_TEXT_TAIL,
                bottom_text_x.c_str());

            pkgi_draw_texture(btn_triangle, 
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING * 2 + pkgi_text_width(bottom_text_x.c_str()),
                second_line + 5);

            pkgi_draw_text(
                VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3 + pkgi_text_width(bottom_text_x.c_str()),
                second_line - 2,
                PKGI_COLOR_TEXT_TAIL,
                bottom_text_triangle.c_str());
        }
        else
        {
            DbItem* item = db->get(selected_item);
            if (item && item->presence == PresenceInstalling)
            {

                std::string bottom_text_circle = "Отмена";
                std::string bottom_text_triangle = "Меню";

                int width_all_buttons = pkgi_text_width(bottom_text_circle.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()) + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3;

                pkgi_draw_texture(btn_x, 
                    VITA_WIDTH/2 - width_all_buttons/2, 
                    second_line + 5);

                pkgi_draw_text(
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
                    second_line - 2,
                    PKGI_COLOR_TEXT_TAIL,
                    bottom_text_circle.c_str());

                pkgi_draw_texture(btn_triangle, 
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING * 2 + pkgi_text_width(bottom_text_circle.c_str()),
                    second_line + 5);

                pkgi_draw_text(
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3 + pkgi_text_width(bottom_text_circle.c_str()),
                    second_line - 2,
                    PKGI_COLOR_TEXT_TAIL,
                    bottom_text_triangle.c_str());
            }
            else if (item && item->presence != PresenceInstalled)
            {
                std::string bottom_text_x = "Установить";
                std::string bottom_text_triangle = "Меню";

                int width_all_buttons = pkgi_text_width(bottom_text_x.c_str()) + pkgi_text_width(bottom_text_triangle.c_str()) + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3;

                pkgi_draw_texture(btn_x, 
                    VITA_WIDTH/2 - width_all_buttons/2, 
                    second_line + 5);

                pkgi_draw_text(
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
                    second_line - 2,
                    PKGI_COLOR_TEXT_TAIL,
                    bottom_text_x.c_str());

                pkgi_draw_texture(btn_triangle, 
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING * 2 + pkgi_text_width(bottom_text_x.c_str()),
                    second_line + 5);

                pkgi_draw_text(
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH * 2 + PKGI_MAIN_BTN_PADDING * 3 + pkgi_text_width(bottom_text_x.c_str()),
                    second_line - 2,
                    PKGI_COLOR_TEXT_TAIL,
                    bottom_text_triangle.c_str());
            }

            std::string bottom_text_triangle = "Меню";

            int width_all_buttons = pkgi_text_width(bottom_text_triangle.c_str()) + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING;

            if (item && item->presence == PresenceInstalled)
            {
                pkgi_draw_texture(btn_triangle, 
                    VITA_WIDTH/2 - width_all_buttons/2, 
                    second_line + 5);

                pkgi_draw_text(
                    VITA_WIDTH/2 - width_all_buttons/2 + PKGI_MAIN_BTN_WIDTH + PKGI_MAIN_BTN_PADDING,
                    second_line - 2,
                    PKGI_COLOR_TEXT_TAIL,
                    bottom_text_triangle.c_str());
            }
        }
    }

    // pkgi_draw_text(
    //         (VITA_WIDTH - pkgi_text_width(bottom_text.c_str())) / 2,
    //         second_line - 2,
    //         PKGI_COLOR_TEXT_TAIL,
    //         bottom_text.c_str());
    pkgi_clip_remove();




    // std::string bottom_text;
    // if (gameview || pkgi_dialog_is_open())
    // {
    //     bottom_text = fmt::format(
    //             "{} Выбор {} Отмена", pkgi_get_ok_str(), pkgi_get_cancel_str());
    // }
    // else if (pkgi_menu_is_open())
    // {
    //     bottom_text = fmt::format(
    //             "{} Выбор  " PKGI_UTF8_T " Сохранить  {} Отмена",
    //             pkgi_get_ok_str(),
    //             pkgi_get_cancel_str());
    // }
    // else
    // {
    //     if (mode == ModeGames)
    //         bottom_text += fmt::format("{} Просмотр ", pkgi_get_ok_str());
    //     else
    //     {
    //         DbItem* item = db->get(selected_item);
    //         if (item && item->presence == PresenceInstalling)
    //             bottom_text += fmt::format("{} Отмена ", pkgi_get_ok_str());
    //         else if (item && item->presence != PresenceInstalled)
    //             bottom_text += fmt::format("{} Установить ", pkgi_get_ok_str());
    //     }
    //     bottom_text += PKGI_UTF8_T " Меню";
    // }

    // pkgi_clip_set(
    //         left,
    //         second_line - 2,
    //         VITA_WIDTH - right - left,
    //         VITA_HEIGHT - second_line);
    // pkgi_draw_text(
    //         (VITA_WIDTH - pkgi_text_width(bottom_text.c_str())) / 2,
    //         second_line - 2,
    //         PKGI_COLOR_TEXT_TAIL,
    //         bottom_text.c_str());
    // pkgi_clip_remove();

    // КОНЕЦ КНОПКИ
}

void pkgi_do_error(void)
{
    pkgi_draw_text(
            (VITA_WIDTH - pkgi_text_width(error_state)) / 2,
            VITA_HEIGHT / 2,
            PKGI_COLOR_TEXT_ERROR,
            error_state);
}

void reposition(void)
{
    uint32_t count = db->count();
    if (first_item + selected_item < count)
    {
        return;
    }

    uint32_t max_items = (avail_height + font_height + PKGI_MAIN_ROW_PADDING -
                          1) / (font_height + PKGI_MAIN_ROW_PADDING) -
                         1;
    if (count > max_items)
    {
        uint32_t delta = selected_item - first_item;
        first_item = count - max_items;
        selected_item = first_item + delta;
    }
    else
    {
        first_item = 0;
        selected_item = 0;
    }
}

void pkgi_reload()
{
    try
    {
        configure_db(db.get(), search_active ? search_text : NULL, &config);
    }
    catch (const std::exception& e)
    {
        LOGF("error during reload: {}", e.what());
        pkgi_dialog_error(
                fmt::format(
                        "не удалось перезагрузить базу данных: попробуйте обновить", e.what())
                        .c_str());
    }
}

void pkgi_open_db()
{
    try
    {
        first_item = 0;
        selected_item = 0;
        db = std::make_unique<TitleDatabase>(pkgi_get_config_folder());

        comppack_db_games = std::make_unique<CompPackDatabase>(
                std::string(pkgi_get_config_folder()) + "/comppack.db");
        comppack_db_updates = std::make_unique<CompPackDatabase>(
                std::string(pkgi_get_config_folder()) + "/comppack_updates.db");
    }
    catch (const std::exception& e)
    {
        LOGF("error during database open: {}", e.what());
        throw formatEx<std::runtime_error>(
                "DB initialization error: %s\nTry to delete them?");
    }

    pkgi_reload();
}
}

void pkgi_create_psp_rif(std::string contentid, uint8_t* rif)
{
    SceNpDrmLicense license;
    memset(&license, 0x00, sizeof(SceNpDrmLicense));
    license.account_id = 0x0123456789ABCDEFLL;
    memset(license.ecdsa_signature, 0xFF, 0x28);
    strncpy(license.content_id, contentid.c_str(), 0x30);

    memcpy(rif, &license, PKGI_PSP_RIF_SIZE);
}


void pkgi_download_psm_runtime_if_needed() {
    if(!pkgi_is_installed("PCSI00011") && !runtime_install_queued) {
        
        uint8_t rif[PKGI_PSM_RIF_SIZE];
        char message[256];
        pkgi_zrif_decode(PSM_RUNTIME_DRMFREE_LICENSE, rif, message, sizeof(message));
        
        pkgi_start_bgdl(
            BgdlTypeGame,
            "PlayStation Mobile Runtime Package",
            "http://ares.dl.playstation.net/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000.pkg",
            std::vector<uint8_t>(rif, rif + PKGI_PSM_RIF_SIZE));
            
        runtime_install_queued = true;
    }
}


void pkgi_start_download(Downloader& downloader, const DbItem& item)
{
    LOGF("[{}] {} - starting to install", item.content, item.name);

    try
    {
        // download PSM Runtime if a PSM game is requested to be installed ..
        if(mode == ModePsmGames)
            pkgi_download_psm_runtime_if_needed();
        // Just use the maximum size to be safe
        uint8_t rif[PKGI_PSM_RIF_SIZE];
        char message[256];
        if (item.zrif.empty() ||
            pkgi_zrif_decode(item.zrif.c_str(), rif, message, sizeof(message)))
        {
            if ( 
                mode == ModeGames || mode == ModeDlcs || mode == ModeDemos || mode == ModeThemes || // Vita contents
                (MODE_IS_PSPEMU(mode) && pkgi_is_module_present("NoPspEmuDrm_kern")) || // Psp Contents
                (mode == ModePsmGames && pkgi_is_module_present("NoPsmDrm")) // Psm Contents
            )
            {

                if (MODE_IS_PSPEMU(mode)) {
                    pkgi_create_psp_rif(item.content, rif);
                }
                
                pkgi_start_bgdl(
                        mode_to_bgdl_type(mode),
                        item.name,
                        item.url,
                        std::vector<uint8_t>(rif, rif + PKGI_PSM_RIF_SIZE));
                pkgi_dialog_message(
                        fmt::format(
                                "Установка {} поставлена в очередь в LiveArea",
                                item.name)
                                .c_str());
            }
            else {
                downloader.add(DownloadItem{
                        mode_to_type(mode),
                        item.name,
                        item.content,
                        item.url,
                        item.zrif.empty()
                                ? std::vector<uint8_t>{}
                                : std::vector<uint8_t>(
                                          rif, rif + PKGI_PSM_RIF_SIZE),
                        item.has_digest ? std::vector<uint8_t>(
                                                  item.digest.begin(),
                                                  item.digest.end())
                                        : std::vector<uint8_t>{},
                        !config.install_psp_as_pbp,
                        pkgi_get_mode_partition(),
                        ""});
            }
        }
        else
        {
            pkgi_dialog_error(message);
        }
    }
    catch (const std::exception& e)
    {
        pkgi_dialog_error(
                fmt::format("Ошибка установки {}: {}", item.name, e.what())
                        .c_str());
    }
}

int main()
{
    pkgi_start();

    try
    {
        if (!pkgi_is_unsafe_mode())
            throw std::runtime_error(
                    "PKGj requires unsafe mode to be enabled in HENkaku "
                    "settings!");

        Downloader downloader;

        downloader.refresh = [](const std::string& content)
        {
            std::lock_guard<Mutex> lock(refresh_mutex);
            content_to_refresh = content;
            need_refresh = true;
        };
        downloader.error = [](const std::string& error)
        {
            // FIXME this runs on the wrong thread
            pkgi_dialog_error(("Ошибка загрузки: " + error).c_str());
        };

        LOG("started");

        config = pkgi_load_config();
        pkgi_dialog_init();

        font_height = pkgi_text_height("M");
        avail_height = VITA_HEIGHT - 3 * (font_height + PKGI_MAIN_HLINE_EXTRA);
        bottom_y = VITA_HEIGHT - 2 * font_height - PKGI_MAIN_ROW_PADDING;

        pkgi_open_db();

        pkgi_texture background = pkgi_load_png(background);
        pkgi_texture batteryislow = pkgi_load_png(batteryislow);
        pkgi_texture batterynormal = pkgi_load_png(batterynormal);
        pkgi_texture batteryischarging = pkgi_load_png(batteryischarging);
        pkgi_texture notfill = pkgi_load_png(notfill);
        pkgi_texture btn_x = pkgi_load_png(btn_x);
        pkgi_texture btn_square = pkgi_load_png(btn_square);
        pkgi_texture btn_triangle = pkgi_load_png(btn_triangle);
        pkgi_texture btn_circle = pkgi_load_png(btn_circle);
        pkgi_texture ps_ico = pkgi_load_png(ps_ico);

        if (!config.no_version_check)
            start_update_thread();

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        // Build and load the texture atlas into a texture
        uint32_t* pixels = NULL;
        int width, height;

        ImFont* fontCyrillic = io.Fonts->AddFontFromFileTTF("sa0:/data/font/pvf/ltn0.pvf", 20.0f, 0, io.Fonts->GetGlyphRangesCyrillic());
        ImFont* fontDefault = io.Fonts->AddFontFromFileTTF("sa0:/data/font/pvf/ltn0.pvf", 20.0f, 0, io.Fonts->GetGlyphRangesDefault());
        ImFont* fontCyrillicMin = io.Fonts->AddFontFromFileTTF("sa0:/data/font/pvf/ltn0.pvf", 8.0f, 0, io.Fonts->GetGlyphRangesCyrillic());

        if (!fontCyrillic)
            throw std::runtime_error("failed to load ltn0.pvf");
        if (!fontDefault)
            throw std::runtime_error("failed to load ltn0.pvf");
        io.Fonts->GetTexDataAsRGBA32((uint8_t**)&pixels, &width, &height);
        vita2d_texture* font_texture =
                vita2d_create_empty_texture(width, height);
        const auto stride = vita2d_texture_get_stride(font_texture) / 4;
        auto texture_data = (uint32_t*)vita2d_texture_get_datap(font_texture);

        for (auto y = 0; y < height; ++y)
            for (auto x = 0; x < width; ++x)
                texture_data[y * stride + x] = pixels[y * width + x];

        io.Fonts->TexID = font_texture;

        // подключене своего стиля
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Button] = PKGI_COLOR_BUTTON;
        style.Colors[ImGuiCol_ButtonHovered] = PKGI_COLOR_BUTTON_ACTIVE_HOVERED;
        style.Colors[ImGuiCol_ButtonActive] = PKGI_COLOR_BUTTON_ACTIVE_HOVERED;

        // Строка заголовка при фокусе
        style.Colors[ImGuiCol_TitleBg] = PKGI_COLOR_WINDOW_TITLE;
        style.Colors[ImGuiCol_TitleBgActive] = PKGI_COLOR_WINDOW_TITLE;
        style.Colors[ImGuiCol_TitleBgCollapsed] = PKGI_COLOR_WINDOW_TITLE;

        // ФОн окна и всплывахи
        style.Colors[ImGuiCol_WindowBg] = PKGI_COLOR_WINDOW_BG;
        style.Colors[ImGuiCol_ChildBg] = PKGI_COLOR_WINDOW_BG_CHILD;
        style.Colors[ImGuiCol_PopupBg] = PKGI_COLOR_POPUP_BG;
        style.Colors[ImGuiCol_NavHighlight] = PKGI_COLOR_HIGHLIGHT;

        style.Colors[ImGuiCol_Border] = PKGI_COLOR_BORDER_WINDOW;
        style.Colors[ImGuiCol_BorderShadow] = PKGI_COLOR_BORDER_WINDOW;
        style.Colors[ImGuiCol_FrameBg] = PKGI_COLOR_BORDER_WINDOW;


        // style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        // style.Colors[ImGuiCol_PopupBorderSize] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        // style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        // style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        // style.PopupBorderSize = 0.00f;

        init_imgui();

        pkgi_input input;
        while (pkgi_update(&input))
        {
            ImGuiIO& io = ImGui::GetIO();
            io.DeltaTime = 1.0f / 60.0f;
            io.DisplaySize.x = VITA_WIDTH;
            io.DisplaySize.y = VITA_HEIGHT;

            if (gameview || pkgi_dialog_is_open())
            {
                io.AddKeyEvent(
                        ImGuiKey_GamepadDpadUp, input.pressed & PKGI_BUTTON_UP);
                io.AddKeyEvent(
                        ImGuiKey_GamepadDpadDown,
                        input.pressed & PKGI_BUTTON_DOWN);
                io.AddKeyEvent(
                        ImGuiKey_GamepadDpadLeft,
                        input.pressed & PKGI_BUTTON_LEFT);
                io.AddKeyEvent(
                        ImGuiKey_GamepadDpadRight,
                        input.pressed & PKGI_BUTTON_RIGHT);
                io.AddKeyEvent(
                        ImGuiKey_GamepadFaceDown,
                        input.pressed & pkgi_ok_button());
                if (input.pressed & pkgi_cancel_button() && gameview)
                    gameview->close();

                input.active = 0;
                input.pressed = 0;
            }

            if (need_refresh)
            {
                std::lock_guard<Mutex> lock(refresh_mutex);
                pkgi_refresh_installed_packages();
                if (!content_to_refresh.empty())
                {
                    const auto item =
                            db->get_by_content(content_to_refresh.c_str());
                    if (item)
                        item->presence = PresenceUnknown;
                    else
                        LOGF("couldn't find {} for refresh",
                             content_to_refresh);
                    content_to_refresh.clear();
                }
                if (gameview)
                    gameview->refresh();
                need_refresh = false;
            }

            ImGui::NewFrame();

            pkgi_draw_texture(background, 0, 0);



            // дата время

            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%d.%m.%Y  %H:%M");
            auto str = oss.str();

            pkgi_draw_text(VITA_WIDTH - 289, 0, PKGI_COLOR_DATE_TIME, fmt::format("{}", str).c_str());

            // аккумулятор

            // pkgi_text_width()

            pkgi_draw_text_with_size(
            VITA_WIDTH - 44 + 4 - pkgi_text_width(fmt::format("{}%", pkgi_bettery_get_level()).c_str()),
            -2,
            0.750f,
            PKGI_COLOR_TEXT,
            fmt::format("{}%", pkgi_bettery_get_level()).c_str());

            // pkgi_draw_text(
            // VITA_WIDTH - 74 - 26,
            // 0,
            // PKGI_COLOR_TEXT,
            // fmt::format("{}%", pkgi_bettery_get_level()).c_str());

            if (pkgi_battery_is_low()) 
            {
                pkgi_draw_texture(batteryislow, VITA_WIDTH - 47, 5);
            }
            else 
            {
                pkgi_draw_texture(batterynormal, VITA_WIDTH - 47, 5);
            }

            // иконка карты памяти
            // pkgi_draw_texture(notfill, VITA_WIDTH - 130, VITA_HEIGHT - 20);
            // pkgi_draw_texture(notfill, VITA_WIDTH - 130, VITA_HEIGHT - 20);
            // pkgi_draw_texture(notfill, VITA_WIDTH - 44 - 29 - pkgi_text_width(fmt::format("{}", pkgi_get_free_space(pkgi_get_mode_partition())).c_str()), VITA_HEIGHT - 20);
            // pkgi_draw_texture(notfill, VITA_WIDTH - pkgi_text_width(fmt::format("{}", pkgi_get_free_space("ux0:")).c_str()), VITA_HEIGHT - 20);




            // pkgi_text_width(fmt::format("{}%", pkgi_bettery_get_level()).c_str())

            //

            // разрядка
            pkgi_draw_rect(
            VITA_WIDTH - 43,
            8,
            28 - ceil((pkgi_bettery_get_level() * 28 / 100)),
            10,
            PKGI_COLOR_HEAD_HLINE);

            if (pkgi_battery_is_charging()) 
            {
                pkgi_draw_texture(batteryischarging, VITA_WIDTH - 47, 5);
            }

            pkgi_do_head(fontCyrillicMin);
            switch (state)
            {
            case StateError:
                pkgi_do_error();
                break;

            case StateRefreshing:
                pkgi_do_refresh();
                break;

            case StateMain:
                pkgi_do_main(
                        downloader,
                        pkgi_dialog_is_open() || pkgi_menu_is_open() ? NULL
                                                                     : &input, 
                        ps_ico);
                break;
            }

            pkgi_do_tail(downloader, notfill, btn_x, btn_square, btn_triangle, btn_circle);

            if (gameview)
            {
                bool mode_mode = mode == ModeGames;

                gameview->render(mode_mode);
                if (gameview->is_closed())
                    gameview = nullptr;
            }

            if (pkgi_dialog_is_open())
            {
                pkgi_do_dialog();
            }

            if (pkgi_dialog_input_update())
            {
                search_active = 1;
                pkgi_dialog_input_get_text(search_text, sizeof(search_text));
                configure_db(db.get(), search_text, &config);
                reposition();
            }

            if (pkgi_menu_is_open())
            {
                if (pkgi_do_menu(&input))
                {
                    Config new_config;
                    pkgi_menu_get(&new_config);
                    if (config_temp.sort != new_config.sort ||
                        config_temp.order != new_config.order ||
                        config_temp.filter != new_config.filter)
                    {
                        config_temp = new_config;
                        configure_db(
                                db.get(),
                                search_active ? search_text : NULL,
                                &config_temp);
                        reposition();
                    }
                }
                else
                {
                    MenuResult mres = pkgi_menu_result();
                    switch (mres)
                    {
                    case MenuResultSearch:
                        pkgi_dialog_input_text("Search", search_text);
                        break;
                    case MenuResultSearchClear:
                        search_active = 0;
                        search_text[0] = 0;
                        configure_db(db.get(), NULL, &config);
                        break;
                    case MenuResultCancel:
                        if (config_temp.sort != config.sort ||
                            config_temp.order != config.order ||
                            config_temp.filter != config.filter)
                        {
                            configure_db(
                                    db.get(),
                                    search_active ? search_text : NULL,
                                    &config);
                            reposition();
                        }
                        break;
                    case MenuResultAccept:
                        pkgi_menu_get(&config);
                        pkgi_save_config(config);
                        break;
                    case MenuResultRefresh:
                        pkgi_refresh_list();
                        break;
                    case MenuResultShowGames:
                        pkgi_set_mode(ModeGames);
                        break;
                    case MenuResultShowDlcs:
                        pkgi_set_mode(ModeDlcs);
                        break;
                    case MenuResultShowDemos:
                        pkgi_set_mode(ModeDemos);
                        break;
                    case MenuResultShowThemes:
                        pkgi_set_mode(ModeThemes);
                        break;
                    case MenuResultShowPsmGames:
                        pkgi_set_mode(ModePsmGames);
                        break;
                    case MenuResultShowPsxGames:
                        pkgi_set_mode(ModePsxGames);
                        break;
                    case MenuResultShowPspGames:
                        pkgi_set_mode(ModePspGames);
                        break;
                    case MenuResultShowPspDlcs:
                        pkgi_set_mode(ModePspDlcs);
                        break;
                    }
                }
            }

            ImGui::EndFrame();
            ImGui::Render();

            pkgi_imgui_render(ImGui::GetDrawData());

            pkgi_swap();
        }
    }
    catch (const std::exception& e)
    {
        LOGF("Error in main: {}", e.what());
        state = StateError;
        pkgi_snprintf(
                error_state, sizeof(error_state), "Фатальная ошибка: %s", e.what());

        pkgi_input input;
        while (pkgi_update(&input))
        {
            pkgi_draw_rect(0, 0, VITA_WIDTH, VITA_HEIGHT, 0);
            pkgi_do_error();
            pkgi_swap();
        }
        pkgi_end();
    }

    LOG("finished");
    pkgi_end();
}
