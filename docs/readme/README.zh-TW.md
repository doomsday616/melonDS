<p align="center"><img src="../images/zh-TW/banner.jpg" alt="melonDS i18n" width="100%"></p>

<p align="center">
<a href="../../README.md">English</a> ·
<a href="README.zh-CN.md">简体中文</a> ·
<b>繁體中文</b> ·
<a href="README.ja.md">日本語</a> ·
<a href="README.de.md">Deutsch</a> ·
<a href="README.fr.md">Français</a> ·
<a href="README.ko.md">한국어</a> ·
<a href="README.it.md">Italiano</a>
</p>

<p align="center">
<a href="https://github.com/doomsday616/melonDS-i18n/releases/latest"><img src="https://img.shields.io/github/v/release/doomsday616/melonDS-i18n?label=release&color=5c913b" alt="Release"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-windows.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-windows.yml/badge.svg" alt="Windows build"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-ubuntu.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-ubuntu.yml/badge.svg" alt="Linux build"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-macos.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-macos.yml/badge.svg" alt="macOS build"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-bsd.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-bsd.yml/badge.svg" alt="BSD build"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg" alt="License: GPLv3"></a>
</p>

# melonDS i18n

**melonDS i18n** 是任天堂 DS 模擬器 [melonDS](https://github.com/melonDS-emu/melonDS) 的非官方多語言分支。它以 melonDS 1.1 為基礎，模擬核心與上游完全相同，遊戲跑起來一模一樣；不同的是 Qt 前端：整個介面改成標準的 Qt Linguist 多語言（i18n）架構，選單、所有設定視窗、提示與錯誤訊息，甚至遊戲畫面上跳出的 OSD 通知，都會以你的語言顯示。目前提供 8 種語言，全部翻譯完成。

## 特色

<p align="center"><img src="../images/zh-TW/features.jpg" alt="特色：8 種語言、自動偵測、即時切換、OSD 提示翻譯" width="100%"></p>

- **8 種語言**：簡體中文、繁體中文（台灣用語）、英文、日文、德文、法文、韓文、義大利文，每種語言 745 條字串全部翻譯。
- **自動偵測**：第一次啟動就依照系統的介面語言選好語言。
- **即時切換**：在選單裡換語言立刻生效，不必重新啟動。
- **OSD 也在地化**：像「已將即時存檔儲存至欄位 1」這類畫面上的通知也有翻譯；Qt 內建的標準按鈕與檔案對話框也會跟著切換。

## 螢幕截圖

<p align="center"><img src="../images/zh-TW/screenshot-main.jpg" alt="主視窗" width="100%"><br><em>繁體中文介面的主視窗</em></p>

<p align="center"><img src="../images/zh-TW/screenshot-menu.jpg" alt="語言選單" width="100%"><br><em>「語言 / Language」選單，每種語言都以自己的名稱列出</em></p>

<p align="center"><img src="../images/zh-TW/screenshot-settings.jpg" alt="設定視窗" width="100%"><br><em>設定視窗同樣全部翻譯</em></p>

## 支援的語言

<p align="center"><img src="../images/shared/languages.jpg" alt="支援的語言" width="100%"></p>

| 語言 | 代碼 | 狀態 |
|---|---|---|
| 简体中文（簡體中文） | `zh_CN` | 745 / 745 |
| 繁體中文（台灣用語） | `zh_TW` | 745 / 745 |
| English（英文） | `en` | 原文 |
| 日本語（日文） | `ja` | 745 / 745 |
| Deutsch（德文） | `de` | 745 / 745 |
| Français（法文） | `fr` | 745 / 745 |
| 한국어（韓文） | `ko` | 745 / 745 |
| Italiano（義大利文） | `it` | 745 / 745 |

## 自動偵測與切換語言

<p align="center"><img src="../images/zh-TW/switching.jpg" alt="自動偵測或手動選擇語言" width="100%"></p>

預設情況下，melonDS i18n 會依照系統的介面語言自動選擇：

| 系統語言 | 介面語言 |
|---|---|
| 中文：繁體字或台灣、香港、澳門（`zh-Hant`、`zh-TW`、`zh-HK`、`zh-MO`） | 繁體中文 |
| 其他中文 | 简体中文 |
| 日文、德文、法文、韓文、義大利文 | 對應的語言 |
| 其他所有語言 | English |

想自己指定語言的話：

- 打開選單列上的「**語言 / Language**」選單（位在「設定」和「說明」之間）。非英文介面下，選單名稱會同時顯示本地名稱和 Language，所以就算切到看不懂的語言也找得回來。
- 選擇「**自動（依照系統語言）**」或任何一種語言。語言一律用自己的名稱列出：English、简体中文、繁體中文、日本語、Deutsch、Français、한국어、Italiano。
- 同一個選項也在「**設定 → 介面設定 → 語言**」裡，兩邊會保持同步。

切換會立即生效，選單和畫面上的通知當場就換成新語言。已經開著的對話視窗，關掉再重新開啟後才會套用新語言。

## 下載

<p align="center"><img src="../images/zh-TW/download.jpg" alt="選擇你的平台下載" width="100%"></p>

請到 [Releases 頁面](https://github.com/doomsday616/melonDS-i18n/releases/latest)下載最新版本：

| 平台 | 檔案 |
|---|---|
| Windows（x86_64，大多數電腦） | `melonDS-1.1-i18n-windows-x86_64.zip` |
| Windows（ARM64） | `melonDS-1.1-i18n-windows-arm64.zip` |
| Linux（x86_64） | `melonDS-1.1-i18n-linux-x86_64.AppImage` |
| Linux（aarch64） | `melonDS-1.1-i18n-linux-aarch64.AppImage` |
| macOS（Intel 與 Apple 晶片） | `melonDS-1.1-i18n-macos-universal.zip` |

Windows 的 zip 裡只有一個 `melonDS.exe`。它是免安裝的可攜版，不會寫入系統，設定檔就存在執行檔旁邊。

## 快速上手

<p align="center"><img src="../images/zh-TW/quickstart.jpg" alt="三步驟開始遊玩" width="100%"></p>

1. **下載並解壓縮**對應平台的版本（Linux 請先把 AppImage 設成可執行再開啟）。
2. **BIOS／韌體（選用）**：內建的 FreeBIOS 讓大部分遊戲不需要任何傾印檔就能直接啟動。如果想透過 DS 韌體選單開機，或使用 DSi 模式，請到「**設定 → 模擬器設定**」指定你自己傾印的檔案。
3. **載入遊戲**：用「**檔案 → 開啟 ROM...**」選擇你從自己擁有的卡匣傾印出來的 ROM。

## 常見問題

**macOS 說這個 App 來自未識別的開發者。**
這些版本沒有經過簽署。請在 App 上按右鍵選「**打開**」，或到「**系統設定 → 隱私權與安全性**」按「**強制打開**」。

**Windows SmartScreen 跳出警告。**
原因相同：執行檔沒有簽署。按「**其他資訊**」，再按「**仍要執行**」。

**Linux 上，畫面通知裡的中文、日文、韓文變成方框。**
OSD 是用系統字型繪製文字的。請安裝 CJK 字型，例如 Noto Sans CJK（Debian／Ubuntu 上的套件是 `fonts-noto-cjk`），然後重新啟動 melonDS。

**BIOS 和韌體是什麼？要去哪裡取得？**
它們是真實主機內部的開機程式和設定資料。melonDS 內建 FreeBIOS，所以直接啟動遊戲時並不需要它們。若要透過韌體選單開機，需要從原版 DS 或 DS Lite 傾印的檔案（DSi 或 3DS 在 DS 模式下的 128 KB 韌體無法用來開機）。DSi 模式則需要 DSi 的 BIOS 和 NAND 傾印檔。唯一合法的來源是從你自己擁有的主機傾印，本專案不會提供任何下載網站的連結。

**切換語言後，開著的視窗還是舊語言。**
這是正常的。把那個視窗關掉再重新開啟即可。

**設定存在哪裡？**
Windows 上存在 `melonDS.exe` 旁邊的 `melonDS.toml`。Linux 和 macOS 上存在使用者設定目錄下的 `melonDS` 資料夾（例如 `~/.config/melonDS/`）；若執行檔旁邊有 `portable` 資料夾，則改存在那裡。

## 參與翻譯

<p align="center"><img src="../images/shared/translation-workflow.jpg" alt="翻譯流程" width="100%"></p>

翻譯檔放在 [`src/frontend/qt_sdl/translations/`](../../src/frontend/qt_sdl/translations/)，是名為 `melonDS_<lang>.ts` 的 Qt Linguist 檔案。編譯好的 `.qm`、Qt 自己的 `qtbase_<lang>.qm` 以及 `translations.qrc` 也一起放在那裡，會內嵌進執行檔。

1. 如果原文字串有變動，先更新 `.ts` 檔：
   `lupdate -no-obsolete -locations none src/frontend/qt_sdl -ts src/frontend/qt_sdl/translations/melonDS_*.ts`
2. 用 Qt Linguist 翻譯（或直接編輯 `.ts` 的 XML）。
3. 編譯翻譯檔：
   `lrelease src/frontend/qt_sdl/translations/melonDS_*.ts`
4. 重新建置 melonDS，`.qm` 會透過 `translations.qrc` 內嵌進去。

完整指令也寫在 `src/frontend/qt_sdl/CMakeLists.txt` 的註解裡。翻譯問題請回報到[本專案的 Issues](https://github.com/doomsday616/melonDS-i18n/issues)，不要回報給上游 melonDS。

## 致謝與授權

- melonDS 由 melonDS 團隊（Arisotura 與各位貢獻者）開發。上游專案：[melonDS-emu/melonDS](https://github.com/melonDS-emu/melonDS)。
- melonDS 圖示由 limittox 設計。
- 建置說明與完整的上游致謝名單請見 [melonDS 官方儲存庫](https://github.com/melonDS-emu/melonDS)。

melonDS i18n 是自由軟體，與 melonDS 相同，以 [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0) 授權釋出。
