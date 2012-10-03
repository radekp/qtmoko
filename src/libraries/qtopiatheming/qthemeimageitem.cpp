/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
**
** Contact: Qt Extended Information (info@qtextended.org)
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#include <QThemeImageItem>
#include "qthemeimageitem_p.h"
#include <QThemedScene>
#include <QThemedView>
#include <QStringList>
#include <QGraphicsScene>
#include <QPainter>
#include <QXmlStreamReader>
#include <QDebug>
#include <QFileInfo>
#include <QPicture>
#include <QMap>
#include <QPixmapCache>
#include "qthemeitem_p.h"
#include <QThemeItemAttribute>
#include <stdlib.h>
#include <qtopianamespace.h>
#ifdef THEME_EDITOR
#include "themeresource.h"
#endif

QThemeImageItemPrivate::QThemeImageItemPrivate()
        : tile("false"), scale("no"), alpha(255), color(0)
{
    replaceColor.setDefaultValue(false);
    forceReload = false;
}

QThemeImageItemPrivate::~QThemeImageItemPrivate()
{
}

/***************************************************************************/

/*!
  \class QThemeImageItem
    \inpublicgroup QtBaseModule
  \brief The QThemeImageItem class provides a image item that you can add to a QThemedView to display an image.
  \since 4.4

  \ingroup qtopiatheming
  \sa QThemeItem, QThemedView
*/

/*!
  Constructs a QThemeImageItem.
  The \a parent is passed to the base class constructor.
*/
QThemeImageItem::QThemeImageItem(QThemeItem *parent)
        : QThemeItem(parent), d(new QThemeImageItemPrivate)
{
    QThemeItem::registerType(Type);
}

/*!
  Destroys the QThemeImageItem.
*/
QThemeImageItem::~QThemeImageItem()
{
    delete d;
}

/*!
  \reimp
*/
int QThemeImageItem::type() const
{
    return Type;
}

/*!
  \internal
*/
QThemeItemAttribute *QThemeImageItem::source()
{
    return &(d->source);
}

/*!
  \reimp
*/
void QThemeImageItem::loadAttributes(QXmlStreamReader &reader)
{
    QThemeItem::loadAttributes(reader);

#ifdef THEME_EDITOR
    //Theme Editor does much reloading of attributes to set items
    delete d;
    d = new QThemeImageItemPrivate;
#endif

    if (!reader.attributes().value("src").isEmpty())
        d->source.setValue(reader.attributes().value("src").toString());
    if (!reader.attributes().value("tile").isEmpty())
        d->tile.setValue(reader.attributes().value("tile").toString() == "yes");
    if (!reader.attributes().value("replacecolor").isEmpty())
        d->replaceColor.setValue(reader.attributes().value("replacecolor").toString() == "yes");
    if (!reader.attributes().value("scale").isEmpty())
        d->scale.setValue(reader.attributes().value("scale").toString());
    if (!reader.attributes().value("alpha").isEmpty())
        d->alpha.setValue(reader.attributes().value("alpha").toString().toInt());
    if (!reader.attributes().value("color").isEmpty())
        d->color.setValue(reader.attributes().value("color").toString());
    if (QThemeItem::d->onClickAtts.contains("src")) {
        d->source.setValue(QThemeItem::d->onClickAtts["src"], "onclick");
    }
}
#ifdef THEME_EDITOR

/*!
  \reimp
*/
void QThemeImageItem::saveAttributes(QXmlStreamWriter &writer)
{
    QThemeItem::saveAttributes(writer);

    if (d->source.isModified())
        writer.writeAttribute("src", d->source.value().toString());
    if (d->alpha.isModified())
        writer.writeAttribute("alpha", d->alpha.value().toString());
    if (d->color.isModified() && !d->color.value().toString().isEmpty())
        writer.writeAttribute("color", d->color.value().toString());
    if (d->tile.isModified())
        writer.writeAttribute("tile", d->tile.value().toBool() ? "yes" : "no");
    if (d->replaceColor.isModified())
        writer.writeAttribute("replacecolor", d->replaceColor.value().toBool() ? "yes" : "no");
    if (d->scale.isModified())
        writer.writeAttribute("scale", d->scale.value().toString());
}
#endif

/*!
  \reimp
*/
void QThemeImageItem::constructionComplete()
{
#ifdef THEME_EDITOR
    //Want to avoid another resize event on the device
    //Theme editor can't guarantee one though(and is needed to init)
    reloadBuffer();
#endif
    QThemeItem::constructionComplete();
}

/*!
  \internal
*/
QString QThemeImageItem::findImageFile(const QString &s)
{
    QString path = ":image/themes/";
    const QString defaultDir = "default/";
    const QString translationDir = "i18n/";
    QString filename = s;

    if (filename.endsWith(".svg") || filename.endsWith(".png")) {
        filename.chop(4);
    }
    if (filename.startsWith(defaultDir)) {
        filename.remove(0, 8);
        path.append(defaultDir);
    } else {
        QFileInfo fi;
        QString themeName = themedScene()->themedView()->themePrefix().toLower() + '/';
        fi.setFile(path + themeName + filename);
        if (fi.exists() || filename.startsWith(translationDir))
            path.append(themeName);
        else
            path.append(defaultDir);
    }
    if (filename.startsWith(translationDir)) {
        filename.remove(0, 5);

#if !defined(THEME_EDITOR)
        static QStringList langs;
        if (langs.empty()) {
            langs = Qtopia::languageList();
            langs.append(QLatin1String("en_US"));
        }
        QFileInfo fi;
        QString file;
        foreach(QString l, langs) {
            file = path + translationDir + l + '/' + filename;
            fi.setFile(file);
            if (fi.exists()) {
                return file;
            }
        }
#endif
    }
    return path + filename;
}

/*!
    \internal
    Replaces pixels with color \a before with color \a after in the \a image.
*/
void QThemeImageItem::replaceColor(QImage &image, const QColor &before, const QColor &after)
{
    QRgb b = before.rgb();
    QRgb a = after.rgb();

    for (int j = 0; j < image.height(); j++) {
        for (int i = 0; i < image.width(); i++) {
            if (image.pixel(i, j) == b)
                image.setPixel(QPoint(i, j), a);
        }
    }
}

/*!
  \internal
*/
void QThemeImageItem::resizeImages(qreal width, qreal height)
{
    if (!d->forceReload && QSizeF(width, height) == d->imagesSize)
        return;

    foreach(QString state, d->source.states()) {
        QFileInfo fileInfo;

#if defined(THEME_EDITOR)
        fileInfo.setFile(ThemeResource::filePath(d->source.value(state).toString()));
#else
        fileInfo.setFile(findImageFile(d->source.value(state).toString()));
#endif
        QString key("qtv_%1_%2_%3_%4_%5");
        if (d->replaceColor.value() == true)
            key = key.arg(fileInfo.fileName()).arg(width).arg(height).arg(scene()->palette().color(QPalette::Highlight).name()).arg(d->alpha.value().toString());
        else
            key = key.arg(fileInfo.fileName()).arg(width).arg(height).arg(d->color.value().toString()).arg(d->alpha.value().toString());

        QPixmap pm;
        if (fileInfo.suffix() == "pic") {
            if (QPixmapCache::find(key, pm)) {
                d->buffer[state] = pm;
            } else {
                QPicture picture;
                picture.load(fileInfo.filePath());
                QImage img((int)width, (int)height, QImage::Format_ARGB32_Premultiplied);
                img.fill(0);
                QPainter painter(&img);
                QRect br = picture.boundingRect();
                painter.setViewport(0, 0, (int)width, (int)height);
                painter.scale(width / br.width(), height / br.height());
                painter.drawPicture(br.topLeft(), picture);
                painter.end();

                if (d->replaceColor.value(state).toBool())
                    replaceColor(img, QColor(255, 0, 255), scene()->palette().color(QPalette::Highlight));
                if (d->color.isModified() || d->alpha.isModified())
                    colorizeImage(img, colorFromString(d->color.value(state).toString()), d->alpha.value(state).toInt(), true);
                d->buffer[state] = QPixmap::fromImage(img);
                QPixmapCache::insert(key, d->buffer[state]);
            }
        } else {
            if (!d->forceReload && QPixmapCache::find(key, pm)) {
                d->buffer[state] = pm;
            } else {
                QImage img(fileInfo.filePath());
#ifdef THEME_EDITOR
                if(img.isNull())
                    d->source.setValue("",state);//So it's known we have no image
#endif
                if (d->replaceColor.value(state).toBool())
                    replaceColor(img, QColor(255, 0, 255), scene()->palette().color(QPalette::Highlight));
                if (d->forceReload || d->color.isModified() || d->alpha.isModified())
                    colorizeImage(img, colorFromString(d->color.value(state).toString()), d->alpha.value(state).toInt(), true);
                d->buffer[state] = QPixmap::fromImage(img);
                QPixmapCache::insert(key, d->buffer[state]);
            }
        }
    }
    d->imagesSize = QSizeF(width, height);
}

/*!
  \internal
*/
static void yuv_to_rgb(int Y, int U, int V, int& R, int& G, int&B)
{
    R = int(Y + ((92242 * (V - 128)) >> 16));
    G = int(Y - (((22643 * (U - 128)) >> 16)) - ((46983 * (V - 128)) >> 16));
    B = int(Y + ((116589 * (U - 128)) >> 16));
}

/*!
  \internal
*/
static void rgb_to_yuv(int R, int G, int B, int& Y, int& U, int& V)
{
    Y = int(R *  19595 + G *  38470 + B *  7471) >> 16;
    U = int(R * -11076 + G * -21758 + (B << 15) + 8388608) >> 16;
    V = int(R *  32768 + G * -27460 + B * -5328 + 8388608) >> 16;
}

/*!
  \internal
*/
static QRgb blendYuv(QRgb rgb, int /*sr*/, int /*sg*/, int /*sb*/, int sy, int su, int sv, int alpha, bool colorroles)
{
    int a = (rgb >> 24) & 0xff;
    int r = (rgb >> 16) & 0xff;
    int g = (rgb >> 8) & 0xff;
    int b = rgb & 0xff;
    if (colorroles) {
        int y, u, v;
        rgb_to_yuv(r, g, b, y, u, v);
        y = (y * 2 + sy) / 3;
        u = (u + su * 2) / 3;
        v = (v + sv * 2) / 3;
        yuv_to_rgb(y, u, v, r, g, b);
        if (r > 255) r = 255;
        if (r < 0) r = 0;
        if (g > 255) g = 255;
        if (g < 0) g = 0;
        if (b > 255) b = 255;
        if (b < 0) b = 0;
    }
    if (alpha != 255)
        a = (a * alpha) / 255;
    return qRgba(r, g, b, a);
}

/*!
  Blend the color \a col and the alpha value \a alpha with the image \a img if \a blendColor is true (the default).
  If \a blendColor is false, only the alpha value \a alpha is blended.
  This function modifies \a img directly.
*/
void QThemeImageItem::colorizeImage(QImage &img, const QColor &col, int alpha, bool blendColor)
{
    if (img.isNull())
        return;

    QColor colour;
    if (!col.isValid()) {
        blendColor = false;
        colour = Qt::white;
    } else {
        colour = col;
    }
    int count;
    int sr, sg, sb;
    colour.getRgb(&sr, &sg, &sb);
    int sy, su, sv;
    rgb_to_yuv(sr, sg, sb, sy, su, sv);

    if (alpha != 255 &&
            img.format() != QImage::Format_ARGB32 &&
            img.format() != QImage::Format_ARGB32_Premultiplied) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    if (img.depth() == 32) {
        const QImage &cimg = img; // avoid QImage::detach().
        QRgb *rgb = (QRgb*)cimg.bits();
        count = img.bytesPerLine() / sizeof(QRgb) * img.height();
        for (int i = 0; i < count; i++, rgb++)
            *rgb = blendYuv(*rgb, sr, sg, sb, sy, su, sv, alpha,
                            blendColor);
    } else {
        QVector<QRgb> ctable = img.colorTable();
        for (int i = 0; i < ctable.count(); i++)
            ctable[i] = blendYuv(ctable[i], sr, sg, sb, sy, su, sv, alpha,
                                 blendColor);
        img.setColorTable(ctable);
    }
}

/*!
  Returns the image for the state \a state.
*/
QPixmap QThemeImageItem::image(const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
#ifdef THEME_EDITOR
    reloadBuffer();
#endif
    return d->buffer[s];
}

/*!
  Sets the image \a path for the state \a state.
*/
void QThemeImageItem::setImage(const QString &path, const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    d->source.setValue(path, s);
    QRectF br = boundingRect();
    resize(br.width(), br.height());
}

/*!
  Sets the image \a pixmap for the state \a state.
*/
void QThemeImageItem::setImage(const QPixmap &pixmap, const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    d->buffer[s] = pixmap;
}

/*!
  Returns the alpha value for the state \a state.
*/
int QThemeImageItem::alpha(const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    return d->alpha.value(s).toInt();
}

/*!
  Returns the \a alpha value for the state \a state.
*/
void QThemeImageItem::setAlpha(int alpha, const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    d->alpha.setValue(alpha, s);
}

/*!
  Returns the color for the state \a state.
*/
QColor QThemeImageItem::color(const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    return colorFromString(d->color.value(s).toString());
}

/*!
  Sets the color \a col for the state \a state.
*/
void QThemeImageItem::setColor(const QString &col, const QString &state)
{
    QString s = state;
    if (s.isEmpty())
        s = "default";
    d->color.setValue(col, s);
}

/*!
  \internal
*/
QStringList QThemeImageItem::states()
{
    QStringList list = d->source.states();
    if (!list.contains(QString("default")))
        list << QString("default");
    return list;
}

/*!
  \reimp
*/
void QThemeImageItem::paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w)
{
    resizeImages(boundingRect().width(), boundingRect().height());
    QString _state = "default";
    if (state() == "onclick" && QThemeItem::d->onClickAtts.contains("src"))
        _state = "onclick";

    if(d->buffer[_state].isNull())
        return;

    if (d->scale.value().toString() == "vertical")
        d->buffer[_state] = d->buffer[_state].scaled(d->buffer[_state].width(), qRound(boundingRect().height()));
    else if (d->scale.value().toString() == "horizontal")
        d->buffer[_state] = d->buffer[_state].scaled(qRound(boundingRect().width()), d->buffer[_state].height());
    else if (d->scale.value().toString() == "yes")
        d->buffer[_state] = d->buffer[_state].scaled(qRound(boundingRect().width()), qRound(boundingRect().height()));

    p->setClipRect(boundingRect());
    p->setClipping(true);
    if (d->tile.value().toBool() == true) {
        p->drawTiledPixmap(boundingRect(), d->buffer[_state]);
    } else {
        int width = d->buffer[_state].width();
        int height = d->buffer[_state].height();
        QRect r;
        r.setX(qRound((boundingRect().width() / 2) - (width / 2)));
        r.setY(qRound((boundingRect().height() / 2) - (height / 2)));
        r.setWidth(width);
        r.setHeight(height);
        p->drawPixmap(r, d->buffer[_state]);
    }
    QThemeItem::paint(p, o, w);
}

#ifdef THEME_EDITOR

QWidget *QThemeImageItem::editWidget()
{
    return new QThemeImageItemEditor(this, QThemeItem::editWidget());
}

void QThemeImageItem::reloadBuffer()
{
    QRectF br = boundingRect();
    d->forceReload = true;
    resizeImages(br.width(), br.height());
    d->forceReload = false;
}
#endif
