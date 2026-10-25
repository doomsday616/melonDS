<p align="center"><img src="../images/ko/banner.jpg" alt="melonDS i18n" width="100%"></p>

<p align="center">
<a href="../../README.md">English</a> ·
<a href="README.zh-CN.md">简体中文</a> ·
<a href="README.zh-TW.md">繁體中文</a> ·
<a href="README.ja.md">日本語</a> ·
<a href="README.de.md">Deutsch</a> ·
<a href="README.fr.md">Français</a> ·
<b>한국어</b> ·
<a href="README.it.md">Italiano</a>
</p>

<p align="center">
<a href="https://github.com/doomsday616/melonDS-i18n/releases/latest"><img src="https://img.shields.io/github/v/release/doomsday616/melonDS-i18n?label=release&color=5c913b" alt="Release"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-windows.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-windows.yml/badge.svg" alt="Windows"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-ubuntu.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-ubuntu.yml/badge.svg" alt="Linux"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-macos.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-macos.yml/badge.svg" alt="macOS"></a>
<a href="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-bsd.yml"><img src="https://github.com/doomsday616/melonDS-i18n/actions/workflows/build-bsd.yml/badge.svg" alt="BSD"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg" alt="GPLv3"></a>
</p>

# melonDS i18n

**melonDS i18n**은 닌텐도 DS 에뮬레이터 [melonDS](https://github.com/melonDS-emu/melonDS) 1.1을 기반으로 한 비공식 다국어 포크입니다. 에뮬레이션 코어는 원본과 완전히 동일하며, 바뀐 부분은 Qt 프런트엔드뿐입니다. 프런트엔드를 Qt Linguist 방식의 제대로 된 다국어 구조로 다시 정리해서, 메뉴와 모든 설정 창, 메시지, 그리고 게임 화면 위에 뜨는 알림까지 한국어로 표시됩니다.

## 주요 기능

<p align="center"><img src="../images/ko/features.jpg" alt="melonDS i18n 주요 기능" width="100%"></p>

- **8개 언어 완전 번역** – 언어마다 인터페이스 문자열 745개를 모두 번역했습니다.
- **시스템 언어 자동 감지** – 처음 실행하면 운영체제의 표시 언어를 따라갑니다.
- **즉시 전환** – 에뮬레이터를 다시 시작하지 않고 바로 언어를 바꿀 수 있습니다.
- **화면 알림(OSD) 번역** – 예를 들어 "슬롯 1에 상태 저장됨" 같은 알림도 게임 화면에 한국어로 나옵니다.
- **Qt 기본 요소도 번역** – 표준 버튼과 파일 선택 창도 선택한 언어를 따릅니다(`qtbase_*.qm` 내장).

## 스크린샷

<p align="center">
<img src="../images/ko/screenshot-main.jpg" alt="메인 창" width="32%">
<img src="../images/ko/screenshot-menu.jpg" alt="언어 메뉴" width="32%">
<img src="../images/ko/screenshot-settings.jpg" alt="설정 창" width="32%">
</p>
<p align="center"><sub>왼쪽부터: 메인 창 · "언어 / Language" 메뉴 · 한국어로 표시된 설정 창</sub></p>

## 지원 언어

| 언어 | 코드 | 번역 현황 |
| --- | --- | --- |
| English | `en` | 원문 |
| 简体中文 (중국어 간체) | `zh_CN` | 745/745 |
| 繁體中文 (중국어 번체, 대만 용어) | `zh_TW` | 745/745 |
| 日本語 (일본어) | `ja` | 745/745 |
| Deutsch (독일어) | `de` | 745/745 |
| Français (프랑스어) | `fr` | 745/745 |
| 한국어 | `ko` | 745/745 |
| Italiano (이탈리아어) | `it` | 745/745 |

<p align="center"><img src="../images/shared/languages.jpg" alt="지원 언어" width="100%"></p>

## 자동 감지와 언어 전환

<p align="center"><img src="../images/ko/switching.jpg" alt="언어 전환" width="100%"></p>

기본값은 **자동 (시스템 언어)** 입니다. 시스템 UI 언어에 따라 다음과 같이 선택됩니다.

- 중국어 번체 계열(zh-Hant, 대만·홍콩·마카오) → 繁體中文
- 그 밖의 중국어 → 简体中文
- 일본어 · 독일어 · 프랑스어 · 한국어 · 이탈리아어 → 해당 언어
- 그 외 모든 언어 → English

직접 고르고 싶다면 메뉴 막대의 **설정**과 **도움말** 사이에 있는 **언어 / Language** 메뉴를 여세요. 영어가 아닌 UI에서는 언어를 몰라도 찾을 수 있도록 제목에 항상 "Language"가 함께 붙습니다. 메뉴에는 **자동 (시스템 언어)** 와 각 언어가 원어 이름(English, 简体中文, 繁體中文, 日本語, Deutsch, Français, 한국어, Italiano)으로 나열됩니다. **설정 → 인터페이스 설정 → 언어:** 에서도 같은 설정을 바꿀 수 있으며, 두 곳은 항상 동기화됩니다.

언어는 그 자리에서 바로 바뀌고 재시작이 필요 없습니다. 다만 이미 열려 있던 설정 창은 한 번 닫았다가 다시 열어야 새 언어로 표시됩니다.

## 다운로드

<p align="center"><img src="../images/ko/download.jpg" alt="다운로드" width="100%"></p>

[최신 릴리스 페이지](https://github.com/doomsday616/melonDS-i18n/releases/latest)에서 내 플랫폼에 맞는 파일을 받으세요.

| 플랫폼 | 파일 |
| --- | --- |
| Windows (x86_64) | `melonDS-1.1-i18n-windows-x86_64.zip` |
| Windows (ARM64) | `melonDS-1.1-i18n-windows-arm64.zip` |
| Linux (x86_64) | `melonDS-1.1-i18n-linux-x86_64.AppImage` |
| Linux (aarch64) | `melonDS-1.1-i18n-linux-aarch64.AppImage` |
| macOS (Universal) | `melonDS-1.1-i18n-macos-universal.zip` |

Windows용 압축 파일에는 `melonDS.exe` 하나만 들어 있습니다. 설치가 필요 없는 포터블 방식이라 설정은 같은 폴더의 `melonDS.toml`에 저장됩니다.

## 빠른 시작

<p align="center"><img src="../images/ko/quickstart.jpg" alt="빠른 시작" width="100%"></p>

1. **다운로드 후 압축 해제** – 위 표에서 파일을 받아 원하는 폴더에 풀어 둡니다(Linux는 AppImage에 실행 권한만 주면 됩니다).
2. **BIOS/펌웨어 (선택)** – 내장 FreeBIOS 덕분에 직접 부팅(direct boot)이라면 따로 파일이 없어도 됩니다. 펌웨어 부팅이나 DSi 모드가 필요하면 **설정 → 에뮬레이터 설정** 에서 본인 기기에서 덤프한 파일을 지정하세요.
3. **ROM 실행** – **파일 → ROM 열기...** 를 선택해 본인이 가진 게임의 ROM 파일을 엽니다.

## 자주 묻는 질문

**macOS에서 "확인되지 않은 개발자" 경고가 떠요.**
릴리스는 서명되지 않은 빌드입니다. 앱을 마우스 오른쪽 버튼으로 클릭한 뒤 **열기**를 누르거나, **시스템 설정 → 개인정보 보호 및 보안**에서 **그래도 열기**를 선택하세요.

**Windows SmartScreen이 실행을 막아요.**
서명되지 않은 실행 파일이라 경고가 나올 수 있습니다. **추가 정보 → 실행**을 누르면 됩니다.

**Linux에서 화면 알림의 한글이 네모로 보여요.**
화면 알림은 시스템 글꼴로 그려지므로 CJK 글꼴이 설치되어 있어야 합니다. 배포판 패키지 관리자로 Noto Sans CJK KR 같은 글꼴을 설치하세요.

**BIOS나 펌웨어 파일이 꼭 필요한가요?**
직접 부팅에는 필요 없습니다. FreeBIOS가 내장되어 있습니다. 펌웨어로 부팅하려면 본인이 가진 DS 또는 DS Lite에서 덤프한 파일이 필요하며, DSi나 3DS의 DS 모드 펌웨어(128KB)로는 부팅할 수 없습니다. DSi 모드에는 DSi BIOS와 NAND 덤프가 필요합니다. 합법적인 입수 방법은 본인 기기에서 직접 덤프하는 것뿐이며, 이 프로젝트는 ROM이나 BIOS 다운로드 사이트를 안내하지 않습니다.

**언어를 바꿨는데 열려 있던 창이 그대로예요.**
이미 열려 있던 창은 닫았다가 다시 열면 새 언어로 표시됩니다.

**설정은 어디에 저장되나요?**
실행 파일과 같은 폴더의 `melonDS.toml`에 저장됩니다.

## 번역 참여하기

<p align="center"><img src="../images/shared/translation-workflow.jpg" alt="번역 작업 흐름" width="100%"></p>

1. 저장소를 포크하고 `src/frontend/qt_sdl/translations/melonDS_<언어>.ts` 를 수정합니다. Qt Linguist를 쓰면 편합니다.
2. 소스 문자열이 바뀌었다면 저장소 루트에서 `lupdate`로 `.ts`를 갱신합니다.
   ```
   lupdate -no-obsolete -locations none src/frontend/qt_sdl -ts src/frontend/qt_sdl/translations/melonDS_*.ts
   ```
3. `lrelease`로 `.qm`을 다시 만듭니다(`.qm`도 저장소에 커밋합니다).
   ```
   lrelease src/frontend/qt_sdl/translations/melonDS_*.ts
   ```
4. 새 언어를 추가할 때는 `qtbase_<언어>.qm`과 `translations.qrc` 항목도 함께 넣어 주세요.
5. 풀 리퀘스트를 보내 주세요. 오역이나 문제 제보는 [이 포크의 이슈](https://github.com/doomsday616/melonDS-i18n/issues)로 부탁드립니다. 업스트림 melonDS에는 올리지 말아 주세요.

## 크레딧 및 라이선스

- 에뮬레이터: melonDS 팀(Arisotura와 기여자들) – [melonDS-emu/melonDS](https://github.com/melonDS-emu/melonDS). 빌드 방법과 전체 크레딧은 해당 저장소에서 볼 수 있습니다.
- 아이콘: limittox
- 라이선스: [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0)

이 프로젝트는 비공식 포크이며 melonDS 팀이나 닌텐도와 관련이 없습니다.
