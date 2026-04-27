/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <string.h>

#include <optional>
#include <algorithm>
#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QFontMetrics>

#include <QDateTime>

#include "OpenGLSupport.h"
#include "duckstation/gl/context.h"

#include "main.h"
#include "EmuInstance.h"

#include "NDS.h"
#include "GPU.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"
#include "Platform.h"
#include "Config.h"

#include "main_shaders.h"
#include "OSD_shaders.h"
#include "font.h"
#include "version.h"

using namespace melonDS;

#if !defined(_WIN32) && !defined(APPLE)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
using namespace QNativeInterface;
#if defined(WAYLAND_ENABLED) && QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
#include <qpa/qplatformnativeinterface.h>
#endif
#else
#include <qpa/qplatformnativeinterface.h>
#endif
#endif


const u32 kOSDMargin = 6;
const int kLogoWidth = 192;


ScreenPanel::ScreenPanel(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents);

    QWidget* w = parent;
    for (;;)
    {
        mainWindow = qobject_cast<MainWindow*>(w);
        if (mainWindow) break;
        w = w->parentWidget();
        if (!w) break;
    }

    emuInstance = mainWindow->getEmuInstance();

    mouseHide = false;
    mouseHideDelay = 0;

    QTimer* mouseTimer = setupMouseTimer();
    connect(mouseTimer, &QTimer::timeout, [=] { if (mouseHide) setCursor(Qt::BlankCursor);});

    osdEnabled = false;
    osdID = 1;
    
    loadConfig();
    setFilter(mainWindow->getWindowConfig().GetBool("ScreenFilter"));

    splashLogo = QPixmap(":/melon-logo");

    strncpy(splashText[0].text, QCoreApplication::translate("OSD", "File->Open ROM...").toUtf8().constData(), 255);
    splashText[0].text[255] = '\0';
    splashText[0].id = 0x80000000;
    splashText[0].color = 0;
    splashText[0].rendered = false;
    splashText[0].rainbowstart = -1;

    strncpy(splashText[1].text, QCoreApplication::translate("OSD", "to get started").toUtf8().constData(), 255);
    splashText[1].text[255] = '\0';
    splashText[1].id = 0x80000001;
    splashText[1].color = 0;
    splashText[1].rendered = false;
    splashText[1].rainbowstart = -1;
    splashRetranslate = false;

    std::string url = MELONDS_URL;
    int urlpos = url.find("://");
    urlpos = (urlpos == std::string::npos) ? 0 : urlpos+3;
    strncpy(splashText[2].text, url.c_str() + urlpos, 256);
    splashText[2].id = 0x80000002;
    splashText[2].color = 0;
    splashText[2].rendered = false;
    splashText[2].rainbowstart = -1;
}

ScreenPanel::~ScreenPanel()
{
    mouseTimer->stop();
    delete mouseTimer;
}

void ScreenPanel::loadConfig()
{
    auto& cfg = mainWindow->getWindowConfig();
    
    screenRotation = cfg.GetInt("ScreenRotation");
    screenGap = cfg.GetInt("ScreenGap");
    screenLayout = cfg.GetInt("ScreenLayout");
    screenSwap = cfg.GetBool("ScreenSwap");
    screenSizing = cfg.GetInt("ScreenSizing");
    integerScaling = cfg.GetBool("IntegerScaling");
    screenAspectTop = cfg.GetInt("ScreenAspectTop");
    screenAspectBot = cfg.GetInt("ScreenAspectBot");
}

void ScreenPanel::setFilter(bool filter)
{
    this->filter = filter;
}

void ScreenPanel::setMouseHide(bool enable, int delay)
{
    mouseHide = enable;
    mouseHideDelay = delay;

    mouseTimer->setInterval(mouseHideDelay);
}

void ScreenPanel::setupScreenLayout()
{
    int w = width();
    int h = height();

    int sizing = screenSizing;
    if (sizing == screenSizing_Auto) sizing = autoScreenSizing;

    float aspectTop, aspectBot;

    for (auto ratio : aspectRatios)
    {
        if (ratio.id == screenAspectTop)
            aspectTop = ratio.ratio;
        if (ratio.id == screenAspectBot)
            aspectBot = ratio.ratio;
    }

    if (aspectTop == 0)
        aspectTop = ((float) w / h) / (4.f / 3.f);

    if (aspectBot == 0)
        aspectBot = ((float) w / h) / (4.f / 3.f);

    layout.Setup(w, h,
                static_cast<ScreenLayoutType>(screenLayout),
                static_cast<ScreenRotation>(screenRotation),
                static_cast<ScreenSizing>(sizing),
                screenGap,
                integerScaling != 0,
                screenSwap != 0,
                aspectTop,
                aspectBot);

    numScreens = layout.GetScreenTransforms(screenMatrix[0], screenKind);

    calcSplashLayout();
}

QSize ScreenPanel::screenGetMinSize(int factor = 1)
{
    bool isHori = (screenRotation == screenRot_90Deg
        || screenRotation == screenRot_270Deg);
    int gap = screenGap * factor;

    int w = 256 * factor;
    int h = 192 * factor;

    if (screenSizing == screenSizing_TopOnly
        || screenSizing == screenSizing_BotOnly)
    {
        return QSize(w, h);
    }

    if (screenLayout == screenLayout_Natural)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w, h+gap+h);
    }
    else if (screenLayout == screenLayout_Vertical)
    {
        if (isHori)
            return QSize(h, w+gap+w);
        else
            return QSize(w, h+gap+h);
    }
    else if (screenLayout == screenLayout_Horizontal)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w+gap+w, h);
    }
    else // hybrid
    {
        if (isHori)
            return QSize(h+gap+h, 3*w + (int)ceil((4*gap) / 3.0));
        else
            return QSize(3*w + (int)ceil((4*gap) / 3.0), h+gap+h);
    }
}

void ScreenPanel::onScreenLayoutChanged()
{
    loadConfig();

    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}

void ScreenPanel::onAutoScreenSizingChanged(int sizing)
{
    autoScreenSizing = sizing;
    if (screenSizing != screenSizing_Auto) return;

    setupScreenLayout();
}

void ScreenPanel::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();
    QWidget::resizeEvent(event);
}

void ScreenPanel::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    if (!emuInstance->emuIsActive()) { touching = false; return; }
    if (event->button() != Qt::LeftButton) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (layout.GetTouchCoords(x, y, false))
    {
        touching = true;
        emuInstance->touchScreen(x, y);
    }
}

void ScreenPanel::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
    if (!emuInstance->emuIsActive()) { touching = false; return; }
    if (event->button() != Qt::LeftButton) return;

    if (touching)
    {
        touching = false;
        emuInstance->releaseScreen();
    }
}

void ScreenPanel::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();

    showCursor();

    if (!emuInstance->emuIsActive()) return;
    //if (!(event->buttons() & Qt::LeftButton)) return;
    if (!touching) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (layout.GetTouchCoords(x, y, true))
    {
        emuInstance->touchScreen(x, y);
    }
}

void ScreenPanel::tabletEvent(QTabletEvent* event)
{
    event->accept();
    if (!emuInstance->emuIsActive()) { touching = false; return; }

    switch(event->type())
    {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
        {
#if QT_VERSION_MAJOR == 6
            int x = event->position().x();
            int y = event->position().y();
#else
            int x = event->x();
            int y = event->y();
#endif

            if (layout.GetTouchCoords(x, y, event->type()==QEvent::TabletMove))
            {
                touching = true;
                emuInstance->touchScreen(x, y);
            }
        }
        break;
    case QEvent::TabletRelease:
        if (touching)
        {
            emuInstance->releaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

void ScreenPanel::touchEvent(QTouchEvent* event)
{
#if QT_VERSION_MAJOR == 6
    if (event->device()->type() == QInputDevice::DeviceType::TouchPad)
        return;
#endif

    event->accept();
    if (!emuInstance->emuIsActive()) { touching = false; return; }

    switch(event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
#if QT_VERSION_MAJOR == 6
        if (event->points().length() > 0)
        {
            QPointF lastPosition = event->points().first().lastPosition();
#else
        if (event->touchPoints().length() > 0)
        {
            QPointF lastPosition = event->touchPoints().first().lastPos();
#endif
            int x = (int)lastPosition.x();
            int y = (int)lastPosition.y();

            if (layout.GetTouchCoords(x, y, event->type()==QEvent::TouchUpdate))
            {
                touching = true;
                emuInstance->touchScreen(x, y);
            }
        }
        break;
    case QEvent::TouchEnd:
        if (touching)
        {
            emuInstance->releaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

bool ScreenPanel::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchUpdate)
    {
        touchEvent((QTouchEvent*)event);
        return true;
    }
    else if (event->type() == QEvent::FocusIn)
        mainWindow->onFocusIn();
    else if (event->type() == QEvent::FocusOut)
        mainWindow->onFocusOut();
    else if (event->type() == QEvent::LanguageChange)
    {
        // The splash texts are re-rendered by osdUpdate(), which runs where
        // their textures may be (re)created.
        osdMutex.lock();
        splashRetranslate = true;
        osdMutex.unlock();
        update();
    }

    return QWidget::event(event);
}

void ScreenPanel::showCursor()
{
    mainWindow->panel->setCursor(Qt::ArrowCursor);
    mouseTimer->start();
}

QTimer* ScreenPanel::setupMouseTimer()
{
    mouseTimer = new QTimer();
    mouseTimer->setSingleShot(true);
    mouseTimer->setInterval(mouseHideDelay);
    mouseTimer->start();

    return mouseTimer;
}

// The built-in bitmap font only covers ASCII. Lowercase accented Latin
// letters (é, ü, à, ç...) are composed from the bitmap base letter plus a
// small bitmap mark so they match the rest of the text. Any other code point
// (CJK, uppercase accented letters, other symbols) is drawn with a system
// font into the same glyph line, then thresholded to solid pixels so it gets
// the same color, rainbow and outline treatment as the bitmap glyphs.

// Font families to try for non-ASCII OSD text, most suitable for the active
// UI language first; Qt's own font fallback covers anything else.
static QStringList osdFontFamiliesFor(const QString& lang)
{
    const QStringList sc = {"Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Hiragino Sans GB",
                            "Noto Sans CJK SC", "Source Han Sans SC", "WenQuanYi Micro Hei"};
    const QStringList tc = {"Microsoft JhengHei UI", "Microsoft JhengHei", "PingFang TC",
                            "Noto Sans CJK TC", "Source Han Sans TC"};
    const QStringList ja = {"Yu Gothic UI", "Meiryo UI", "Meiryo", "Hiragino Sans", "Hiragino Kaku Gothic ProN",
                            "Noto Sans CJK JP", "Source Han Sans JP"};
    const QStringList ko = {"Malgun Gothic", "Apple SD Gothic Neo", "Noto Sans CJK KR", "Source Han Sans KR"};
    const QStringList latin = {"Segoe UI", "Helvetica Neue", "Helvetica", "DejaVu Sans", "Noto Sans"};

    QStringList list;
    if (lang == "zh_TW")    list << tc << sc << ja << ko;
    else if (lang == "ja")  list << ja << sc << tc << ko;
    else if (lang == "ko")  list << ko << sc << tc << ja;
    else if (lang == "zh_CN") list << sc << tc << ja << ko;
    else                    list << latin << sc << tc << ja << ko;

    // for Latin text in CJK languages
    if (lang.startsWith("zh") || lang == "ja" || lang == "ko")
        list << latin;

    return list;
}

static QFont osdFallbackFont(const QStringList& families, bool bold)
{
    QFont font = QGuiApplication::font();
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    font.setFamilies(QStringList(families) << font.family());
#endif
    font.setPixelSize(12);
    // Latin/Greek/Cyrillic glyphs are drawn bold to match the 2-pixel
    // strokes of the bitmap font; CJK glyphs stay regular to remain legible.
    font.setBold(bold);
    // crisp, pixel-font-like glyphs (also hinted where the font supports it)
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

static bool osdIsCJK(uint ch)
{
    return ch >= 0x2E80;
}

// True if the text contains Han, Kana or Hangul. Such messages are drawn
// entirely (ASCII included) with the system font, so that Latin letters,
// digits and symbols match the size and weight of the CJK glyphs instead of
// mixing in the bitmap font.
static bool osdHasCJKScript(const uint* text)
{
    for (int i = 0; text[i] != '\0'; i++)
    {
        switch (QChar::script(text[i]))
        {
        case QChar::Script_Han:
        case QChar::Script_Hiragana:
        case QChar::Script_Katakana:
        case QChar::Script_Hangul:
            return true;
        default:
            break;
        }
    }
    return false;
}

// Small bitmap marks for composing accented lowercase letters. Bits are
// columns (bit 0 = leftmost), like the bitmap font rows.
struct OSDMark
{
    uint codepoint; // combining character
    int width;
    bool below;
    u16 rows[2];
};

static const OSDMark osdMarks[] =
{
    {0x0300, 3, false, {0x3, 0x6}},   // grave:      ##.  .##
    {0x0301, 3, false, {0x6, 0x3}},   // acute:      .##  ##.
    {0x0302, 4, false, {0x6, 0x9}},   // circumflex: .##. #..#
    {0x0303, 5, false, {0x16, 0xD}},  // tilde:      .##.# #.##.
    {0x0308, 5, false, {0x1B, 0x1B}}, // diaeresis:  ##.## ##.##
    {0x0327, 3, true,  {0x6, 0x3}},   // cedilla:    .##  ##.
};

// If ch is a lowercase ASCII letter with one supported mark, return the
// base letter and the mark.
static bool osdDecomposeLatin(uint ch, uint* base, const OSDMark** mark)
{
    if (ch < 0xC0 || ch > 0x24F)
        return false;

    const QString dec = QChar::decomposition(ch);
    if (dec.size() != 2)
        return false;

    const uint b = dec[0].unicode();
    const uint m = dec[1].unicode();
    if (b < 'a' || b > 'z')
        return false;

    for (const OSDMark& om : osdMarks)
    {
        if (om.codepoint == m)
        {
            *base = b;
            *mark = &om;
            return true;
        }
    }
    return false;
}

static QString osdCodePointString(uint ch)
{
    if (QChar::requiresSurrogates(ch))
    {
        const QChar pair[2] = {QChar(QChar::highSurrogate(ch)), QChar(QChar::lowSurrogate(ch))};
        return QString(pair, 2);
    }
    return QString(QChar(ch));
}

static int osdFallbackAdvance(const QFontMetrics& fm, uint ch)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return fm.horizontalAdvance(osdCodePointString(ch));
#else
    return fm.width(osdCodePointString(ch));
#endif
}

static int osdSpaceAdvance(const QFontMetrics& fm)
{
    int adv = osdFallbackAdvance(fm, ' ');
    return (adv > 0) ? adv : 4;
}

// Height of one text line. Bitmap-font text uses 14 pixels; system-font
// (CJK) messages get the full font height so Latin descenders (g, p, y)
// are not cut off.
static int osdLineHeight(const QFontMetrics& fm, bool sysText)
{
    return sysText ? std::max(14, fm.height()) : 14;
}

// Width taken by one character, including spacing. sysText: the whole
// message is drawn with the system font (see osdHasCJKScript()).
static int osdGlyphWidth(const QFontMetrics& fm, const QFontMetrics& fmLatin, uint ch, bool sysText)
{
    if (ch == ' ')
        return sysText ? osdSpaceAdvance(fm) : 6;

    if (sysText)
    {
        int adv = osdFallbackAdvance(fm, ch);
        return (adv > 0) ? (adv + 1) : 6;
    }

    uint base; const OSDMark* mark;
    if (osdDecomposeLatin(ch, &base, &mark))
    {
        int glyphsize = ::font[(base-0x10) << 4];
        return std::max(glyphsize, mark->width) + 2;
    }

    int glyphsize;
    if (ch < 0x80)
    {
        if (ch < 0x10 || ch > 0x7E) ch = 0x7F;

        glyphsize = ::font[(ch-0x10) << 4];
        if (!glyphsize) glyphsize = 6;
        else            glyphsize += 2; // space around the character
    }
    else
    {
        glyphsize = osdFallbackAdvance(osdIsCJK(ch) ? fm : fmLatin, ch);
        if (glyphsize <= 0) glyphsize = 6;
        else                glyphsize += 1;
    }

    return glyphsize;
}

int ScreenPanel::osdFindBreakPoint(const uint* text, int i)
{
    // i = character that went out of bounds

    for (int j = i; j >= 0; j--)
    {
        if (text[j] == ' ')
            return j;
    }

    return i;
}

void ScreenPanel::osdLayoutText(const uint* text, const QFontMetrics& fm, const QFontMetrics& fmLatin, bool sysText, int* width, int* height, int* breaks)
{
    int w = 0;
    const int lineh = osdLineHeight(fm, sysText);
    int h = lineh;
    int totalw = 0;
    int maxw = ((QWidget*)this)->width() - (kOSDMargin*2);
    int lastbreak = -1;
    int numbrk = 0;

    memset(breaks, 0, sizeof(int)*64);

    for (int i = 0; text[i] != '\0'; )
	{
	    int glyphsize = osdGlyphWidth(fm, fmLatin, text[i], sysText);

		w += glyphsize;
		if (w > maxw)
        {
            // wrap shit as needed
            if (text[i] == ' ')
            {
                if (numbrk >= 64) break;
                breaks[numbrk++] = i;
                i++;
            }
            else
            {
                int brk = osdFindBreakPoint(text, i);
                if (brk != lastbreak) i = brk;

                if (numbrk >= 64) break;
                breaks[numbrk++] = i;

                lastbreak = brk;
            }

            w = 0;
            h += lineh;
        }
        else
            i++;

        if (w > totalw) totalw = w;
    }

    *width = totalw;
    *height = h;
}

unsigned int ScreenPanel::osdRainbowColor(int inc)
{
    // inspired from Acmlmboard

    if      (inc < 100) return 0xFFFF9B9B + (inc << 8);
    else if (inc < 200) return 0xFFFFFF9B - ((inc-100) << 16);
    else if (inc < 300) return 0xFF9BFF9B + (inc-200);
    else if (inc < 400) return 0xFF9BFFFF - ((inc-300) << 8);
    else if (inc < 500) return 0xFF9B9BFF + ((inc-400) << 16);
    else                return 0xFFFF9BFF - (inc-500);
}

void ScreenPanel::osdRenderItem(OSDItem* item)
{
    int w, h;
    int breaks[64];

    // OSD text is UTF-8; work on code points (zero-terminated)
    auto textbuf = QString::fromUtf8(item->text).toUcs4();
    textbuf.append(0);
    const uint* text = textbuf.constData();
    u32 color = item->color;

    const QStringList families = osdFontFamiliesFor(currentUILanguage());
    const QFont fallbackFont = osdFallbackFont(families, false);
    const QFont latinFont = osdFallbackFont(families, true);
    const QFontMetrics fm(fallbackFont);
    const QFontMetrics fmLatin(latinFont);

    // messages with CJK text use the system font for every character
    const bool sysText = osdHasCJKScript(text);
    const int lineh = osdLineHeight(fm, sysText);

    bool rainbow = (color == 0);
    u32 rainbowinc;
    if (item->rainbowstart == -1)
    {
        u32 ticks = (u32) QDateTime::currentMSecsSinceEpoch();
        rainbowinc = ((text[0] * 17) + (ticks * 13)) % 600;
    }
    else
        rainbowinc = (u32)item->rainbowstart;

    color |= 0xFF000000;
    const u32 shadow = 0xE0000000;

    osdLayoutText(text, fm, fmLatin, sysText, &w, &h, breaks);

    item->bitmap = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    u32* bitmap = (u32*)item->bitmap.bits();
    memset(bitmap, 0, w*h*sizeof(u32));

    int x = 0, y = 1;
    u32 maxw = ((QWidget*)this)->width() - (kOSDMargin*2);
    int curline = 0;
    u16* ptr;
    uint base;
    const OSDMark* mark;

    for (int i = 0; text[i] != '\0'; )
	{
		if (text[i] == ' ')
		{
			x += sysText ? osdSpaceAdvance(fm) : 6;
		}
		else if (!sysText && osdDecomposeLatin(text[i], &base, &mark))
        {
            // accented lowercase letter: bitmap base letter plus a bitmap mark
            if (rainbow)
            {
                color = osdRainbowColor(rainbowinc);
                rainbowinc = (rainbowinc + 30) % 600;
            }

            ptr = &::font[(base-0x10) << 4];
            int glyphsize = ptr[0];
            int cellw = std::max(glyphsize, mark->width);
            int bx = x + 1 + (cellw - glyphsize) / 2;
            int mx = x + 1 + (cellw - mark->width) / 2;

            // the bitmap 'i' has its dot in rows 0-1: drop it under a mark
            int firstrow = (base == 'i') ? 2 : 0;
            for (int cy = firstrow; cy < 12; cy++)
            {
                u16 val = ptr[4+cy];
                for (int cx = 0; cx < glyphsize; cx++)
                {
                    if ((val & (1<<cx)) && (bx+cx) < w)
                        bitmap[((y+cy) * w) + bx+cx] = color;
                }
            }

            // marks above use rows -1..0 (one free row above the x-height;
            // on 'i' they replace the dot in rows 0..1), marks below use rows 10..11
            int my = mark->below ? (y + 10) : ((base == 'i') ? y : (y - 1));
            for (int cy = 0; cy < 2; cy++)
            {
                int py = my + cy;
                if (py < 0 || py >= h) continue;
                for (int cx = 0; cx < mark->width; cx++)
                {
                    if ((mark->rows[cy] & (1<<cx)) && (mx+cx) < w)
                        bitmap[(py * w) + mx+cx] = color;
                }
            }

            x += cellw + 2;
        }
		else if (sysText || text[i] >= 0x80)
        {
            // non-ASCII, or any character of a CJK message: rasterize with
            // the system font into the glyph cell. In a CJK message every
            // glyph uses the CJK font and the same centered baseline.
            const bool cjk = sysText || osdIsCJK(text[i]);
            const QFont& gfont = cjk ? fallbackFont : latinFont;
            const QFontMetrics& gfm = cjk ? fm : fmLatin;
            int adv = osdFallbackAdvance(gfm, text[i]);
            if (adv <= 0) x += 6;
            else
            {
                if (rainbow)
                {
                    color = osdRainbowColor(rainbowinc);
                    rainbowinc = (rainbowinc + 30) % 600;
                }

                // the glyph uses the full line (14 pixels: one row above and
                // below the 12-pixel bitmap glyph cell; taller for system-font
                // messages, see osdLineHeight())
                QImage glyph(adv, lineh, QImage::Format_ARGB32_Premultiplied);
                glyph.fill(0);
                {
                    QPainter gp(&glyph);
                    gp.setFont(gfont);
                    gp.setPen(Qt::white);
                    // CJK glyphs are centered in the line; other scripts sit
                    // on the bitmap font baseline (bottom of row 9 of the cell)
                    int baseline = cjk ? (lineh + gfm.ascent() - gfm.descent()) / 2 : 11;
                    gp.drawText(0, baseline, osdCodePointString(text[i]));
                }

                for (int cy = 0; cy < lineh; cy++)
                {
                    int py = y - 1 + cy;
                    if (py < 0 || py >= h) continue;

                    const u32* line = (const u32*)glyph.constScanLine(cy);
                    for (int cx = 0; cx < adv; cx++)
                    {
                        if ((x+cx) >= w) break;
                        if ((line[cx] >> 24) >= 0x80)
                            bitmap[(py * w) + x+cx] = color;
                    }
                }

                x += adv + 1;
            }
        }
		else
        {
            u32 ch = text[i];
            if (ch < 0x10 || ch > 0x7E) ch = 0x7F;

            ptr = &::font[(ch-0x10) << 4];
            int glyphsize = ptr[0];
            if (!glyphsize) x += 6;
            else
            {
                x++;

                if (rainbow)
                {
                    color = osdRainbowColor(rainbowinc);
                    rainbowinc = (rainbowinc + 30) % 600;
                }

                // draw character
                for (int cy = 0; cy < 12; cy++)
                {
                    u16 val = ptr[4+cy];

                    for (int cx = 0; cx < glyphsize; cx++)
                    {
                        if (val & (1<<cx))
                            bitmap[((y+cy) * w) + x+cx] = color;
                    }
                }

                x += glyphsize;
                x++;
            }
        }

		i++;
		if (breaks[curline] && i >= breaks[curline])
        {
            i = breaks[curline++];
            if (text[i] == ' ') i++;

            x = 0;
            y += lineh;
        }
    }

    // shadow
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            u32 val;

            val = bitmap[(y * w) + x];
            if ((val >> 24) == 0xFF) continue;

            if (x > 0)   val  = bitmap[(y * w) + x-1];
            if (x < w-1) val |= bitmap[(y * w) + x+1];
            if (y > 0)
            {
                if (x > 0)   val |= bitmap[((y-1) * w) + x-1];
                val |= bitmap[((y-1) * w) + x];
                if (x < w-1) val |= bitmap[((y-1) * w) + x+1];
            }
            if (y < h-1)
            {
                if (x > 0)   val |= bitmap[((y+1) * w) + x-1];
                val |= bitmap[((y+1) * w) + x];
                if (x < w-1) val |= bitmap[((y+1) * w) + x+1];
            }

            if ((val >> 24) == 0xFF)
                bitmap[(y * w) + x] = shadow;
        }
    }

    item->rainbowend = (int)rainbowinc;
}

void ScreenPanel::osdDeleteItem(OSDItem* item)
{
}

void ScreenPanel::osdSetEnabled(bool enabled)
{
    osdMutex.lock();
    osdEnabled = enabled;
    osdMutex.unlock();
}

void ScreenPanel::osdAddMessage(unsigned int color, const char* text)
{
    if (!osdEnabled) return;

    osdMutex.lock();

    OSDItem item;

    item.id = (osdID++) & 0x7FFFFFFF;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();
    strncpy(item.text, text, 255); item.text[255] = '\0';
    item.color = color;
    item.rendered = false;
    item.rainbowstart = -1;

    osdItems.push_back(item);

    osdMutex.unlock();
}

void ScreenPanel::osdUpdate()
{
    osdMutex.lock();

    qint64 tick_now = QDateTime::currentMSecsSinceEpoch();
    qint64 tick_min = tick_now - 2500;

    for (auto it = osdItems.begin(); it != osdItems.end(); )
    {
        OSDItem& item = *it;

        if ((!osdEnabled) || (item.timestamp < tick_min))
        {
            osdDeleteItem(&item);
            it = osdItems.erase(it);
            continue;
        }

        if (!item.rendered)
        {
            osdRenderItem(&item);
            item.rendered = true;
        }

        it++;
    }

    // render splashscreen text items if needed

    if (splashRetranslate)
    {
        splashRetranslate = false;

        const char* src[2] = {QT_TRANSLATE_NOOP("OSD", "File->Open ROM..."), QT_TRANSLATE_NOOP("OSD", "to get started")};
        for (int i = 0; i < 3; i++)
        {
            if (i < 2)
            {
                strncpy(splashText[i].text, QCoreApplication::translate("OSD", src[i]).toUtf8().constData(), 255);
                splashText[i].text[255] = '\0';
            }

            // the URL is not translated, but is re-rendered so the rainbow
            // colors stay continuous across the three splash lines
            if (splashText[i].rendered)
                osdDeleteItem(&splashText[i]);
            splashText[i].rendered = false;
            splashText[i].rainbowstart = -1;
        }
    }

    int rainbowinc = -1;
    bool needrecalc = false;

    for (int i = 0; i < 3; i++)
    {
        if (!splashText[i].rendered)
        {
            splashText[i].rainbowstart = rainbowinc;
            osdRenderItem(&splashText[i]);
            splashText[i].rendered = true;
            rainbowinc = splashText[i].rainbowend;
            needrecalc = true;
        }
    }

    osdMutex.unlock();

    if (needrecalc)
        calcSplashLayout();
}

void ScreenPanel::calcSplashLayout()
{
    if (!splashText[0].rendered)
        return;

    osdMutex.lock();

    int w = width();
    int h = height();

    int xlogo = (w - kLogoWidth) / 2;
    int ylogo = (h - kLogoWidth) / 2;

    // top text
    int totalwidth = splashText[0].bitmap.width() + 6 + splashText[1].bitmap.width();
    if (totalwidth >= w)
    {
        // stacked vertically
        splashPos[0].setX((width() - splashText[0].bitmap.width()) / 2);
        splashPos[1].setX((width() - splashText[1].bitmap.width()) / 2);

        int basey = ylogo / 2;
        splashPos[0].setY(basey - splashText[0].bitmap.height() - 1);
        splashPos[1].setY(basey + 1);
    }
    else
    {
        // horizontal
        splashPos[0].setX((w - totalwidth) / 2);
        splashPos[1].setX(splashPos[0].x() + splashText[0].bitmap.width() + 6);

        int basey = (ylogo - splashText[0].bitmap.height()) / 2;
        splashPos[0].setY(basey);
        splashPos[1].setY(basey);
    }

    // bottom text
    splashPos[2].setX((w - splashText[2].bitmap.width()) / 2);
    splashPos[2].setY(ylogo + kLogoWidth + ((ylogo - splashText[2].bitmap.height()) / 2));

    // logo
    splashPos[3].setX(xlogo);
    splashPos[3].setY(ylogo);

    osdMutex.unlock();
}



ScreenPanelNative::ScreenPanelNative(QWidget* parent) : ScreenPanel(parent)
{
    hasBuffers = false;

    screen[0] = QImage(256, 192, QImage::Format_RGB32);
    screen[1] = QImage(256, 192, QImage::Format_RGB32);

    screenTrans[0].reset();
    screenTrans[1].reset();
}

ScreenPanelNative::~ScreenPanelNative()
{
}

void ScreenPanelNative::setupScreenLayout()
{
    ScreenPanel::setupScreenLayout();

    for (int i = 0; i < numScreens; i++)
    {
        float* mtx = screenMatrix[i];
        screenTrans[i].setMatrix(mtx[0], mtx[1], 0.f,
                                 mtx[2], mtx[3], 0.f,
                                 mtx[4], mtx[5], 1.f);
    }
}

void ScreenPanelNative::drawScreen()
{
    auto emuThread = emuInstance->getEmuThread();
    if (!emuThread->emuIsActive())
    {
        hasBuffers = false;
        return;
    }

    auto nds = emuInstance->getNDS();
    assert(nds != nullptr);

    bufferLock.lock();
    hasBuffers = nds->GPU.GetFramebuffers(&topBuffer, &bottomBuffer);
    bufferLock.unlock();
}

void ScreenPanelNative::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    auto emuThread = emuInstance->getEmuThread();
    
    if (emuThread->emuIsActive())
    {
        emuInstance->renderLock.lock();

        bufferLock.lock();
        if (hasBuffers)
        {
            memcpy(screen[0].scanLine(0), topBuffer, 256 * 192 * 4);
            memcpy(screen[1].scanLine(0), bottomBuffer, 256 * 192 * 4);
        }
        bufferLock.unlock();

        QRect screenrc(0, 0, 256, 192);

        for (int i = 0; i < numScreens; i++)
        {
            painter.setTransform(screenTrans[i]);
            painter.drawImage(screenrc, screen[screenKind[i]]);
        }
        emuInstance->renderLock.unlock();
    }

    osdUpdate();

    if (!emuThread->emuIsActive())
    {
        // splashscreen
        osdMutex.lock();

        painter.drawPixmap(QRect(splashPos[3], QSize(kLogoWidth, kLogoWidth)), splashLogo);

        for (int i = 0; i < 3; i++)
            painter.drawImage(splashPos[i], splashText[i].bitmap);

        osdMutex.unlock();
    }

    if (osdEnabled)
    {
        osdMutex.lock();

        u32 y = kOSDMargin;

        painter.resetTransform();

        for (auto it = osdItems.begin(); it != osdItems.end(); )
        {
            OSDItem& item = *it;

            painter.drawImage(kOSDMargin, y, item.bitmap);

            y += item.bitmap.height();
            it++;
        }

        osdMutex.unlock();
    }
}



ScreenPanelGL::ScreenPanelGL(QWidget* parent) : ScreenPanel(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_KeyCompression, false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(screenGetMinSize());

    glInited = false;
}

ScreenPanelGL::~ScreenPanelGL()
{}

bool ScreenPanelGL::createContext()
{
    std::optional<WindowInfo> windowinfo = getWindowInfo();

    // if our parent window is parented to another window, we will
    // share our OpenGL context with that window
    MainWindow* ourwin = (MainWindow*)parentWidget();
    MainWindow* parentwin = (MainWindow*)parentWidget()->parentWidget();
    //if (parentwin)
    if (ourwin->getWindowID() != 0)
    {
        if (windowinfo.has_value())
            if ((glContext = parentwin->getOGLContext()->CreateSharedContext(*windowinfo)))
                glContext->DoneCurrent();
    }
    else
    {
        std::array<GL::Context::Version, 2> versionsToTry = {
                GL::Context::Version{GL::Context::Profile::Core, 4, 3},
                GL::Context::Version{GL::Context::Profile::Core, 3, 1}};
        if (windowinfo.has_value())
            if ((glContext = GL::Context::Create(*windowinfo, versionsToTry)))
                glContext->DoneCurrent();
    }

    return glContext != nullptr;
}

void ScreenPanelGL::setSwapInterval(int intv)
{
    if (!glContext) return;

    glContext->SetSwapInterval(intv);
}

void ScreenPanelGL::initOpenGL()
{
    if (!glContext) return;
    if (glInited) return;

    glContext->MakeCurrent();

    OpenGL::CompileVertexFragmentProgram(screenShaderProgram,
                                         kScreenVS, kScreenFS,
                                         "ScreenShader",
                                         {{"vPosition", 0}, {"vTexcoord", 1}},
                                         {{"oColor", 0}});

    glUseProgram(screenShaderProgram);
    glUniform1i(glGetUniformLocation(screenShaderProgram, "TopScreenTex"), 0);
    glUniform1i(glGetUniformLocation(screenShaderProgram, "BottomScreenTex"), 1);

    screenShaderScreenSizeULoc = glGetUniformLocation(screenShaderProgram, "uScreenSize");
    screenShaderTransformULoc = glGetUniformLocation(screenShaderProgram, "uTransform");

    const float vertices[] =
    {
        0.f,   0.f,    0.f, 0.f, 0.f,
        0.f,   192.f,  0.f, 1.f, 0.f,
        256.f, 192.f,  1.f, 1.f, 0.f,
        0.f,   0.f,    0.f, 0.f, 0.f,
        256.f, 192.f,  1.f, 1.f, 0.f,
        256.f, 0.f,    1.f, 0.f, 0.f,

        0.f,   0.f,    0.f, 0.f, 1.f,
        0.f,   192.f,  0.f, 1.f, 1.f,
        256.f, 192.f,  1.f, 1.f, 1.f,
        0.f,   0.f,    0.f, 0.f, 1.f,
        256.f, 192.f,  1.f, 1.f, 1.f,
        256.f, 0.f,    1.f, 0.f, 1.f
    };

    glGenBuffers(1, &screenVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &screenVertexArray);
    glBindVertexArray(screenVertexArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*4, (void*)(0));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5*4, (void*)(2*4));

    glGenTextures(1, &screenTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, screenTexture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 256, 192, 2, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);


    OpenGL::CompileVertexFragmentProgram(osdShader,
                                         kScreenVS_OSD, kScreenFS_OSD,
                                         "OSDShader",
                                         {{"vPosition", 0}},
                                         {{"oColor", 0}});

    glUseProgram(osdShader);
    glUniform1i(glGetUniformLocation(osdShader, "OSDTex"), 0);

    osdScreenSizeULoc = glGetUniformLocation(osdShader, "uScreenSize");
    osdPosULoc = glGetUniformLocation(osdShader, "uOSDPos");
    osdSizeULoc = glGetUniformLocation(osdShader, "uOSDSize");
    osdScaleFactorULoc = glGetUniformLocation(osdShader, "uScaleFactor");
    osdTexScaleULoc = glGetUniformLocation(osdShader, "uTexScale");

    const float osdvertices[6*2] =
    {
        0, 0,
        1, 1,
        1, 0,
        0, 0,
        0, 1,
        1, 1
    };

    glGenBuffers(1, &osdVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, osdVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(osdvertices), osdvertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &osdVertexArray);
    glBindVertexArray(osdVertexArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(0));

    // splash logo texture
    QImage logo = splashLogo.scaled(kLogoWidth*2, kLogoWidth*2).toImage();
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logo.width(), logo.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, logo.bits());
    logoTexture = tex;

    transferLayout();
    glInited = true;
}

void ScreenPanelGL::deinitOpenGL()
{
    if (!glContext) return;
    if (!glInited) return;

    glContext->MakeCurrent();

    glDeleteTextures(1, &screenTexture);

    glDeleteVertexArrays(1, &screenVertexArray);
    glDeleteBuffers(1, &screenVertexBuffer);

    glDeleteProgram(screenShaderProgram);


    for (const auto& [key, tex] : osdTextures)
    {
        glDeleteTextures(1, &tex);
    }
    osdTextures.clear();

    glDeleteVertexArrays(1, &osdVertexArray);
    glDeleteBuffers(1, &osdVertexBuffer);

    glDeleteTextures(1, &logoTexture);

    glDeleteProgram(osdShader);


    glContext->DoneCurrent();

    lastScreenWidth = lastScreenHeight = -1;
    glInited = false;
}

void ScreenPanelGL::makeCurrentGL()
{
    if (!glContext) return;

    glContext->MakeCurrent();
}

void ScreenPanelGL::releaseGL()
{
    if (!glContext) return;

    glContext->DoneCurrent();
}

void ScreenPanelGL::osdRenderItem(OSDItem* item)
{
    ScreenPanel::osdRenderItem(item);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item->bitmap.width(), item->bitmap.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, item->bitmap.bits());

    osdTextures[item->id] = tex;
}

void ScreenPanelGL::osdDeleteItem(OSDItem* item)
{
    if (osdTextures.count(item->id))
    {
        GLuint tex = osdTextures[item->id];
        glDeleteTextures(1, &tex);
        osdTextures.erase(item->id);
    }

    ScreenPanel::osdDeleteItem(item);
}

void ScreenPanelGL::drawScreen()
{
    if (!glContext) return;

    auto emuThread = emuInstance->getEmuThread();

    glContext->MakeCurrent();

    int w = windowInfo.surface_width;
    int h = windowInfo.surface_height;
    float factor = windowInfo.surface_scale;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    if (emuThread->emuIsActive())
    {
        auto nds = emuInstance->getNDS();

        glUseProgram(screenShaderProgram);
        glUniform2f(screenShaderScreenSizeULoc, w / factor, h / factor);

        void* topbuf; void* bottombuf;
        if (nds->GPU.GetFramebuffers(&topbuf, &bottombuf))
        {
            // if we're doing a regular render, use the provided framebuffers
            // otherwise, GetFramebuffers() will set up the required state

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, screenTexture);

            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 192, 1, GL_BGRA,
                            GL_UNSIGNED_BYTE, topbuf);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 256, 192, 1, GL_BGRA,
                            GL_UNSIGNED_BYTE, bottombuf);
        }
        else
        {
            GLuint texid = *(GLuint*)topbuf;

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texid);
        }

        screenSettingsLock.lock();

        GLint filter = this->filter ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);

        glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
        glBindVertexArray(screenVertexArray);

        for (int i = 0; i < numScreens; i++)
        {
            glUniformMatrix2x3fv(screenShaderTransformULoc, 1, GL_TRUE, screenMatrix[i]);
            glDrawArrays(GL_TRIANGLES, screenKind[i] == 0 ? 0 : 2 * 3, 2 * 3);
        }

        screenSettingsLock.unlock();
    }

    osdUpdate();

    if (!emuThread->emuIsActive())
    {
        // splashscreen
        osdMutex.lock();

        glUseProgram(osdShader);

        glUniform2f(osdScreenSizeULoc, w, h);
        glUniform1f(osdScaleFactorULoc, factor);
        glUniform1f(osdTexScaleULoc, 2.0);

        glBindBuffer(GL_ARRAY_BUFFER, osdVertexBuffer);
        glBindVertexArray(osdVertexArray);

        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glBindTexture(GL_TEXTURE_2D, logoTexture);
        glUniform2i(osdPosULoc, splashPos[3].x(), splashPos[3].y());
        glUniform2i(osdSizeULoc, kLogoWidth, kLogoWidth);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);

        glUniform1f(osdTexScaleULoc, 1.0);

        for (int i = 0; i < 3; i++)
        {
            OSDItem& item = splashText[i];

            if (!osdTextures.count(item.id))
                continue;

            glBindTexture(GL_TEXTURE_2D, osdTextures[item.id]);
            glUniform2i(osdPosULoc, splashPos[i].x(), splashPos[i].y());
            glUniform2i(osdSizeULoc, item.bitmap.width(), item.bitmap.height());
            glDrawArrays(GL_TRIANGLES, 0, 2*3);
        }

        glDisable(GL_BLEND);
        glUseProgram(0);

        osdMutex.unlock();
    }

    if (osdEnabled)
    {
        osdMutex.lock();

        u32 y = kOSDMargin;

        glUseProgram(osdShader);

        glUniform2f(osdScreenSizeULoc, w, h);
        glUniform1f(osdScaleFactorULoc, factor);
        glUniform1f(osdTexScaleULoc, 1.0);

        glBindBuffer(GL_ARRAY_BUFFER, osdVertexBuffer);
        glBindVertexArray(osdVertexArray);

        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        for (auto it = osdItems.begin(); it != osdItems.end(); )
        {
            OSDItem& item = *it;

            if (!osdTextures.count(item.id))
                continue;

            glBindTexture(GL_TEXTURE_2D, osdTextures[item.id]);
            glUniform2i(osdPosULoc, kOSDMargin, y);
            glUniform2i(osdSizeULoc, item.bitmap.width(), item.bitmap.height());
            glDrawArrays(GL_TRIANGLES, 0, 2*3);

            y += item.bitmap.height();
            it++;
        }

        glDisable(GL_BLEND);
        glUseProgram(0);

        osdMutex.unlock();
    }

    glContext->SwapBuffers();
}

qreal ScreenPanelGL::devicePixelRatioFromScreen() const
{
    const QScreen* screen_for_ratio = window()->windowHandle()->screen();
    if (!screen_for_ratio)
        screen_for_ratio = QGuiApplication::primaryScreen();

    return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
}

int ScreenPanelGL::scaledWindowWidth() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen())), 1);
}

int ScreenPanelGL::scaledWindowHeight() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen())), 1);
}

std::optional<WindowInfo> ScreenPanelGL::getWindowInfo()
{
    WindowInfo wi;

    // Windows and Apple are easy here since there's no display connection.
    #if defined(_WIN32)
    wi.type = WindowInfo::Type::Win32;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #elif defined(__APPLE__)
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #else
    const QString platform_name = QGuiApplication::platformName();

    #if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (platform_name == QStringLiteral("xcb"))
    {
        wi.type = WindowInfo::Type::X11;
        const QX11Application* x11 = qApp->nativeInterface<QX11Application>();
        wi.display_connection = x11->display();
        wi.window_handle = reinterpret_cast<void*>(winId());
    }
    #if defined(WAYLAND_ENABLED)
    else if (platform_name == QStringLiteral("wayland"))
    {
        wi.type = WindowInfo::Type::Wayland;
        const QWaylandApplication* wl = qApp->nativeInterface<QWaylandApplication>();
        wi.display_connection = wl->display();

        #if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QWindow* handle = windowHandle();
        if (handle == nullptr)
            return std::nullopt;

        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        wi.window_handle = pni->nativeResourceForWindow("surface", handle);
        #else
        wi.window_handle = reinterpret_cast<void*>(winId());
        #endif
    }
    #endif
    #else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    if (platform_name == QStringLiteral("xcb"))
    {
        wi.type = WindowInfo::Type::X11;
        wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
        wi.window_handle = reinterpret_cast<void*>(winId());
    }
    else if (platform_name == QStringLiteral("wayland"))
    {
        wi.type = WindowInfo::Type::Wayland;
        QWindow* handle = windowHandle();
        if (handle == nullptr)
            return std::nullopt;

        wi.display_connection = pni->nativeResourceForWindow("display", handle);
        wi.window_handle = pni->nativeResourceForWindow("surface", handle);
    }
    #endif
    else
    {
        Platform::Log(Platform::LogLevel::Error, "Unknown PNI platform %s\n", platform_name.toStdString().c_str());
        return std::nullopt;
    }
    #endif

    wi.surface_width = static_cast<u32>(scaledWindowWidth());
    wi.surface_height = static_cast<u32>(scaledWindowHeight());
    wi.surface_scale = static_cast<float>(devicePixelRatioFromScreen());

    return wi;
}


QPaintEngine* ScreenPanelGL::paintEngine() const
{
  return nullptr;
}

void ScreenPanelGL::setupScreenLayout()
{
    ScreenPanel::setupScreenLayout();
    transferLayout();
}

void ScreenPanelGL::transferLayout()
{
    std::optional<WindowInfo> windowInfo = getWindowInfo();
    if (windowInfo.has_value())
    {
        screenSettingsLock.lock();

        if (lastScreenWidth != windowInfo->surface_width || lastScreenHeight != windowInfo->surface_height)
        {
            if (glContext)
                glContext->ResizeSurface(windowInfo->surface_width, windowInfo->surface_height);
            lastScreenWidth = windowInfo->surface_width;
            lastScreenHeight = windowInfo->surface_height;
        }

        this->windowInfo = *windowInfo;

        screenSettingsLock.unlock();
    }
}
