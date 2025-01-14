#include "gameview.hpp"

#include <fmt/format.h>
#include "db.hpp"
#include "dialog.hpp"
#include "file.hpp"
#include "utils.hpp"
#include "imgui.hpp"
#include "db.hpp"
extern "C"
{
#include "style.h"
}

namespace
{
constexpr unsigned GameViewWidth = VITA_WIDTH * 0.8;
constexpr unsigned GameViewHeight = VITA_HEIGHT * 0.8;
}

GameView::GameView(
        const Config* config,
        Downloader* downloader,
        DbItem* item,
        std::optional<CompPackDatabase::Item> base_comppack,
        std::optional<CompPackDatabase::Item> patch_comppack)
    : _config(config)
    , _downloader(downloader)
    , _item(item)
    , _base_comppack(base_comppack)
    , _patch_comppack(patch_comppack)
    , _patch_info_fetcher(item->titleid)
    , _image_fetcher(item)
{
    refresh();
}

void GameView::render(bool mode)
{
    std::string title_game = mode ? erase_string_elements(_item->name) : _item->name;

    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH - GameViewWidth) / 2,
                   (VITA_HEIGHT - GameViewHeight) / 2));
    ImGui::SetNextWindowSize(ImVec2(GameViewWidth, GameViewHeight), 0);

    ImGui::PushStyleColor(ImGuiCol_Text, PKGI_COLOR_DIALOG_TEXT_WHITE);
    ImGui::Begin(
            fmt::format("{} ###gameview", title_game)
                    .c_str(),
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();

    ImGui::PushTextWrapPos(
            _image_fetcher.get_texture() == nullptr ? 0.f
                                                    : GameViewWidth - 300.f);
    ImGui::Text(fmt::format("Версия ПО консоли: {}", pkgi_get_system_version())
                        .c_str());
    ImGui::Text(
            fmt::format(
                    "Необходимая версия ПО: {}", get_min_system_version())
                    .c_str());

    ImGui::Text(" ");

    // fix версия игры без пробела 
    std::string pkgi_c_game_version = _game_version.c_str();
    std::string v_tmp;

    for(char c:pkgi_c_game_version) if (c != ' ') v_tmp += c;
    pkgi_c_game_version = v_tmp;
    // конец версия игры без пробела

    ImGui::Text(fmt::format(
                        "Установлена версия: {}",
                        _game_version.empty() ? "не установлена" : pkgi_c_game_version)
                        .c_str());
    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::Text("Версия совместимости: неизвестна");
    }
    else
    {
        ImGui::Text(fmt::format(
                            "Пакет для игры установлен: {}",
                            _comppack_versions.base.empty() ? "нет" : "да")
                            .c_str());
        ImGui::Text(fmt::format(
                            "Пакет для патча установлен: {}",
                            _comppack_versions.patch.empty()
                                    ? "нет"
                                    : _comppack_versions.patch)
                            .c_str());
    }

    ImGui::Text(" ");

    printDiagnostic();

    ImGui::Text(" ");

    ImGui::PopTextWrapPos();

    if (_patch_info_fetcher.get_status() == PatchInfoFetcher::Status::Found)
    {
        if (ImGui::Button("Установить игру и патч###installgame"))
            start_download_package();
    }
    else
    {
        if (ImGui::Button("Установить игру###installgame"))
            start_download_package();
    }
    ImGui::SetItemDefaultFocus();

    if (_base_comppack)
    {
        if (!_downloader->is_in_queue(CompPackBase, _item->titleid))
        {
            if (ImGui::Button("Установить базовый пакет  "
                              "совместимости###installbasecomppack"))
                start_download_comppack(false);
        }
        else
        {
            if (ImGui::Button("Отменить установку базового пакета "
                              "совместимости###installbasecomppack"))
                cancel_download_comppacks(false);
        }
    }
    if (_patch_comppack)
    {
        if (!_downloader->is_in_queue(CompPackPatch, _item->titleid))
        {
            if (ImGui::Button(fmt::format(
                                      "Установить пакет совместимости "
                                      "{}###installpatchcommppack",
                                      _patch_comppack->app_version)
                                      .c_str()))
                start_download_comppack(true);
        }
        else
        {
            if (ImGui::Button("Отменить установку пакета совместимости "
                              "исправлений###installpatchcommppack"))
                cancel_download_comppacks(true);
        }
    }

    auto tex = _image_fetcher.get_texture();

    // Display game image
    // обложка

    if (tex != nullptr)
    {
        int tex_w = vita2d_texture_get_width(tex);
        int tex_h = vita2d_texture_get_height(tex);
        float tex_x = ImGui::GetWindowContentRegionMax().x - tex_w - 10;
        float tex_y = ImGui::GetWindowContentRegionMin().y + 10;
        ImGui::SetCursorPos(ImVec2(tex_x, tex_y));
        ImGui::Image(tex, ImVec2(tex_w, tex_h));

    }

    ImGui::End();
}

static const auto Red = ImVec4(0.776f, 0.322f, 0.322f, 1.0f);
static const auto Yellow = ImVec4(0.98f, 0.867f, 0.0f, 1.0f);
static const auto Green = ImVec4(0.00f, 0.569f, 0.235f, 1.00f);

void GameView::printDiagnostic()
{
    bool ok = true;
    auto const printError = [&](auto const& str)
    {
        ok = false;
        ImGui::TextColored(Red, str);
    };

    auto const systemVersion = pkgi_get_system_version();
    auto const minSystemVersion = get_min_system_version();

    ImGui::Text("Информация:");

    if (systemVersion < minSystemVersion)
    {
        if (!_comppack_versions.present)
        {
            if (_refood_present)
                ImGui::Text("- Эта игра будет работать благодаря reF00D");
            else if (_0syscall6_present)
                ImGui::Text("- Эта игра будет работать благодаря 0syscall6");
            else
                printError(
                        "- Ваша прошивка слишком старая для этой игры, "
                        "вам необходимо установить reF00D или 0syscall6");
        }
    }
    else
    {
        ImGui::Text("- Версия прошивки подходит для запуска");
    }

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::TextColored(
                Yellow,
                "Установлен сторонний пакет в папку rePatch. Убедитесь, "
                "что данный пакет соответствует TITLE_ID игры. В противном "
                "случае, переустановите его");
        ok = false;
    }

    if (_comppack_versions.base.empty() && !_comppack_versions.patch.empty())
        printError(
                "- Вы установили пакет совместимости обновлений без "
                "установки базового пакета:, сначала установите базовый пакет, "
                "а затем переустановите пакет совместимости обновлений");

    std::string comppack_version;
    if (!_comppack_versions.patch.empty())
        comppack_version = _comppack_versions.patch;
    else if (!_comppack_versions.base.empty())
        comppack_version = _comppack_versions.base;

    if (_item->presence == PresenceInstalled && !comppack_version.empty() &&
        comppack_version < _game_version)
        printError(
                "- Версия игры не соответствует установленному "
                "пакету совместимости. Если вы обновили игру, также "
                "установите пакет совместимости обновлений");

    if (_item->presence == PresenceInstalled &&
        comppack_version > _game_version)
        printError(
                "- Версия игры не соответствует установленному пакету "
                "совместимости. Понизьте версию до базового пакета "
                "совместимости или обновите игру через Live Area");

    if (_item->presence != PresenceInstalled)
    {
        ImGui::Text("- Игра не установлена");
        ok = false;
    }

    if (ok)
        ImGui::TextColored(Green, "Игра установлена");
}

std::string GameView::get_min_system_version()
{
    auto const patchInfo = _patch_info_fetcher.get_patch_info();
    if (patchInfo)
        return patchInfo->fw_version;
    else
        return _item->fw_version;
}

void GameView::refresh()
{
    LOGF("refreshing gameview");
    _refood_present = pkgi_is_module_present("ref00d");
    _0syscall6_present = pkgi_is_module_present("0syscall6");
    _game_version = pkgi_get_game_version(_item->titleid);
    _comppack_versions = pkgi_get_comppack_versions(_item->titleid);
}


void GameView::do_download() {
    pkgi_start_download(*_downloader, *_item);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_package()
{
    std::string title_game = erase_string_elements(_item->name);

    if (_item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - already installed", _item->titleid, _item->name);

        pkgi_dialog_question(
        fmt::format(
                "{} уже установлено. "
                "Хотите переустановить?",
                title_game)
                .c_str(),
        {{"Да", [this] { this->do_download(); }},
         {"Нет", [] {} }});
        return;
    }
    this->do_download();
}

void GameView::cancel_download_package()
{
    _downloader->remove_from_queue(Game, _item->content);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_comppack(bool patch)
{
    const auto& entry = patch ? _patch_comppack : _base_comppack;

    _downloader->add(DownloadItem{
            patch ? CompPackPatch : CompPackBase,
            _item->name,
            _item->titleid,
            _config->comppack_url + entry->path,
            std::vector<uint8_t>{},
            std::vector<uint8_t>{},
            false,
            "ux0:",
            entry->app_version});
}

void GameView::cancel_download_comppacks(bool patch)
{
    _downloader->remove_from_queue(
            patch ? CompPackPatch : CompPackBase, _item->titleid);
}
